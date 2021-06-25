#include <iostream>
#include <vkcv/Core.hpp>
#include <GLFW/glfw3.h>
#include <vkcv/camera/CameraManager.hpp>
#include <chrono>
#include <vkcv/asset/asset_loader.hpp>
#include <vkcv/shader/GLSLCompiler.hpp>
#include <vkcv/Logger.hpp>
#include "Voxelization.hpp"
#include <glm/glm.hpp>
#include "vkcv/gui/GUI.hpp"

int main(int argc, const char** argv) {
	const char* applicationName = "Voxelization";

	uint32_t windowWidth = 1280;
	uint32_t windowHeight = 720;
	
	vkcv::Window window = vkcv::Window::create(
		applicationName,
		windowWidth,
		windowHeight,
		true
	);

    vkcv::camera::CameraManager cameraManager(window);
    uint32_t camIndex = cameraManager.addCamera(vkcv::camera::ControllerType::PILOT);
    uint32_t camIndex2 = cameraManager.addCamera(vkcv::camera::ControllerType::TRACKBALL);
    
    cameraManager.getCamera(camIndex).setPosition(glm::vec3(0.f, 0.f, 3.f));
    cameraManager.getCamera(camIndex).setNearFar(0.1f, 30.0f);
	cameraManager.getCamera(camIndex).setYaw(180.0f);
	
	cameraManager.getCamera(camIndex2).setNearFar(0.1f, 30.0f);

	vkcv::Core core = vkcv::Core::create(
		window,
		applicationName,
		VK_MAKE_VERSION(0, 0, 1),
		{ vk::QueueFlagBits::eTransfer,vk::QueueFlagBits::eGraphics, vk::QueueFlagBits::eCompute },
		{},
		{ "VK_KHR_swapchain" }
	);

	vkcv::asset::Scene mesh;

	const char* path = argc > 1 ? argv[1] : "resources/Sponza/Sponza.gltf";
	vkcv::asset::Scene scene;
	int result = vkcv::asset::loadScene(path, scene);

	if (result == 1) {
		std::cout << "Scene loading successful!" << std::endl;
	}
	else {
		std::cout << "Scene loading failed: " << result << std::endl;
		return 1;
	}

	// build index and vertex buffers
	assert(!scene.vertexGroups.empty());
	std::vector<std::vector<uint8_t>> vBuffers;
	std::vector<std::vector<uint8_t>> iBuffers;

	std::vector<vkcv::VertexBufferBinding> vBufferBindings;
	std::vector<std::vector<vkcv::VertexBufferBinding>> vertexBufferBindings;
	std::vector<vkcv::asset::VertexAttribute> vAttributes;

	for (int i = 0; i < scene.vertexGroups.size(); i++) {

		vBuffers.push_back(scene.vertexGroups[i].vertexBuffer.data);
		iBuffers.push_back(scene.vertexGroups[i].indexBuffer.data);

		auto& attributes = scene.vertexGroups[i].vertexBuffer.attributes;

		std::sort(attributes.begin(), attributes.end(), [](const vkcv::asset::VertexAttribute& x, const vkcv::asset::VertexAttribute& y) {
			return static_cast<uint32_t>(x.type) < static_cast<uint32_t>(y.type);
		});
	}

	std::vector<vkcv::Buffer<uint8_t>> vertexBuffers;
	for (const vkcv::asset::VertexGroup& group : scene.vertexGroups) {
		vertexBuffers.push_back(core.createBuffer<uint8_t>(
			vkcv::BufferType::VERTEX,
			group.vertexBuffer.data.size()));
		vertexBuffers.back().fill(group.vertexBuffer.data);
	}

	std::vector<vkcv::Buffer<uint8_t>> indexBuffers;
	for (const auto& dataBuffer : iBuffers) {
		indexBuffers.push_back(core.createBuffer<uint8_t>(
			vkcv::BufferType::INDEX,
			dataBuffer.size()));
		indexBuffers.back().fill(dataBuffer);
	}

	int vertexBufferIndex = 0;
	for (const auto& vertexGroup : scene.vertexGroups) {
		for (const auto& attribute : vertexGroup.vertexBuffer.attributes) {
			vAttributes.push_back(attribute);
			vBufferBindings.push_back(vkcv::VertexBufferBinding(attribute.offset, vertexBuffers[vertexBufferIndex].getVulkanHandle()));
		}
		vertexBufferBindings.push_back(vBufferBindings);
		vBufferBindings.clear();
		vertexBufferIndex++;
	}

	const vk::Format colorBufferFormat = vk::Format::eB10G11R11UfloatPack32;
	const vkcv::AttachmentDescription color_attachment(
		vkcv::AttachmentOperation::STORE,
		vkcv::AttachmentOperation::CLEAR,
		colorBufferFormat
	);
	
	const vk::Format depthBufferFormat = vk::Format::eD32Sfloat;
	const vkcv::AttachmentDescription depth_attachment(
		vkcv::AttachmentOperation::STORE,
		vkcv::AttachmentOperation::CLEAR,
		depthBufferFormat
	);

	vkcv::PassConfig forwardPassDefinition({ color_attachment, depth_attachment });
	vkcv::PassHandle forwardPass = core.createPass(forwardPassDefinition);

	vkcv::shader::GLSLCompiler compiler;

	vkcv::ShaderProgram forwardProgram;
	compiler.compile(vkcv::ShaderStage::VERTEX, std::filesystem::path("resources/shaders/shader.vert"), 
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		forwardProgram.addShader(shaderStage, path);
	});
	compiler.compile(vkcv::ShaderStage::FRAGMENT, std::filesystem::path("resources/shaders/shader.frag"),
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		forwardProgram.addShader(shaderStage, path);
	});

	const std::vector<vkcv::VertexAttachment> vertexAttachments = forwardProgram.getVertexAttachments();

	std::vector<vkcv::VertexBinding> vertexBindings;
	for (size_t i = 0; i < vertexAttachments.size(); i++) {
		vertexBindings.push_back(vkcv::VertexBinding(i, { vertexAttachments[i] }));
	}
	const vkcv::VertexLayout vertexLayout (vertexBindings);

	// shadow map
	vkcv::SamplerHandle shadowSampler = core.createSampler(
		vkcv::SamplerFilterType::NEAREST,
		vkcv::SamplerFilterType::NEAREST,
		vkcv::SamplerMipmapMode::NEAREST,
		vkcv::SamplerAddressMode::CLAMP_TO_EDGE
	);
	const vk::Format shadowMapFormat = vk::Format::eD16Unorm;
	const uint32_t shadowMapResolution = 1024;
	const vkcv::Image shadowMap = core.createImage(shadowMapFormat, shadowMapResolution, shadowMapResolution);

	// light info buffer
	struct LightInfo {
		glm::vec3   direction;
		float       padding;
		glm::vec3   sunColor    = glm::vec3(1.f);
		float       sunStrength = 8.f;
		glm::mat4   lightMatrix;
	};
	LightInfo lightInfo;
	vkcv::Buffer lightBuffer = core.createBuffer<LightInfo>(vkcv::BufferType::UNIFORM, sizeof(glm::vec3));

	vkcv::DescriptorSetHandle forwardShadingDescriptorSet = 
		core.createDescriptorSet({ forwardProgram.getReflectedDescriptors()[0] });

	vkcv::DescriptorWrites forwardDescriptorWrites;
	forwardDescriptorWrites.uniformBufferWrites = { vkcv::UniformBufferDescriptorWrite(0, lightBuffer.getHandle()) };
	forwardDescriptorWrites.sampledImageWrites  = { vkcv::SampledImageDescriptorWrite(1, shadowMap.getHandle()) };
	forwardDescriptorWrites.samplerWrites       = { vkcv::SamplerDescriptorWrite(2, shadowSampler) };
	core.writeDescriptorSet(forwardShadingDescriptorSet, forwardDescriptorWrites);

	vkcv::SamplerHandle colorSampler = core.createSampler(
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerMipmapMode::LINEAR,
		vkcv::SamplerAddressMode::REPEAT
	);

	// create descriptor sets
	std::vector<vkcv::DescriptorSetHandle> materialDescriptorSets;
	std::vector<vkcv::Image> sceneImages;

	for (const auto& material : scene.materials) {
		int baseColorIndex = material.baseColor;
		if (baseColorIndex < 0) {
			vkcv_log(vkcv::LogLevel::WARNING, "Material lacks base color");
			baseColorIndex = 0;
		}

		materialDescriptorSets.push_back(core.createDescriptorSet(forwardProgram.getReflectedDescriptors()[1]));

		vkcv::asset::Texture& sceneTexture = scene.textures[baseColorIndex];

		sceneImages.push_back(core.createImage(vk::Format::eR8G8B8A8Srgb, sceneTexture.w, sceneTexture.h, 1, true));
		sceneImages.back().fill(sceneTexture.data.data());
		sceneImages.back().generateMipChainImmediate();
		sceneImages.back().switchLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

		vkcv::DescriptorWrites setWrites;
		setWrites.sampledImageWrites = {
			vkcv::SampledImageDescriptorWrite(0, sceneImages.back().getHandle())
		};
		setWrites.samplerWrites = {
			vkcv::SamplerDescriptorWrite(1, colorSampler),
		};
		core.writeDescriptorSet(materialDescriptorSets.back(), setWrites);
	}

	std::vector<vkcv::DescriptorSetHandle> perMeshDescriptorSets;
	for (const auto& vertexGroup : scene.vertexGroups) {
		perMeshDescriptorSets.push_back(materialDescriptorSets[vertexGroup.materialIndex]);
	}

	const vkcv::PipelineConfig forwardPipelineConfig {
		forwardProgram,
		windowWidth,
		windowHeight,
		forwardPass,
		vertexLayout,
		{	core.getDescriptorSet(forwardShadingDescriptorSet).layout, 
			core.getDescriptorSet(perMeshDescriptorSets[0]).layout },
		true
	};
	
	vkcv::PipelineHandle forwardPipeline = core.createGraphicsPipeline(forwardPipelineConfig);
	
	if (!forwardPipeline) {
		std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	vkcv::ImageHandle depthBuffer = core.createImage(depthBufferFormat, windowWidth, windowHeight).getHandle();
	vkcv::ImageHandle colorBuffer = core.createImage(colorBufferFormat, windowWidth, windowHeight, 1, false, true, true).getHandle();

	const vkcv::ImageHandle swapchainInput = vkcv::ImageHandle::createSwapchainImageHandle();

	vkcv::ShaderProgram shadowShader;
	compiler.compile(vkcv::ShaderStage::VERTEX, "resources/shaders/shadow.vert",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shadowShader.addShader(shaderStage, path);
	});
	compiler.compile(vkcv::ShaderStage::FRAGMENT, "resources/shaders/shadow.frag",
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		shadowShader.addShader(shaderStage, path);
	});

	const std::vector<vkcv::AttachmentDescription> shadowAttachments = {
		vkcv::AttachmentDescription(vkcv::AttachmentOperation::STORE, vkcv::AttachmentOperation::CLEAR, shadowMapFormat)
	};
	const vkcv::PassConfig shadowPassConfig(shadowAttachments);
	const vkcv::PassHandle shadowPass = core.createPass(shadowPassConfig);
	const vkcv::PipelineConfig shadowPipeConfig{
		shadowShader,
		shadowMapResolution,
		shadowMapResolution,
		shadowPass,
		vertexLayout,
		{},
		false
	};
	const vkcv::PipelineHandle shadowPipe = core.createGraphicsPipeline(shadowPipeConfig);

	std::vector<std::array<glm::mat4, 2>> mainPassMatrices;
	std::vector<glm::mat4> mvpLight;

	bool renderVoxelVis = false;
	window.e_key.add([&renderVoxelVis](int key ,int scancode, int action, int mods) {
		if (key == GLFW_KEY_V && action == GLFW_PRESS) {
			renderVoxelVis = !renderVoxelVis;
		}
	});

	// tonemapping compute shader
	vkcv::ShaderProgram tonemappingProgram;
	compiler.compile(vkcv::ShaderStage::COMPUTE, "resources/shaders/tonemapping.comp", 
		[&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		tonemappingProgram.addShader(shaderStage, path);
	});
	vkcv::DescriptorSetHandle tonemappingDescriptorSet = core.createDescriptorSet(tonemappingProgram.getReflectedDescriptors()[0]);
	vkcv::PipelineHandle tonemappingPipeline = core.createComputePipeline(tonemappingProgram,
		{ core.getDescriptorSet(tonemappingDescriptorSet).layout });

	// model matrices per mesh
	std::vector<glm::mat4> modelMatrices;
	modelMatrices.resize(scene.vertexGroups.size(), glm::mat4(1.f));
	for (const auto& mesh : scene.meshes) {
		const glm::mat4 m = *reinterpret_cast<const glm::mat4*>(&mesh.modelMatrix[0]);
		for (const auto& vertexGroupIndex : mesh.vertexGroups) {
			modelMatrices[vertexGroupIndex] = m;
		}
	}

	// prepare drawcalls
	std::vector<vkcv::Mesh> meshes;
	for (int i = 0; i < scene.vertexGroups.size(); i++) {
		vkcv::Mesh mesh(
			vertexBufferBindings[i], 
			indexBuffers[i].getVulkanHandle(), 
			scene.vertexGroups[i].numIndices);
		meshes.push_back(mesh);
	}

	std::vector<vkcv::DrawcallInfo> drawcalls;
	std::vector<vkcv::DrawcallInfo> shadowDrawcalls;
	for (int i = 0; i < meshes.size(); i++) {

		drawcalls.push_back(vkcv::DrawcallInfo(meshes[i], { 
			vkcv::DescriptorSetUsage(0, core.getDescriptorSet(forwardShadingDescriptorSet).vulkanHandle),
			vkcv::DescriptorSetUsage(1, core.getDescriptorSet(perMeshDescriptorSets[i]).vulkanHandle) }));
		shadowDrawcalls.push_back(vkcv::DrawcallInfo(meshes[i], {}));
	}

	Voxelization::Dependencies voxelDependencies;
	voxelDependencies.colorBufferFormat = colorBufferFormat;
	voxelDependencies.depthBufferFormat = depthBufferFormat;
	voxelDependencies.vertexLayout = vertexLayout;
	Voxelization voxelization(
		&core,
		voxelDependencies,
		lightBuffer.getHandle(),
		shadowMap.getHandle(),
		shadowSampler);

	vkcv::gui::GUI gui(core, window);

	glm::vec2 lightAngles(90.f, 0.f);
	int voxelVisualisationMip = 0;
	float voxelizationExtent = 20.f;

	auto start = std::chrono::system_clock::now();
	const auto appStartTime = start;
	while (window.isWindowOpen()) {
		vkcv::Window::pollEvents();

		uint32_t swapchainWidth, swapchainHeight;
		if (!core.beginFrame(swapchainWidth, swapchainHeight)) {
			continue;
		}

		if ((swapchainWidth != windowWidth) || ((swapchainHeight != windowHeight))) {
			depthBuffer = core.createImage(depthBufferFormat, swapchainWidth, swapchainHeight).getHandle();
			colorBuffer = core.createImage(colorBufferFormat, swapchainWidth, swapchainHeight, 1, false, true, true).getHandle();

			windowWidth = swapchainWidth;
			windowHeight = swapchainHeight;
		}

		auto end = std::chrono::system_clock::now();
		auto deltatime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

		// update descriptor sets which use swapchain image
		vkcv::DescriptorWrites tonemappingDescriptorWrites;
		tonemappingDescriptorWrites.storageImageWrites = {
			vkcv::StorageImageDescriptorWrite(0, colorBuffer),
			vkcv::StorageImageDescriptorWrite(1, swapchainInput) };
		core.writeDescriptorSet(tonemappingDescriptorSet, tonemappingDescriptorWrites);

		start = end;
		cameraManager.update(0.000001 * static_cast<double>(deltatime.count()));

		glm::vec2 lightAngleRadian = glm::radians(lightAngles);
		lightInfo.direction = glm::normalize(glm::vec3(
			std::cos(lightAngleRadian.x) * std::cos(lightAngleRadian.y),
			std::sin(lightAngleRadian.x),
			std::cos(lightAngleRadian.x) * std::sin(lightAngleRadian.y)));

		const float shadowProjectionSize = 20.f;
		glm::mat4 projectionLight = glm::ortho(
			-shadowProjectionSize,
			shadowProjectionSize,
			-shadowProjectionSize,
			shadowProjectionSize,
			-shadowProjectionSize,
			shadowProjectionSize);

		glm::mat4 vulkanCorrectionMatrix(1.f);
		vulkanCorrectionMatrix[2][2] = 0.5;
		vulkanCorrectionMatrix[3][2] = 0.5;
		projectionLight = vulkanCorrectionMatrix * projectionLight;

		const glm::mat4 viewLight = glm::lookAt(glm::vec3(0), -lightInfo.direction, glm::vec3(0, -1, 0));

		lightInfo.lightMatrix = projectionLight * viewLight;
		lightBuffer.fill({ lightInfo });

		const glm::mat4 viewProjectionCamera = cameraManager.getActiveCamera().getMVP();

		mainPassMatrices.clear();
		mvpLight.clear();
		for (const auto& m : modelMatrices) {
			mainPassMatrices.push_back({ viewProjectionCamera * m, m });
			mvpLight.push_back(lightInfo.lightMatrix * m);
		}

		vkcv::PushConstantData pushConstantData((void*)mainPassMatrices.data(), 2 * sizeof(glm::mat4));
		const std::vector<vkcv::ImageHandle> renderTargets = { colorBuffer, depthBuffer };

		const vkcv::PushConstantData shadowPushConstantData((void*)mvpLight.data(), sizeof(glm::mat4));

		auto cmdStream = core.createCommandStream(vkcv::QueueType::Graphics);

		// shadow map
		core.recordDrawcallsToCmdStream(
			cmdStream,
			shadowPass,
			shadowPipe,
			shadowPushConstantData,
			shadowDrawcalls,
			{ shadowMap.getHandle() });
		core.prepareImageForSampling(cmdStream, shadowMap.getHandle());

		voxelization.setVoxelExtent(voxelizationExtent);
		voxelization.voxelizeMeshes(
			cmdStream, 
			cameraManager.getActiveCamera().getPosition(), 
			meshes, 
			modelMatrices,
			perMeshDescriptorSets);

		// main pass
		core.recordDrawcallsToCmdStream(
			cmdStream,
            forwardPass,
            forwardPipeline,
			pushConstantData,
			drawcalls,
			renderTargets);

		if (renderVoxelVis) {
			voxelization.renderVoxelVisualisation(cmdStream, viewProjectionCamera, renderTargets, voxelVisualisationMip);
		}

		const uint32_t tonemappingLocalGroupSize = 8;
		const uint32_t tonemappingDispatchCount[3] = {
			static_cast<uint32_t>(glm::ceil(windowWidth / static_cast<float>(tonemappingLocalGroupSize))),
			static_cast<uint32_t>(glm::ceil(windowHeight / static_cast<float>(tonemappingLocalGroupSize))),
			1
		};

		core.prepareImageForStorage(cmdStream, swapchainInput);
		core.prepareImageForStorage(cmdStream, colorBuffer);

		core.recordComputeDispatchToCmdStream(
			cmdStream, 
			tonemappingPipeline, 
			tonemappingDispatchCount,
			{ vkcv::DescriptorSetUsage(0, core.getDescriptorSet(tonemappingDescriptorSet).vulkanHandle) },
			vkcv::PushConstantData(nullptr, 0));

		// present and end
		core.prepareSwapchainImageForPresent(cmdStream);
		core.submitCommandStream(cmdStream);

		gui.beginGUI();

		ImGui::Begin("Settings");
		ImGui::DragFloat2("Light angles",   &lightAngles.x);
		ImGui::ColorEdit3("Sun color",      &lightInfo.sunColor.x);
		ImGui::DragFloat("Sun strength",    &lightInfo.sunStrength);
		ImGui::Checkbox("Draw voxel visualisation", &renderVoxelVis);
		ImGui::SliderInt("Visualisation mip",       &voxelVisualisationMip, 0, 7);
		ImGui::DragFloat("Voxelization extent",     &voxelizationExtent, 1.f, 0.f);
		voxelVisualisationMip = std::max(voxelVisualisationMip, 0);
		ImGui::End();

		gui.endGUI();

		core.endFrame();
	}
	
	return 0;
}