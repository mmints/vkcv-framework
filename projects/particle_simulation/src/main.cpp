#include <iostream>
#include <vkcv/Core.hpp>
#include <GLFW/glfw3.h>
#include <vkcv/camera/CameraManager.hpp>
#include <chrono>
#include "ParticleSystem.hpp"
#include <random>
#include <glm/gtc/matrix_access.hpp>

int main(int argc, const char **argv) {
    const char *applicationName = "Particlesystem";

    uint32_t windowWidth = 800;
    uint32_t windowHeight = 600;
    vkcv::Window window = vkcv::Window::create(
            applicationName,
            windowWidth,
            windowHeight,
            true
    );

    vkcv::CameraManager cameraManager(window, windowWidth, windowHeight);
    cameraManager.getCamera().setNearFar(0.000000001f, 10.f);

    window.initEvents();

    vkcv::Core core = vkcv::Core::create(
            window,
            applicationName,
            VK_MAKE_VERSION(0, 0, 1),
            {vk::QueueFlagBits::eTransfer, vk::QueueFlagBits::eGraphics, vk::QueueFlagBits::eCompute},
            {},
            {"VK_KHR_swapchain"}
    );

    auto particleIndexBuffer = core.createBuffer<uint16_t>(vkcv::BufferType::INDEX, 3,
                                                           vkcv::BufferMemoryType::DEVICE_LOCAL);
    uint16_t indices[3] = {0, 1, 2};
    particleIndexBuffer.fill(&indices[0], sizeof(indices));


    // an example attachment for passes that output to the window
    const vkcv::AttachmentDescription present_color_attachment(
            vkcv::AttachmentOperation::STORE,
            vkcv::AttachmentOperation::CLEAR,
            core.getSwapchainImageFormat());


    vkcv::PassConfig particlePassDefinition({present_color_attachment});
    vkcv::PassHandle particlePass = core.createPass(particlePassDefinition);

    vkcv::PassConfig computePassDefinition({});
    vkcv::PassHandle computePass = core.createPass(computePassDefinition);

    if (!particlePass || !computePass)
    {
        std::cout << "Error. Could not create renderpass. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    vkcv::ShaderProgram computeShaderProgram{};
    computeShaderProgram.addShader(vkcv::ShaderStage::COMPUTE, std::filesystem::path("shaders/comp1.spv"));

    vkcv::DescriptorSetHandle computeDescriptorSet = core.createDescriptorSet(computeShaderProgram.getReflectedDescriptors()[0]);

    const std::vector<vkcv::VertexAttachment> computeVertexAttachments = computeShaderProgram.getVertexAttachments();

    std::vector<vkcv::VertexBinding> computeBindings;
    for (size_t i = 0; i < computeVertexAttachments.size(); i++) {
        computeBindings.push_back(vkcv::VertexBinding(i, { computeVertexAttachments[i] }));
    }
    const vkcv::VertexLayout computeLayout(computeBindings);

    vkcv::ShaderProgram particleShaderProgram{};
    particleShaderProgram.addShader(vkcv::ShaderStage::VERTEX, std::filesystem::path("shaders/vert.spv"));
    particleShaderProgram.addShader(vkcv::ShaderStage::FRAGMENT, std::filesystem::path("shaders/frag.spv"));

    vkcv::DescriptorSetHandle descriptorSet = core.createDescriptorSet(
            particleShaderProgram.getReflectedDescriptors()[0]);

    vkcv::Buffer<glm::vec3> vertexBuffer = core.createBuffer<glm::vec3>(
            vkcv::BufferType::VERTEX,
            3
    );
    const std::vector<vkcv::VertexAttachment> vertexAttachments = particleShaderProgram.getVertexAttachments();

    const std::vector<vkcv::VertexBufferBinding> vertexBufferBindings = {
            vkcv::VertexBufferBinding(0, vertexBuffer.getVulkanHandle())};

    std::vector<vkcv::VertexBinding> bindings;
    for (size_t i = 0; i < vertexAttachments.size(); i++) {
        bindings.push_back(vkcv::VertexBinding(i, {vertexAttachments[i]}));
    }

    const vkcv::VertexLayout particleLayout(bindings);

    const vkcv::PipelineConfig particlePipelineDefinition(
            particleShaderProgram,
            UINT32_MAX,
            UINT32_MAX,
            particlePass,
            {particleLayout},
            {core.getDescriptorSet(descriptorSet).layout},
            true);

    const std::vector<glm::vec3> vertices = {glm::vec3(-0.005, 0.005, 0),
                                             glm::vec3(0.005, 0.005, 0),
                                             glm::vec3(0, -0.005, 0)};

    vertexBuffer.fill(vertices);

    vkcv::PipelineHandle particlePipeline = core.createGraphicsPipeline(particlePipelineDefinition);

    vkcv::PipelineHandle computePipeline = core.createComputePipeline(computeShaderProgram, {core.getDescriptorSet(computeDescriptorSet).layout} );

    vkcv::Buffer<glm::vec4> color = core.createBuffer<glm::vec4>(
            vkcv::BufferType::UNIFORM,
            1
    );

    vkcv::Buffer<glm::vec2> position = core.createBuffer<glm::vec2>(
            vkcv::BufferType::UNIFORM,
            1
    );

    glm::vec3 minVelocity = glm::vec3(-0.0f,-0.0f,0.f);
    glm::vec3 maxVelocity = glm::vec3(0.0f,0.0f,0.f);
    glm::vec2 lifeTime = glm::vec2(0.f,5.f);
    ParticleSystem particleSystem = ParticleSystem( 100 , minVelocity, maxVelocity, lifeTime);

    vkcv::Buffer<Particle> particleBuffer = core.createBuffer<Particle>(
            vkcv::BufferType::STORAGE,
            particleSystem.getParticles().size()
    );

    particleBuffer.fill(particleSystem.getParticles());

    vkcv::DescriptorWrites setWrites;
    setWrites.uniformBufferWrites = {vkcv::UniformBufferDescriptorWrite(0,color.getHandle()),
                                     vkcv::UniformBufferDescriptorWrite(1,position.getHandle())};
    setWrites.storageBufferWrites = { vkcv::StorageBufferDescriptorWrite(2,particleBuffer.getHandle())};
    core.writeDescriptorSet(descriptorSet, setWrites);

    vkcv::DescriptorWrites computeWrites;
    computeWrites.storageBufferWrites = { vkcv::StorageBufferDescriptorWrite(0,particleBuffer.getHandle())};
    core.writeDescriptorSet(computeDescriptorSet, computeWrites);

    if (!particlePipeline || !computePipeline)
    {
        std::cout << "Error. Could not create graphics pipeline. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    const vkcv::ImageHandle swapchainInput = vkcv::ImageHandle::createSwapchainImageHandle();

    const vkcv::Mesh renderMesh({vertexBufferBindings}, particleIndexBuffer.getVulkanHandle(),
                                particleIndexBuffer.getCount());
    vkcv::DescriptorSetUsage descriptorUsage(0, core.getDescriptorSet(descriptorSet).vulkanHandle);
    //vkcv::DrawcallInfo drawcalls(renderMesh, {vkcv::DescriptorSetUsage(0, core.getDescriptorSet(descriptorSet).vulkanHandle)});

    std::uniform_real_distribution<float> rdmVel(-0.1f, 0.1f);
    std::default_random_engine rdmEngine;

    ParticleSystem particleSystem;
    particleSystem.setMaxLifeTime(3.f);
    glm::vec3 vel0 = glm::vec3(rdmVel(rdmEngine), rdmVel(rdmEngine), 0.0f);
    glm::vec3 vel1 = glm::vec3(rdmVel(rdmEngine), rdmVel(rdmEngine), 0.0f);
    glm::vec3 vel2 = glm::vec3(rdmVel(rdmEngine), rdmVel(rdmEngine), 0.0f);
    particleSystem.addParticles({
                                        Particle(glm::vec3(0.f, 1.f, 0.0f), vel0, 1.f),
                                        Particle(glm::vec3(0.2f, 0.1f, 0.0f), vel1, 1.5f),
                                        Particle(glm::vec3(0.15f, 0.f, 0.0f), vel2, 2.f),
                                        Particle(glm::vec3(-0.15f, 0.1f, 0.0f), vel2, 2.5f),
                                        Particle(glm::vec3(0.25f, 0.f, 0.0f), vel2, 3.f),
                                        Particle(glm::vec3(-0.15f, 0.2f, 0.0f), vel2, 3.5f)});

    glm::vec2 pos = glm::vec2(0.f);
    glm::vec3 spawnPosition = glm::vec3(0.f);
    glm::vec4 tempPosition = glm::vec4(0.f);

    window.e_mouseMove.add([&](double offsetX, double offsetY) {
        pos = glm::vec2(static_cast<float>(offsetX), static_cast<float>(offsetY));
        // borders are assumed to be 0.5
        //pos = glm::vec2((pos.x -0.5f * static_cast<float>(window.getWidth()))/static_cast<float>(window.getWidth()), (pos.y -0.5f * static_cast<float>(window.getHeight()))/static_cast<float>(window.getHeight()));
        //borders are assumed to be 1
        pos.x = (2 * pos.x - static_cast<float>(window.getWidth())) / static_cast<float>(window.getWidth());
        pos.y = (2 * pos.y - static_cast<float>(window.getHeight())) / static_cast<float>(window.getHeight());
        glm::vec4 camera_pos = glm::column(cameraManager.getCamera().getView(), 3);
        std::cout << "camerapos: " << camera_pos.x << ", " << camera_pos.y << ", " << camera_pos.z << std::endl;
        //glm::vec4 view_axis = glm::row(cameraManager.getCamera().getView(), 2);
        // std::cout << "view_axis: " << view_axis.x << ", " << view_axis.y << ", " << view_axis.z << std::endl;
        //std::cout << "Front: " << cameraManager.getCamera().getFront().x << ", " << cameraManager.getCamera().getFront().z << ", " << cameraManager.getCamera().getFront().z << std::endl;
        glm::mat4 viewmat = cameraManager.getCamera().getView();
        spawnPosition = glm::vec3(pos.x, pos.y, 0.f);
        tempPosition = glm::inverse(viewmat) * glm::vec4(spawnPosition, 1.0f);
        spawnPosition = glm::vec3(tempPosition.x, tempPosition.y, tempPosition.z);
        particleSystem.setRespawnPos(glm::vec3(-spawnPosition.x, -spawnPosition.y, -spawnPosition.z));
        std::cout << "respawn pos: " << spawnPosition.x << ", " << spawnPosition.y << ", " << spawnPosition.z << std::endl;
    });

    std::vector<glm::mat4> modelMatrices;
    std::vector<vkcv::DrawcallInfo> drawcalls;
    for (auto particle :  particleSystem.getParticles()) {
        modelMatrices.push_back(glm::translate(glm::mat4(1.f), particle.getPosition()));
        drawcalls.push_back(vkcv::DrawcallInfo(renderMesh, {descriptorUsage}));
    }

    auto start = std::chrono::system_clock::now();

    glm::vec4 colorData = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    cameraManager.getCamera().setPosition(glm::vec3(0.f, 0.f, 0.f));
    std::cout << "FRONT: " << cameraManager.getCamera().getFront().x << ", " << cameraManager.getCamera().getFront().y << ", " << cameraManager.getCamera().getFront().z << std::endl;

    while (window.isWindowOpen()) {
        window.pollEvents();

        uint32_t swapchainWidth, swapchainHeight;
        if (!core.beginFrame(swapchainWidth, swapchainHeight)) {
            continue;
        }

        color.fill(&colorData);


        position.fill(&pos);
        auto end = std::chrono::system_clock::now();
        float deltatime = std::chrono::duration<float>(end - start).count();
        start = end;
        particleSystem.updateParticles(deltatime);

        modelMatrices.clear();
        for (Particle particle :  particleSystem.getParticles()) {
            modelMatrices.push_back(glm::translate(glm::mat4(1.f), particle.getPosition()));
        }

        cameraManager.getCamera().updateView(deltatime);
        std::vector<glm::mat4> mvp;
        mvp.clear();
        for (const auto &m : modelMatrices) {
            mvp.push_back(m * cameraManager.getCamera().getProjection() * cameraManager.getCamera().getView());
        }

        vkcv::PushConstantData pushConstantData((void *) mvp.data(), sizeof(glm::mat4));
        auto cmdStream = core.createCommandStream(vkcv::QueueType::Graphics);

        uint32_t computeDispatchCount[3] = {static_cast<uint32_t> (std::ceil(particleSystem.getParticles().size()/64.f)),1,1};
        core.recordComputeDispatchToCmdStream(cmdStream,
                                              computePipeline,
                                              computeDispatchCount,
                                              {vkcv::DescriptorSetUsage(0,core.getDescriptorSet(computeDescriptorSet).vulkanHandle)},
                                               vkcv::PushConstantData(nullptr, 0));

        core.recordDrawcallsToCmdStream(
                cmdStream,
                particlePass,
                particlePipeline,
                pushConstantData,
                {drawcalls},
                {swapchainInput});
        core.prepareSwapchainImageForPresent(cmdStream);
        core.submitCommandStream(cmdStream);
        core.endFrame();
    }

    return 0;
}
