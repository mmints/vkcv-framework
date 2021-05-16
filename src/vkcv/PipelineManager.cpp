#include "PipelineManager.hpp"

namespace vkcv
{

    PipelineManager::PipelineManager(vk::Device device) noexcept :
    m_Device{device},
    m_Pipelines{},
    m_PipelineLayouts{},
    m_NextPipelineId{1}
    {}

    PipelineManager::~PipelineManager() noexcept
    {
        for(const auto &pipeline: m_Pipelines)
            m_Device.destroy(pipeline);

        for(const auto &layout : m_PipelineLayouts)
            m_Device.destroy(layout);

        m_Pipelines.clear();
        m_PipelineLayouts.clear();
        m_NextPipelineId = 1;
    }

    PipelineHandle PipelineManager::createPipeline(const PipelineConfig &config, const vk::RenderPass &pass)
    {

        // TODO: this search could be avoided if ShaderProgram could be queried for a specific stage
        const auto shaderStageFlags = config.m_shaderProgram.getShaderStages();
        const auto shaderCode = config.m_shaderProgram.getShaderCode();
        std::vector<char> vertexCode;
        std::vector<char> fragCode;
        assert(shaderStageFlags.size() == shaderCode.size());
        for (int i = 0; i < shaderStageFlags.size(); i++) {
            switch (shaderStageFlags[i]) {
                case vk::ShaderStageFlagBits::eVertex: vertexCode = shaderCode[i]; break;
                case vk::ShaderStageFlagBits::eFragment: fragCode = shaderCode[i]; break;
                default: std::cout << "Core::createGraphicsPipeline encountered unknown shader stage" << std::endl;
                return PipelineHandle{0};
            }
        }

        const bool foundVertexCode = !vertexCode.empty();
        const bool foundFragCode = !fragCode.empty();
        const bool foundRequiredShaderCode = foundVertexCode && foundFragCode;
        if (!foundRequiredShaderCode) {
            std::cout << "Core::createGraphicsPipeline requires vertex and fragment shader code" << std::endl;
            return PipelineHandle{0};
        }

        // vertex shader stage
        // TODO: store shader code as uint32_t in ShaderProgram to avoid pointer cast
        vk::ShaderModuleCreateInfo vertexModuleInfo({}, vertexCode.size(), reinterpret_cast<uint32_t*>(vertexCode.data()));
        vk::ShaderModule vertexModule{};
        if (m_Device.createShaderModule(&vertexModuleInfo, nullptr, &vertexModule) != vk::Result::eSuccess)
            return PipelineHandle{0};

        vk::PipelineShaderStageCreateInfo pipelineVertexShaderStageInfo(
                {},
                vk::ShaderStageFlagBits::eVertex,
                vertexModule,
                "main",
                nullptr
        );

        // fragment shader stage
        vk::ShaderModuleCreateInfo fragmentModuleInfo({}, fragCode.size(), reinterpret_cast<uint32_t*>(fragCode.data()));
        vk::ShaderModule fragmentModule{};
        if (m_Device.createShaderModule(&fragmentModuleInfo, nullptr, &fragmentModule) != vk::Result::eSuccess)
        {
            m_Device.destroy(vertexModule);
            return PipelineHandle{0};
        }

        vk::PipelineShaderStageCreateInfo pipelineFragmentShaderStageInfo(
                {},
                vk::ShaderStageFlagBits::eFragment,
                fragmentModule,
                "main",
                nullptr
        );

        // vertex input state
        vk::VertexInputBindingDescription vertexInputBindingDescription(0, 12, vk::VertexInputRate::eVertex);
        vk::VertexInputAttributeDescription vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0);

        vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
                {},
                1,
                &vertexInputBindingDescription,
                1,
                &vertexInputAttributeDescription
        );

        // input assembly state
        vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(
                {},
                vk::PrimitiveTopology::eTriangleList,
                false
        );

        // viewport state
        vk::Viewport viewport(0.f, 0.f, static_cast<float>(config.m_width), static_cast<float>(config.m_height), 0.f, 1.f);
        vk::Rect2D scissor({ 0,0 }, { config.m_width, config.m_height });
        vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo({}, 1, &viewport, 1, &scissor);

        // rasterization state
        vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo(
                {},
                false,
                false,
                vk::PolygonMode::eFill,
                vk::CullModeFlagBits::eNone,
                vk::FrontFace::eCounterClockwise,
                false,
                0.f,
                0.f,
                0.f,
                1.f
        );

        // multisample state
        vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo(
                {},
                vk::SampleCountFlagBits::e1,
                false,
                0.f,
                nullptr,
                false,
                false
        );

        // color blend state
        vk::ColorComponentFlags colorWriteMask(VK_COLOR_COMPONENT_R_BIT |
                                               VK_COLOR_COMPONENT_G_BIT |
                                               VK_COLOR_COMPONENT_B_BIT |
                                               VK_COLOR_COMPONENT_A_BIT);
        vk::PipelineColorBlendAttachmentState colorBlendAttachmentState(
                false,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eOne,
                vk::BlendOp::eAdd,
                vk::BlendFactor::eOne,
                vk::BlendFactor::eOne,
                vk::BlendOp::eAdd,
                colorWriteMask
        );
        vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
                {},
                false,
                vk::LogicOp::eClear,
                1,	//TODO: hardcoded to one
                &colorBlendAttachmentState,
                { 1.f,1.f,1.f,1.f }
        );

        // pipeline layout
        vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo(
                {},
                0,
                {},
                0,
                {}
        );
        vk::PipelineLayout vkPipelineLayout{};
        if (m_Device.createPipelineLayout(&pipelineLayoutCreateInfo, nullptr, &vkPipelineLayout) != vk::Result::eSuccess)
        {
            m_Device.destroy(vertexModule);
            m_Device.destroy(fragmentModule);
            return PipelineHandle{0};
        }

        // graphics pipeline create
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { pipelineVertexShaderStageInfo, pipelineFragmentShaderStageInfo };
        vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
                {},
                static_cast<uint32_t>(shaderStages.size()),
                shaderStages.data(),
                &pipelineVertexInputStateCreateInfo,
                &pipelineInputAssemblyStateCreateInfo,
                nullptr,
                &pipelineViewportStateCreateInfo,
                &pipelineRasterizationStateCreateInfo,
                &pipelineMultisampleStateCreateInfo,
                nullptr,
                &pipelineColorBlendStateCreateInfo,
                nullptr,
                vkPipelineLayout,
                pass,
                0,
                {},
                0
        );

        vk::Pipeline vkPipeline{};
        if (m_Device.createGraphicsPipelines(nullptr, 1, &graphicsPipelineCreateInfo, nullptr, &vkPipeline) != vk::Result::eSuccess)
        {
            m_Device.destroy(vertexModule);
            m_Device.destroy(fragmentModule);
            return PipelineHandle{0};
        }

        m_Device.destroy(vertexModule);
        m_Device.destroy(fragmentModule);

        m_Pipelines.push_back(vkPipeline);
        m_PipelineLayouts.push_back(vkPipelineLayout);
        return PipelineHandle{m_NextPipelineId++};
    }

    vk::Pipeline PipelineManager::getVkPipeline(const PipelineHandle &handle) const
    {
        return m_Pipelines[handle.id -1];
    }

    vk::PipelineLayout PipelineManager::getVkPipelineLayout(const PipelineHandle &handle) const
    {
        return m_PipelineLayouts[handle.id - 1];
    }
}