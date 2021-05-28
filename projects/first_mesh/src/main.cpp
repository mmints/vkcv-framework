#include <iostream>
#include <vkcv/Core.hpp>
#include <GLFW/glfw3.h>
#include <vkcv/camera/CameraManager.hpp>
#include <chrono>
#include <vkcv/asset/asset_loader.hpp>


int main(int argc, const char** argv) {
	const char* applicationName = "First Mesh";

	const int windowWidth = 800;
	const int windowHeight = 600;
	vkcv::Window window = vkcv::Window::create(
		applicationName,
		windowWidth,
		windowHeight,
		false
	);

	vkcv::CameraManager cameraManager(window, windowWidth, windowHeight);

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
	const size_t vertexBufferSize = mesh.vertexGroups[0].vertexBuffer.data.size();
	auto vertexBuffer = core.createBuffer<uint8_t>(vkcv::BufferType::VERTEX, vertexBufferSize, vkcv::BufferMemoryType::DEVICE_LOCAL);
	vertexBuffer.fill(mesh.vertexGroups[0].vertexBuffer.data.data(), vertexBufferSize);

	const size_t indexBufferSize = mesh.vertexGroups[0].indexBuffer.data.size();
	auto indexBuffer = core.createBuffer<uint8_t>(vkcv::BufferType::INDEX, indexBufferSize, vkcv::BufferMemoryType::DEVICE_LOCAL);
	indexBuffer.fill(mesh.vertexGroups[0].indexBuffer.data.data(), indexBufferSize);

	// an example attachment for passes that output to the window
	const vkcv::AttachmentDescription present_color_attachment(
		vkcv::AttachmentLayout::UNDEFINED,
		vkcv::AttachmentLayout::COLOR_ATTACHMENT,
		vkcv::AttachmentLayout::PRESENTATION,
		vkcv::AttachmentOperation::STORE,
		vkcv::AttachmentOperation::CLEAR,
		core.getSwapchainImageFormat());

	vkcv::PassConfig trianglePassDefinition({ present_color_attachment });
	vkcv::PassHandle trianglePass = core.createPass(trianglePassDefinition);

	if (trianglePass.id == 0)
	{
		std::cout << "Error. Could not create renderpass. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	vkcv::ShaderProgram triangleShaderProgram{};
	triangleShaderProgram.addShader(vkcv::ShaderStage::VERTEX, std::filesystem::path("resources/shaders/vert.spv"));
	triangleShaderProgram.addShader(vkcv::ShaderStage::FRAGMENT, std::filesystem::path("resources/shaders/frag.spv"));
	triangleShaderProgram.reflectShader(vkcv::ShaderStage::VERTEX);
	triangleShaderProgram.reflectShader(vkcv::ShaderStage::FRAGMENT);

	const vkcv::PipelineConfig trianglePipelineDefinition(triangleShaderProgram, windowWidth, windowHeight, trianglePass);
	vkcv::PipelineHandle trianglePipeline = core.createGraphicsPipeline(trianglePipelineDefinition);
	if (trianglePipeline.id == 0)
	{
		std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	auto start = std::chrono::system_clock::now();
	while (window.isWindowOpen())
	{
		core.beginFrame();
		window.pollEvents();
		auto end = std::chrono::system_clock::now();
		auto deltatime = end - start;
		start = end;
		cameraManager.getCamera().updateView(std::chrono::duration<double>(deltatime).count());
		const glm::mat4 mvp = cameraManager.getCamera().getProjection() * cameraManager.getCamera().getView();

		core.renderMesh(trianglePass, trianglePipeline, windowWidth, windowHeight, sizeof(mvp), &mvp, vertexBuffer.getHandle(), indexBuffer.getHandle(), mesh.vertexGroups[0].numIndices);
		core.endFrame();
	}
	return 0;
}
