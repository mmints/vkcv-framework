#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include "vkcv/Handles.hpp"
#include "vkcv/PassConfig.hpp"

namespace vkcv
{
    class PassManager
    {
    private:
        vk::Device m_Device;
        std::vector<vk::RenderPass> m_RenderPasses;
        uint64_t m_NextPassId;
    public:
        PassManager() = delete; // no default ctor
        explicit PassManager(vk::Device device) noexcept; // ctor
        ~PassManager() noexcept; // dtor

        PassManager(const PassManager &other) = delete; // copy-ctor
        PassManager(PassManager &&other) = delete; // move-ctor;

        PassManager & operator=(const PassManager &other) = delete; // copy-assign op
        PassManager & operator=(PassManager &&other) = delete; // move-assign op

        PassHandle createPass(const PassConfig &config);

        [[nodiscard]]
        vk::RenderPass getVkPass(const PassHandle &handle) const;
    };
}
