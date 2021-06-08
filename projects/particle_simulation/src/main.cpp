#include <iostream>
#include <vkcv/Core.hpp>
#include <GLFW/glfw3.h>
#include <vkcv/camera/CameraManager.hpp>
#include <chrono>

int main(int argc, const char** argv) {
    const char* applicationName = "First Triangle";

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

    const auto& context = core.getContext();
    const vk::Instance& instance = context.getInstance();
    const vk::PhysicalDevice& physicalDevice = context.getPhysicalDevice();
    const vk::Device& device = context.getDevice();

    struct vec3 {
        float x, y, z;
    };

    const size_t n = 5027;

    auto testBuffer = core.createBuffer<vec3>(vkcv::BufferType::VERTEX, n, vkcv::BufferMemoryType::DEVICE_LOCAL);
    vec3 vec_data[n];

    for (size_t i = 0; i < n; i++) {
        vec_data[i] = { 42, static_cast<float>(i), 7 };
    }

    testBuffer.fill(vec_data);

    auto triangleIndexBuffer = core.createBuffer<uint16_t>(vkcv::BufferType::INDEX, n, vkcv::BufferMemoryType::DEVICE_LOCAL);
    uint16_t indices[3] = { 0, 1, 2 };
    triangleIndexBuffer.fill(&indices[0], sizeof(indices));


    vkcv::SamplerHandle sampler = core.createSampler(
            vkcv::SamplerFilterType::NEAREST,
            vkcv::SamplerFilterType::NEAREST,
            vkcv::SamplerMipmapMode::NEAREST,
            vkcv::SamplerAddressMode::REPEAT
    );

    std::cout << "Physical device: " << physicalDevice.getProperties().deviceName << std::endl;

    switch (physicalDevice.getProperties().vendorID) {
        case 0x1002: std::cout << "Running AMD huh? You like underdogs, are you a Linux user?" << std::endl; break;
        case 0x10DE: std::cout << "An NVidia GPU, how predictable..." << std::endl; break;
        case 0x8086: std::cout << "Poor child, running on an Intel GPU, probably integrated..."
                                  "or perhaps you are from the future with a dedicated one?" << std::endl; break;
        case 0x13B5: std::cout << "ARM? What the hell are you running on, next thing I know you're trying to run Vulkan on a leg..." << std::endl; break;
        default: std::cout << "Unknown GPU vendor?! Either you're on an exotic system or your driver is broken..." << std::endl;
    }

    // an example attachment for passes that output to the window
    const vkcv::AttachmentDescription present_color_attachment(
            vkcv::AttachmentLayout::UNDEFINED,
            vkcv::AttachmentLayout::COLOR_ATTACHMENT,
            vkcv::AttachmentLayout::PRESENTATION,
            vkcv::AttachmentOperation::STORE,
            vkcv::AttachmentOperation::CLEAR,
            core.getSwapchainImageFormat());

    vkcv::PassConfig particlePassDefinition({ present_color_attachment });
    vkcv::PassHandle particlePass = core.createPass(particlePassDefinition);

    if (!particlePass)
    {
        std::cout << "Error. Could not create renderpass. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    vkcv::ShaderProgram particleShaderProgram{};
    particleShaderProgram.addShader(vkcv::ShaderStage::VERTEX, std::filesystem::path("shaders/vert.spv"));
    particleShaderProgram.addShader(vkcv::ShaderStage::FRAGMENT, std::filesystem::path("shaders/frag.spv"));
    particleShaderProgram.reflectShader(vkcv::ShaderStage::VERTEX);
    particleShaderProgram.reflectShader(vkcv::ShaderStage::FRAGMENT);

    vkcv::DescriptorSetConfig setConfig({
        vkcv::DescriptorBinding(vkcv::DescriptorType::UNIFORM_BUFFER, 1, vkcv::ShaderStage::FRAGMENT),
    });
    vkcv::ResourcesHandle set = core.createResourceDescription({setConfig});

    const vkcv::PipelineConfig particlePipelineDefinition(
            particleShaderProgram,
            windowWidth,
            windowHeight,
            particlePass,
            {},
            {core.getDescriptorSetLayout(set, 0)});
    vkcv::PipelineHandle particlePipeline = core.createGraphicsPipeline(particlePipelineDefinition);
    vkcv::Buffer<glm_vec4> color = core.createBuffer<glm_vec4>(
            vkcv::BufferType::UNIFORM,
            1
            );

    vkcv::DescriptorWrites setWrites;
    setWrites.uniformBufferWrites = {vkcv::UniformBufferDescriptorWrite(0,color.getHandle())};
    core.writeResourceDescription(set,0,setWrites);

    if (!particlePipeline)
    {
        std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<vkcv::VertexBufferBinding> vertexBufferBindings;

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

        core.renderMesh(
                particlePass,
                particlePipeline,
                sizeof(mvp),
                &mvp,
                vertexBufferBindings,
                triangleIndexBuffer.getHandle(),
                3,
                vkcv::ResourcesHandle(),
                0);

        core.endFrame();
    }
    return 0;
}
