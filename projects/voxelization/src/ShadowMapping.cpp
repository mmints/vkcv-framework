#include "ShadowMapping.hpp"
#include <vkcv/shader/GLSLCompiler.hpp>

const vk::Format            shadowMapFormat         = vk::Format::eR16G16B16A16Unorm;
const vk::Format            shadowMapDepthFormat    = vk::Format::eD32Sfloat;
const uint32_t              shadowMapResolution     = 2048;
const vkcv::Multisampling   msaa                    = vkcv::Multisampling::MSAA8X;

vkcv::ShaderProgram loadShadowShader() {
	vkcv::ShaderProgram shader;
	vkcv::shader::GLSLCompiler compiler;
	compiler.compile(vkcv::ShaderStage::VERTEX, "resources/shaders/shadow.vert",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shader.addShader(shaderStage, path);
	});
	compiler.compile(vkcv::ShaderStage::FRAGMENT, "resources/shaders/shadow.frag",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shader.addShader(shaderStage, path);
	});
	return shader;
}

vkcv::ShaderProgram loadDepthToMomentsShader() {
	vkcv::ShaderProgram shader;
	vkcv::shader::GLSLCompiler compiler;
	compiler.compile(vkcv::ShaderStage::COMPUTE, "resources/shaders/depthToMoments.comp",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shader.addShader(shaderStage, path);
	});
	return shader;
}

vkcv::ShaderProgram loadShadowBlurXShader() {
	vkcv::ShaderProgram shader;
	vkcv::shader::GLSLCompiler compiler;
	compiler.compile(vkcv::ShaderStage::COMPUTE, "resources/shaders/shadowBlurX.comp",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shader.addShader(shaderStage, path);
	});
	return shader;
}

vkcv::ShaderProgram loadShadowBlurYShader() {
	vkcv::ShaderProgram shader;
	vkcv::shader::GLSLCompiler compiler;
	compiler.compile(vkcv::ShaderStage::COMPUTE, "resources/shaders/shadowBlurY.comp",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shader.addShader(shaderStage, path);
	});
	return shader;
}

glm::mat4 computeShadowViewProjectionMatrix(
	const glm::vec3&            lightDirection, 
	const vkcv::camera::Camera& camera, 
	float                       maxShadowDistance,
	const glm::vec3&            voxelVolumeOffset,
	float                       voxelVolumeExtent) {

	const glm::vec3 cameraPos   = camera.getPosition();
	const glm::vec3 forward     = glm::normalize(camera.getFront());
	glm::vec3 up                = glm::normalize(camera.getUp());
	const glm::vec3 right       = glm::normalize(glm::cross(forward, up));
	up = glm::cross(right, forward);

	const float fov         = camera.getFov();
	const float aspectRatio = camera.getRatio();

	float near;
	float far;
	camera.getNearFar(near, far);
	far = std::min(maxShadowDistance, far);

	const glm::vec3 nearCenter  = cameraPos + forward * near;
	const float nearUp          = near * tan(fov * 0.5);
	const float nearRight       = nearUp * aspectRatio;
	
	const glm::vec3 farCenter   = cameraPos + forward * far;
	const float farUp           = far * tan(fov * 0.5);
	const float farRight        = farUp * aspectRatio;

	std::array<glm::vec3, 8> viewFrustumCorners = {
		nearCenter + right * nearRight + nearUp * up,
		nearCenter + right * nearRight - nearUp * up,
		nearCenter - right * nearRight + nearUp * up,
		nearCenter - right * nearRight - nearUp * up,

		farCenter + right * farRight + farUp * up,
		farCenter + right * farRight - farUp * up,
		farCenter - right * farRight + farUp * up,
		farCenter - right * farRight - farUp * up
	};

	std::array<glm::vec3, 8> voxelVolumeCorners = {
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(1, 1, 1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(1, 1, -1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(1, -1, 1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(1, -1, -1),

		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(-1, 1, 1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(-1, 1, -1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(-1, -1, 1),
		voxelVolumeOffset + voxelVolumeExtent * glm::vec3(-1, -1, -1),
	};

	glm::vec3 minView(std::numeric_limits<float>::max());
	glm::vec3 maxView(std::numeric_limits<float>::lowest());

	const glm::mat4 view = glm::lookAt(glm::vec3(0), -lightDirection, glm::vec3(0, -1, 0));

	auto getMinMaxView = [&](std::array<glm::vec3, 8> points) {
		for (const glm::vec3& p : points) {
			const auto& pView = glm::vec3(view * glm::vec4(p, 1));
			minView = glm::min(minView, pView);
			maxView = glm::max(maxView, pView);
		}
	};

	getMinMaxView(viewFrustumCorners);
	getMinMaxView(voxelVolumeCorners);

	// rotationaly invariant to avoid shadow  swimming when moving camera
	// could potentially be wasteful, but guarantees stability, regardless of camera and voxel volume
	 glm::vec3 scale = glm::vec3(1.f / glm::max(far, voxelVolumeExtent));

	glm::vec3 offset = -0.5f * (maxView + minView) * scale;

	// snap to texel to avoid shadow swimming when moving
	glm::vec2 offset2D = glm::vec2(offset);
	glm::vec2 frustumExtent2D = glm::vec2(1) / glm::vec2(scale);
	glm::vec2 texelSize = glm::vec2(frustumExtent2D / static_cast<float>(shadowMapResolution));
	offset2D = glm::ceil(offset2D / texelSize) * texelSize;
	offset.x = offset2D.x;
	offset.y = offset2D.y;

	glm::mat4 crop(1);
	crop[0][0] = scale.x;
	crop[1][1] = scale.y;
	crop[2][2] = scale.z;

	crop[3][0] = offset.x;
	crop[3][1] = offset.y;
	crop[3][2] = offset.z;

	glm::mat4 vulkanCorrectionMatrix(1.f);
	vulkanCorrectionMatrix[2][2] = 0.5;
	vulkanCorrectionMatrix[3][2] = 0.5;

	return vulkanCorrectionMatrix * crop * view;
}

ShadowMapping::ShadowMapping(vkcv::Core* corePtr, const vkcv::VertexLayout& vertexLayout) : 
	m_corePtr(corePtr),
	m_shadowMap(corePtr->createImage(shadowMapFormat, shadowMapResolution, shadowMapResolution, 1, true, true)),
	m_shadowMapIntermediate(corePtr->createImage(shadowMapFormat, shadowMapResolution, shadowMapResolution, 1, false, true)),
	m_shadowMapDepth(corePtr->createImage(shadowMapDepthFormat, shadowMapResolution, shadowMapResolution, 1, false, false, false, msaa)),
	m_lightInfoBuffer(corePtr->createBuffer<LightInfo>(vkcv::BufferType::UNIFORM, sizeof(glm::vec3))){

	vkcv::ShaderProgram shadowShader = loadShadowShader();

	// pass
	const std::vector<vkcv::AttachmentDescription> shadowAttachments = {
		vkcv::AttachmentDescription(vkcv::AttachmentOperation::STORE, vkcv::AttachmentOperation::CLEAR, shadowMapDepthFormat)
	};
	vkcv::PassConfig shadowPassConfig(shadowAttachments, msaa);
	m_shadowMapPass = corePtr->createPass(shadowPassConfig);

	// pipeline
	vkcv::PipelineConfig shadowPipeConfig{
		shadowShader,
		shadowMapResolution,
		shadowMapResolution,
		m_shadowMapPass,
		vertexLayout,
		{},
		false
	};
	shadowPipeConfig.m_multisampling        = msaa;
	shadowPipeConfig.m_EnableDepthClamping  = true;
	shadowPipeConfig.m_culling              = vkcv::CullMode::Front;
	m_shadowMapPipe                         = corePtr->createGraphicsPipeline(shadowPipeConfig);

	m_shadowSampler = corePtr->createSampler(
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerMipmapMode::LINEAR,
		vkcv::SamplerAddressMode::CLAMP_TO_EDGE
	);

	// depth to moments
	vkcv::ShaderProgram depthToMomentsShader    = loadDepthToMomentsShader();
	m_depthToMomentsDescriptorSet               = corePtr->createDescriptorSet(depthToMomentsShader.getReflectedDescriptors()[0]);
	m_depthToMomentsPipe                        = corePtr->createComputePipeline(depthToMomentsShader, { corePtr->getDescriptorSet(m_depthToMomentsDescriptorSet).layout });

	vkcv::DescriptorWrites depthToMomentDescriptorWrites;
	depthToMomentDescriptorWrites.sampledImageWrites    = { vkcv::SampledImageDescriptorWrite(0, m_shadowMapDepth.getHandle()) };
	depthToMomentDescriptorWrites.samplerWrites         = { vkcv::SamplerDescriptorWrite(1, m_shadowSampler) };
	depthToMomentDescriptorWrites.storageImageWrites    = { vkcv::StorageImageDescriptorWrite(2, m_shadowMap.getHandle()) };
	corePtr->writeDescriptorSet(m_depthToMomentsDescriptorSet, depthToMomentDescriptorWrites);

	// shadow blur X
	vkcv::ShaderProgram shadowBlurXShader    = loadShadowBlurXShader();
	m_shadowBlurXDescriptorSet              = corePtr->createDescriptorSet(shadowBlurXShader.getReflectedDescriptors()[0]);
	m_shadowBlurXPipe                       = corePtr->createComputePipeline(shadowBlurXShader, { corePtr->getDescriptorSet(m_shadowBlurXDescriptorSet).layout });

	vkcv::DescriptorWrites shadowBlurXDescriptorWrites;
	shadowBlurXDescriptorWrites.sampledImageWrites   = { vkcv::SampledImageDescriptorWrite(0, m_shadowMap.getHandle()) };
	shadowBlurXDescriptorWrites.samplerWrites        = { vkcv::SamplerDescriptorWrite(1, m_shadowSampler) };
	shadowBlurXDescriptorWrites.storageImageWrites   = { vkcv::StorageImageDescriptorWrite(2, m_shadowMapIntermediate.getHandle()) };
	corePtr->writeDescriptorSet(m_shadowBlurXDescriptorSet, shadowBlurXDescriptorWrites);

	// shadow blur Y
	vkcv::ShaderProgram shadowBlurYShader = loadShadowBlurYShader();
	m_shadowBlurYDescriptorSet = corePtr->createDescriptorSet(shadowBlurYShader.getReflectedDescriptors()[0]);
	m_shadowBlurYPipe = corePtr->createComputePipeline(shadowBlurYShader, { corePtr->getDescriptorSet(m_shadowBlurYDescriptorSet).layout });

	vkcv::DescriptorWrites shadowBlurYDescriptorWrites;
	shadowBlurYDescriptorWrites.sampledImageWrites  = { vkcv::SampledImageDescriptorWrite(0, m_shadowMapIntermediate.getHandle()) };
	shadowBlurYDescriptorWrites.samplerWrites       = { vkcv::SamplerDescriptorWrite(1, m_shadowSampler) };
	shadowBlurYDescriptorWrites.storageImageWrites  = { vkcv::StorageImageDescriptorWrite(2, m_shadowMap.getHandle()) };
	corePtr->writeDescriptorSet(m_shadowBlurYDescriptorSet, shadowBlurYDescriptorWrites);
}

void ShadowMapping::recordShadowMapRendering(
	const vkcv::CommandStreamHandle&    cmdStream,
	const glm::vec2&                    lightAngleRadian,
	const glm::vec3&                    lightColor,
	float                               lightStrength,
	float                               maxShadowDistance,
	const std::vector<vkcv::Mesh>&      meshes,
	const std::vector<glm::mat4>&       modelMatrices,
	const vkcv::camera::Camera&         camera,
	const glm::vec3&                    voxelVolumeOffset,
	float                               voxelVolumeExtent) {

	LightInfo lightInfo;
	lightInfo.sunColor = lightColor;
	lightInfo.sunStrength = lightStrength;
	lightInfo.direction = glm::normalize(glm::vec3(
		std::cos(lightAngleRadian.x) * std::cos(lightAngleRadian.y),
		std::sin(lightAngleRadian.x),
		std::cos(lightAngleRadian.x) * std::sin(lightAngleRadian.y)));

	lightInfo.lightMatrix = computeShadowViewProjectionMatrix(
		lightInfo.direction,
		camera,
		maxShadowDistance,
		voxelVolumeOffset,
		voxelVolumeExtent);
	m_lightInfoBuffer.fill({ lightInfo });

	std::vector<glm::mat4> mvpLight;
	for (const auto& m : modelMatrices) {
		mvpLight.push_back(lightInfo.lightMatrix * m);
	}
	const vkcv::PushConstantData shadowPushConstantData((void*)mvpLight.data(), sizeof(glm::mat4));

	std::vector<vkcv::DrawcallInfo> drawcalls;
	for (const auto& mesh : meshes) {
		drawcalls.push_back(vkcv::DrawcallInfo(mesh, {}));
	}

	m_corePtr->recordDrawcallsToCmdStream(
		cmdStream,
		m_shadowMapPass,
		m_shadowMapPipe,
		shadowPushConstantData,
		drawcalls,
		{ m_shadowMapDepth.getHandle() });
	m_corePtr->prepareImageForSampling(cmdStream, m_shadowMapDepth.getHandle());

	// depth to moments
	uint32_t dispatchCount[3];
	dispatchCount[0] = static_cast<uint32_t>(std::ceil(shadowMapResolution / 8.f));
	dispatchCount[1] = static_cast<uint32_t>(std::ceil(shadowMapResolution / 8.f));
	dispatchCount[2] = 1;

	const uint32_t msaaSampleCount = msaaToSampleCount(msaa);

	m_corePtr->prepareImageForStorage(cmdStream, m_shadowMap.getHandle());
	m_corePtr->recordComputeDispatchToCmdStream(
		cmdStream,
		m_depthToMomentsPipe,
		dispatchCount,
		{ vkcv::DescriptorSetUsage(0, m_corePtr->getDescriptorSet(m_depthToMomentsDescriptorSet).vulkanHandle) },
		vkcv::PushConstantData((void*)&msaaSampleCount, sizeof(msaaSampleCount)));
	m_corePtr->prepareImageForSampling(cmdStream, m_shadowMap.getHandle());

	// blur X
	m_corePtr->prepareImageForStorage(cmdStream, m_shadowMapIntermediate.getHandle());
	m_corePtr->recordComputeDispatchToCmdStream(
		cmdStream,
		m_shadowBlurXPipe,
		dispatchCount,
		{ vkcv::DescriptorSetUsage(0, m_corePtr->getDescriptorSet(m_shadowBlurXDescriptorSet).vulkanHandle) },
		vkcv::PushConstantData(nullptr, 0));
	m_corePtr->prepareImageForSampling(cmdStream, m_shadowMapIntermediate.getHandle());

	// blur Y
	m_corePtr->prepareImageForStorage(cmdStream, m_shadowMap.getHandle());
	m_corePtr->recordComputeDispatchToCmdStream(
		cmdStream,
		m_shadowBlurYPipe,
		dispatchCount,
		{ vkcv::DescriptorSetUsage(0, m_corePtr->getDescriptorSet(m_shadowBlurYDescriptorSet).vulkanHandle) },
		vkcv::PushConstantData(nullptr, 0));
	m_shadowMap.recordMipChainGeneration(cmdStream);
	m_corePtr->prepareImageForSampling(cmdStream, m_shadowMap.getHandle());
}

vkcv::ImageHandle ShadowMapping::getShadowMap() {
	return m_shadowMap.getHandle();
}

vkcv::SamplerHandle ShadowMapping::getShadowSampler() {
	return m_shadowSampler;
}

vkcv::BufferHandle ShadowMapping::getLightInfoBuffer() {
	return m_lightInfoBuffer.getHandle();
}