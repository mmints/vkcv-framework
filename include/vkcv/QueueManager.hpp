#pragma once
#include <vulkan/vulkan.hpp>

namespace vkcv {

	enum class QueueType { Compute, Transfer, Graphics, Present };

	struct Queue {
		int familyIndex;
		int queueIndex;
		
		vk::Queue handle;
	};
	
	class QueueManager {
	public:
		static QueueManager create(vk::Device device,
                            std::vector<std::pair<int, int>> &queuePairsGraphics,
                            std::vector<std::pair<int, int>> &queuePairsCompute,
                            std::vector<std::pair<int, int>> &queuePairsTransfer);

        [[nodiscard]]
        const Queue &getPresentQueue() const;
		
		[[nodiscard]]
		const std::vector<Queue> &getGraphicsQueues() const;
		
		[[nodiscard]]
        const std::vector<Queue> &getComputeQueues() const;
		
		[[nodiscard]]
        const std::vector<Queue> &getTransferQueues() const;

        static void queueCreateInfosQueueHandles(vk::PhysicalDevice &physicalDevice,
                std::vector<float> &queuePriorities,
                std::vector<vk::QueueFlagBits> &queueFlags,
                std::vector<vk::DeviceQueueCreateInfo> &queueCreateInfos,
                std::vector<std::pair<int, int>> &queuePairsGraphics,
                std::vector<std::pair<int, int>> &queuePairsCompute,
                std::vector<std::pair<int, int>> &queuePairsTransfer);

    private:
        std::vector<Queue> m_graphicsQueues;
        std::vector<Queue> m_computeQueues;
        std::vector<Queue> m_transferQueues;
		
		size_t m_presentIndex;

        QueueManager(std::vector<Queue>&& graphicsQueues, std::vector<Queue>&& computeQueues, std::vector<Queue>&& transferQueues, size_t presentIndex);
	};
}
