#include <iostream>
#include <vkcv/Core.hpp>
#include <GLFW/glfw3.h>
#include <vkcv/camera/CameraManager.hpp>
#include <chrono>
#include <vkcv/asset/asset_loader.hpp>

int main(int argc, const char** argv) {
	const char* applicationName = "First Mesh";

	uint32_t windowWidth = 800;
	uint32_t windowHeight = 600;
	
	vkcv::Window window = vkcv::Window::create(
		applicationName,
		windowWidth,
		windowHeight,
		true
	);

	vkcv::CameraManager cameraManager(window, windowWidth, windowHeight);
	cameraManager.getCamera().setPosition(glm::vec3(0.f, 0.f, 3.f));
	cameraManager.getCamera().setNearFar(0.1, 30);

	window.initEvents();

	vkcv::Core core = vkcv::Core::create(
		window,
		applicationName,
		VK_MAKE_VERSION(0, 0, 1),
		{ vk::QueueFlagBits::eTransfer,vk::QueueFlagBits::eGraphics, vk::QueueFlagBits::eCompute },
		{},
		{ "VK_KHR_swapchain" }
	);

	vkcv::asset::Mesh mesh;

	const char* path = argc > 1 ? argv[1] : "resources/cube/cube.gltf";
	int result = vkcv::asset::loadMesh(path, mesh);

	if (result == 1) {
		std::cout << "Mesh loading successful!" << std::endl;
	}
	else {
		std::cout << "Mesh loading failed: " << result << std::endl;
		return 1;
	}

	assert(mesh.vertexGroups.size() > 0);
	auto vertexBuffer = core.createBuffer<uint8_t>(
			vkcv::BufferType::VERTEX,
			mesh.vertexGroups[0].vertexBuffer.data.size(),
			vkcv::BufferMemoryType::DEVICE_LOCAL
	);
	
	vertexBuffer.fill(mesh.vertexGroups[0].vertexBuffer.data);

	auto indexBuffer = core.createBuffer<uint8_t>(
			vkcv::BufferType::INDEX,
			mesh.vertexGroups[0].indexBuffer.data.size(),
			vkcv::BufferMemoryType::DEVICE_LOCAL
	);
	
	indexBuffer.fill(mesh.vertexGroups[0].indexBuffer.data);
	
	auto& attributes = mesh.vertexGroups[0].vertexBuffer.attributes;
	
	std::sort(attributes.begin(), attributes.end(), [](const vkcv::VertexAttribute& x, const vkcv::VertexAttribute& y) {
		return static_cast<uint32_t>(x.type) < static_cast<uint32_t>(y.type);
	});

	const std::vector<vkcv::VertexBufferBinding> vertexBufferBindings = {
		vkcv::VertexBufferBinding(attributes[0].offset, vertexBuffer.getVulkanHandle()),
		vkcv::VertexBufferBinding(attributes[1].offset, vertexBuffer.getVulkanHandle()),
		vkcv::VertexBufferBinding(attributes[2].offset, vertexBuffer.getVulkanHandle()) };

	const vkcv::Mesh loadedMesh(vertexBufferBindings, indexBuffer.getVulkanHandle(), mesh.vertexGroups[0].numIndices);

	// an example attachment for passes that output to the window
	const vkcv::AttachmentDescription present_color_attachment(
		vkcv::AttachmentOperation::STORE,
		vkcv::AttachmentOperation::CLEAR,
		core.getSwapchainImageFormat()
	);
	
	const vkcv::AttachmentDescription depth_attachment(
			vkcv::AttachmentOperation::STORE,
			vkcv::AttachmentOperation::CLEAR,
			vk::Format::eD32Sfloat
	);

	vkcv::PassConfig trianglePassDefinition({ present_color_attachment, depth_attachment });
	vkcv::PassHandle trianglePass = core.createPass(trianglePassDefinition);

	if (!trianglePass) {
		std::cout << "Error. Could not create renderpass. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	vkcv::ShaderProgram triangleShaderProgram{};
	triangleShaderProgram.addShader(vkcv::ShaderStage::VERTEX, std::filesystem::path("resources/shaders/vert.spv"));
	triangleShaderProgram.addShader(vkcv::ShaderStage::FRAGMENT, std::filesystem::path("resources/shaders/frag.spv"));
	triangleShaderProgram.reflectShader(vkcv::ShaderStage::VERTEX);
	triangleShaderProgram.reflectShader(vkcv::ShaderStage::FRAGMENT);
	
	std::vector<vkcv::DescriptorBinding> descriptorBindings = {
		vkcv::DescriptorBinding(vkcv::DescriptorType::IMAGE_SAMPLED,    1, vkcv::ShaderStage::FRAGMENT),
		vkcv::DescriptorBinding(vkcv::DescriptorType::SAMPLER,          1, vkcv::ShaderStage::FRAGMENT),
		vkcv::DescriptorBinding(vkcv::DescriptorType::UNIFORM_BUFFER,   1, vkcv::ShaderStage::FRAGMENT),
		vkcv::DescriptorBinding(vkcv::DescriptorType::IMAGE_SAMPLED,    1, vkcv::ShaderStage::FRAGMENT) ,
		vkcv::DescriptorBinding(vkcv::DescriptorType::SAMPLER,          1, vkcv::ShaderStage::FRAGMENT) };
	vkcv::DescriptorSetHandle descriptorSet = core.createDescriptorSet(descriptorBindings);

	const vkcv::PipelineConfig trianglePipelineDefinition(
		triangleShaderProgram, 
		windowWidth,
		windowHeight,
		trianglePass,
		attributes,
		{ core.getDescriptorSet(descriptorSet).layout },
		true);
	vkcv::PipelineHandle trianglePipeline = core.createGraphicsPipeline(trianglePipelineDefinition);
	
	if (!trianglePipeline) {
		std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
		return EXIT_FAILURE;
	}
	
	vkcv::Image texture = core.createImage(vk::Format::eR8G8B8A8Srgb, mesh.texture_hack.w, mesh.texture_hack.h);
	texture.fill(mesh.texture_hack.img);

	vkcv::SamplerHandle sampler = core.createSampler(
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerFilterType::LINEAR,
		vkcv::SamplerMipmapMode::LINEAR,
		vkcv::SamplerAddressMode::REPEAT
	);

    vkcv::SamplerHandle shadowSampler = core.createSampler(
        vkcv::SamplerFilterType::NEAREST,
        vkcv::SamplerFilterType::NEAREST,
        vkcv::SamplerMipmapMode::NEAREST,
        vkcv::SamplerAddressMode::CLAMP_TO_EDGE
    );

	vkcv::ImageHandle depthBuffer = core.createImage(vk::Format::eD32Sfloat, windowWidth, windowHeight).getHandle();

	const vkcv::ImageHandle swapchainInput = vkcv::ImageHandle::createSwapchainImageHandle();

	const vkcv::DescriptorSetUsage descriptorUsage(0, core.getDescriptorSet(descriptorSet).vulkanHandle);

	const std::vector<glm::vec3> instancePositions = {
		glm::vec3( 0.f, -2.f, 0.f),
		glm::vec3( 3.f,  0.f, 0.f),
		glm::vec3(-3.f,  0.f, 0.f),
		glm::vec3( 0.f,  2.f, 0.f),
		glm::vec3( 0.f, -5.f, 0.f)
	};

	std::vector<glm::mat4> modelMatrices;
	std::vector<vkcv::DrawcallInfo> drawcalls;
	std::vector<vkcv::DrawcallInfo> shadowDrawcalls;
	for (const auto& position : instancePositions) {
		modelMatrices.push_back(glm::translate(glm::mat4(1.f), position));
		drawcalls.push_back(vkcv::DrawcallInfo(loadedMesh, { descriptorUsage }));
		shadowDrawcalls.push_back(vkcv::DrawcallInfo(loadedMesh, {}));
	}

	modelMatrices.back() *= glm::scale(glm::mat4(1.f), glm::vec3(10.f, 1.f, 10.f));

	std::vector<std::array<glm::mat4, 2>> mainPassMatrices;
	std::vector<glm::mat4> mvpLight;

	vkcv::ShaderProgram shadowShader;
	shadowShader.addShader(vkcv::ShaderStage::VERTEX, "resources/shaders/shadow_vert.spv");
	shadowShader.addShader(vkcv::ShaderStage::FRAGMENT, "resources/shaders/shadow_frag.spv");
    shadowShader.reflectShader(vkcv::ShaderStage::VERTEX);
    shadowShader.reflectShader(vkcv::ShaderStage::FRAGMENT);

	const vk::Format shadowMapFormat = vk::Format::eD16Unorm;
	const std::vector<vkcv::AttachmentDescription> shadowAttachments = {
		vkcv::AttachmentDescription(vkcv::AttachmentOperation::STORE, vkcv::AttachmentOperation::CLEAR, shadowMapFormat)
	};
	const vkcv::PassConfig shadowPassConfig(shadowAttachments);
	const vkcv::PassHandle shadowPass = core.createPass(shadowPassConfig);

	const uint32_t shadowMapResolution = 1024;
	const vkcv::Image shadowMap = core.createImage(shadowMapFormat, shadowMapResolution, shadowMapResolution, 1);
	const vkcv::PipelineConfig shadowPipeConfig(
		shadowShader, 
		shadowMapResolution, 
		shadowMapResolution, 
		shadowPass, 
		attributes,
		{}, 
		false);
	const vkcv::PipelineHandle shadowPipe = core.createGraphicsPipeline(shadowPipeConfig);

	struct LightInfo {
		glm::vec3 direction;
		float padding;
		glm::mat4 lightMatrix;
	};
	LightInfo lightInfo;
	vkcv::Buffer lightBuffer = core.createBuffer<LightInfo>(vkcv::BufferType::UNIFORM, sizeof(glm::vec3));

	vkcv::DescriptorWrites setWrites;
	setWrites.sampledImageWrites    = { 
        vkcv::SampledImageDescriptorWrite(0, texture.getHandle()),
        vkcv::SampledImageDescriptorWrite(3, shadowMap.getHandle()) };
	setWrites.samplerWrites         = { 
        vkcv::SamplerDescriptorWrite(1, sampler), 
        vkcv::SamplerDescriptorWrite(4, shadowSampler) };
    setWrites.uniformBufferWrites   = { vkcv::UniformBufferDescriptorWrite(2, lightBuffer.getHandle()) };
	core.writeResourceDescription(descriptorSet, 0, setWrites);

	auto start = std::chrono::system_clock::now();
	const auto appStartTime = start;
	while (window.isWindowOpen()) {
		vkcv::Window::pollEvents();
		
		uint32_t swapchainWidth, swapchainHeight;
		if (!core.beginFrame(swapchainWidth, swapchainHeight)) {
			continue;
		}
		
		if ((swapchainWidth != windowWidth) || ((swapchainHeight != windowHeight))) {
			depthBuffer = core.createImage(vk::Format::eD32Sfloat, swapchainWidth, swapchainHeight).getHandle();
			
			windowWidth = swapchainWidth;
			windowHeight = swapchainHeight;
		}
		
		auto end = std::chrono::system_clock::now();
		auto deltatime = end - start;
		start = end;
		cameraManager.getCamera().updateView(std::chrono::duration<double>(deltatime).count());

		const float sunTheta = (end - appStartTime).count() * 0.0000001;
		lightInfo.direction = glm::normalize(glm::vec3(cos(sunTheta), 1, sin(sunTheta)));

		const float shadowProjectionSize = 5.f;
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

		const glm::mat4 viewProjectionCamera = cameraManager.getCamera().getProjection() * cameraManager.getCamera().getView();

		mainPassMatrices.clear();
		mvpLight.clear();
		for (const auto& m : modelMatrices) {
			mainPassMatrices.push_back({ viewProjectionCamera * m, m });
			mvpLight.push_back(lightInfo.lightMatrix* m);
		}

		vkcv::PushConstantData pushConstantData((void*)mainPassMatrices.data(), 2 * sizeof(glm::mat4));
		const std::vector<vkcv::ImageHandle> renderTargets = { swapchainInput, depthBuffer };

		vkcv::PushConstantData shadowPushConstantData((void*)mvpLight.data(), sizeof(glm::mat4));

		auto cmdStream = core.createCommandStream(vkcv::QueueType::Graphics);

		core.recordDrawcallsToCmdStream(
			cmdStream,
			shadowPass,
			shadowPipe,
			shadowPushConstantData,
			shadowDrawcalls,
			{ shadowMap.getHandle() });

		core.prepareImageForSampling(cmdStream, shadowMap.getHandle());

		core.recordDrawcallsToCmdStream(
			cmdStream,
			trianglePass,
			trianglePipeline,
			pushConstantData,
			drawcalls,
			renderTargets);
		core.prepareSwapchainImageForPresent(cmdStream);
		core.submitCommandStream(cmdStream);

		core.endFrame();
	}
	
	return 0;
}
