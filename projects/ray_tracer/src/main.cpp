#include <iostream>
#include <vkcv/Core.hpp>
#include <vkcv/Buffer.hpp>
#include <vkcv/Pass.hpp>
#include <vkcv/Sampler.hpp>

#include <vkcv/camera/CameraManager.hpp>
#include <vkcv/asset/asset_loader.hpp>
#include <vkcv/shader/GLSLCompiler.hpp>

#include <cmath>
#include <vector>
#include <cstring>
#include <GLFW/glfw3.h>

#include "scene.hpp"

void createQuadraticLightCluster(std::vector<scene::Light>& lights, int countPerDimension, float dimension, float height, float intensity) {
    float distance = dimension/countPerDimension;

    for(int x = 0; x <= countPerDimension; x++) {
        for (int z = 0; z <= countPerDimension; z++) {
            lights.push_back(scene::Light(glm::vec3(x * distance, height, z * distance),
                                              float (intensity/countPerDimension) / 10.f) // Divide by 10, because intensity is busting O.o
                                              );
        }
    }

}

int main(int argc, const char** argv) {
	const std::string applicationName = "Ray tracer";

	//window creation
	const int windowWidth = 800;
	const int windowHeight = 600;

	vkcv::Features features;
	features.requireExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	vkcv::Core core = vkcv::Core::create(
		applicationName,
		VK_MAKE_VERSION(0, 0, 1),
		{ vk::QueueFlagBits::eTransfer,vk::QueueFlagBits::eGraphics, vk::QueueFlagBits::eCompute },
		features
	);

	vkcv::WindowHandle windowHandle = core.createWindow(applicationName, windowWidth, windowHeight, true);
	vkcv::Window& window = core.getWindow(windowHandle);

	std::string shaderPathCompute = "shaders/raytracing.comp";

	//creating the shader programs
	vkcv::shader::GLSLCompiler compiler;
	vkcv::ShaderProgram computeShaderProgram;

	compiler.compile(vkcv::ShaderStage::COMPUTE, shaderPathCompute, [&](vkcv::ShaderStage shaderStage, const std::filesystem::path& path) {
		computeShaderProgram.addShader(shaderStage, path);
	});

	vkcv::ImageHandle swapchainInput = vkcv::ImageHandle::createSwapchainImageHandle();

	const vkcv::DescriptorBindings& computeDescriptorBindings = computeShaderProgram.getReflectedDescriptors().at(0);
	
	vkcv::DescriptorSetLayoutHandle computeDescriptorSetLayout = core.createDescriptorSetLayout(computeDescriptorBindings);
	vkcv::DescriptorSetHandle computeDescriptorSet = core.createDescriptorSet(computeDescriptorSetLayout);

	const std::vector<vkcv::VertexAttachment> computeVertexAttachments = computeShaderProgram.getVertexAttachments();

	std::vector<vkcv::VertexBinding> computeBindings;
	for (size_t i = 0; i < computeVertexAttachments.size(); i++) {
		computeBindings.push_back(vkcv::createVertexBinding(i, { computeVertexAttachments[i] }));
	}
	const vkcv::VertexLayout computeLayout { computeBindings };
	
	/*
	* create the scene
	*/

	//materials for the spheres
	std::vector<scene::Material> materials;
	scene::Material ivory(glm::vec4(0.6, 0.3, 0.1, 0.0), glm::vec3(0.4, 0.4, 0.3), 50., 1.0);
	scene::Material red_rubber(glm::vec4(0.9, 0.1, 0.0, 0.0), glm::vec3(0.3, 0.1, 0.1), 10., 1.0);
	scene::Material mirror(glm::vec4(0.0, 10.0, 0.8, 0.0), glm::vec3(1.0, 1.0, 1.0), 1425., 1.0);
    scene::Material glass(glm::vec4(0.0, 10.0, 0.8, 0.0), glm::vec3(1.0, 1.0, 1.0), 1425., 1.5);

	materials.push_back(ivory);
	materials.push_back(red_rubber);
	materials.push_back(mirror);

	//spheres for the scene
	std::vector<scene::Sphere> spheres;
	spheres.push_back(scene::Sphere(glm::vec3(-3, 0, -16), 2, ivory));
	//spheres.push_back(safrScene::Sphere(glm::vec3(-1.0, -1.5, 12), 2, mirror));
	spheres.push_back(scene::Sphere(glm::vec3(-1.0, -1.5, -12), 2, glass));
	spheres.push_back(scene::Sphere(glm::vec3(1.5, -0.5, -18), 3, red_rubber));
	spheres.push_back(scene::Sphere(glm::vec3(7, 5, -18), 4, mirror));

	//lights for the scene
	std::vector<scene::Light> lights;
	/*
	lights.push_back(safrScene::Light(glm::vec3(-20, 20,  20), 1.5));
	lights.push_back(safrScene::Light(glm::vec3(30,  50, -25), 1.8));
	lights.push_back(safrScene::Light(glm::vec3(30,  20,  30), 1.7));
    */
    createQuadraticLightCluster(lights, 10, 2.5f, 20, 1.5f);
	
	vkcv::SamplerHandle sampler = vkcv::samplerLinear(core);
	
	//create Buffer for compute shader
	vkcv::Buffer<scene::Light> lightsBuffer = vkcv::buffer<scene::Light>(
		core,
		vkcv::BufferType::STORAGE,
		lights.size()
	);
	lightsBuffer.fill(lights);

	vkcv::Buffer<scene::Sphere> sphereBuffer = vkcv::buffer<scene::Sphere>(
		core,
		vkcv::BufferType::STORAGE,
		spheres.size()
	);
	sphereBuffer.fill(spheres);

	vkcv::DescriptorWrites computeWrites;
	computeWrites.writeStorageBuffer(
			0, lightsBuffer.getHandle()
	).writeStorageBuffer(
			1, sphereBuffer.getHandle()
	);
	
    core.writeDescriptorSet(computeDescriptorSet, computeWrites);

	auto safrIndexBuffer = vkcv::buffer<uint16_t>(core, vkcv::BufferType::INDEX, 3);
	uint16_t indices[3] = { 0, 1, 2 };
	safrIndexBuffer.fill(&indices[0], sizeof(indices));
	
	vkcv::PassHandle safrPass = vkcv::passSwapchain(
			core,
			window.getSwapchain(),
			{ vk::Format::eUndefined }
	);

	if (!safrPass) {
		std::cout << "Error. Could not create renderpass. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	vkcv::ComputePipelineHandle computePipeline = core.createComputePipeline(
			vkcv::ComputePipelineConfig(
					computeShaderProgram,
					{ computeDescriptorSetLayout }
			)
	);

	if (!computePipeline) {
		std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	//create the camera
	vkcv::camera::CameraManager cameraManager(window);
	auto camHandle0 = cameraManager.addCamera(vkcv::camera::ControllerType::PILOT);
	auto camHandle1 = cameraManager.addCamera(vkcv::camera::ControllerType::TRACKBALL);

	cameraManager.getCamera(camHandle0).setPosition(glm::vec3(0, 0, 2));
	
	cameraManager.getCamera(camHandle1).setPosition(glm::vec3(0, 0, -4));
	cameraManager.getCamera(camHandle1).setCenter(glm::vec3(0.0f, 0.0f, -1.0f));
	
	core.run([&](const vkcv::WindowHandle &windowHandle, double t, double dt,
				 uint32_t swapchainWidth, uint32_t swapchainHeight) {
		//adjust light position
		/*
		639a53157e7d3936caf7c3e40379159cbcf4c89e
		lights[0].position.x += std::cos(time * 3.0f) * 2.5f;
		lights[1].position.z += std::cos(time * 2.5f) * 3.0f;
		lights[2].position.y += std::cos(time * 1.5f) * 4.0f;
		lightsBuffer.fill(lights);
		*/

		spheres[0].center.y += std::cos(t * 0.5f * 3.141f) * 0.25f;
		spheres[1].center.x += std::cos(t * 2.f) * 0.25f;
		spheres[1].center.z += std::cos(t * 2.f + 0.5f * 3.141f) * 0.25f;
        sphereBuffer.fill(spheres);

		//update camera
		cameraManager.update(dt);
		glm::mat4 mvp = cameraManager.getActiveCamera().getMVP();
		glm::mat4 proj = cameraManager.getActiveCamera().getProjection();

		//create pushconstants for render
		vkcv::PushConstants pushConstants(sizeof(glm::mat4) * 2);
		pushConstants.appendDrawcall(std::array<glm::mat4, 2>{ mvp, proj });

		auto cmdStream = core.createCommandStream(vkcv::QueueType::Graphics);

		//configure the outImage for compute shader (render into the swapchain image)
        computeWrites.writeStorageImage(2, swapchainInput);
        core.writeDescriptorSet(computeDescriptorSet, computeWrites);
        core.prepareImageForStorage (cmdStream, swapchainInput);

		//fill pushconstants for compute shader
        struct RaytracingPushConstantData {
            glm::mat4 viewToWorld;
            int32_t lightCount;
            int32_t sphereCount;
        };

        RaytracingPushConstantData raytracingPushData;
        raytracingPushData.lightCount   = lights.size();
        raytracingPushData.sphereCount  = spheres.size();
        raytracingPushData.viewToWorld  = glm::inverse(cameraManager.getActiveCamera().getView());

        vkcv::PushConstants pushConstantsCompute = vkcv::pushConstants<RaytracingPushConstantData>();
        pushConstantsCompute.appendDrawcall(raytracingPushData);

		//dispatch compute shader
		const auto computeDispatchCount = vkcv::dispatchInvocations(
				vkcv::DispatchSize(swapchainWidth, swapchainHeight),
				vkcv::DispatchSize(16, 16)
		);
		
		core.recordComputeDispatchToCmdStream(cmdStream,
			computePipeline,
			computeDispatchCount,
			{ vkcv::useDescriptorSet(0, computeDescriptorSet) },
			pushConstantsCompute
		);

		core.recordBufferMemoryBarrier(cmdStream, lightsBuffer.getHandle());

		core.prepareSwapchainImageForPresent(cmdStream);
		core.submitCommandStream(cmdStream);
	});
	
	return 0;
}
