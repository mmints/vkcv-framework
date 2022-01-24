#pragma once

#include "Logger.hpp"

#include <functional>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vkcv {

	class FeatureManager {
	private:
		vk::PhysicalDevice& m_physicalDevice;
		
		std::vector<const char*> m_supportedExtensions;
		std::vector<const char*> m_activeExtensions;
		
		vk::PhysicalDeviceFeatures2 m_featuresBase;
		std::vector<vk::BaseOutStructure*> m_featuresExtensions;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceFeatures& features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDevice16BitStorageFeatures& features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDevice8BitStorageFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceBufferDeviceAddressFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceDescriptorIndexingFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceHostQueryResetFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceImagelessFramebufferFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceMultiviewFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceProtectedMemoryFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceSamplerYcbcrConversionFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceScalarBlockLayoutFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceSeparateDepthStencilLayoutsFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceShaderAtomicInt64Features &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceShaderFloat16Int8Features& features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceShaderSubgroupExtendedTypesFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceTimelineSemaphoreFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceUniformBufferStandardLayoutFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceVariablePointersFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceVulkanMemoryModelFeatures &features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceMeshShaderFeaturesNV& features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT& features, bool required) const;
		
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceShaderAtomicFloat2FeaturesEXT& features, bool required) const;
		
		/**
         * @brief Currently used for RTX. Checks support of the @p vk::PhysicalDeviceVulkan12Features.
         * @param features The features.
         * @param required True, if the @p features are required, else false.
         * @return @p True, if the @p features are supported, else @p false.
         */
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceVulkan12Features& features, bool required) const;

		/**
         * @brief Currently used for RTX. Checks support of the @p vk::PhysicalDeviceVulkan11Features.
         * @param features The features.
         * @param required True, if the @p features are required, else false.
         * @return @p True, if the @p features are supported, else @p false.
         */
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceVulkan11Features& features, bool required) const;

		/**
		 * @brief Only used for RTX. Checks support of the @p vk::PhysicalDeviceAccelerationStructureFeaturesKHR.
		 * @param features The features.
		 * @param required True, if the @p features are required, else false.
		 * @return @p True, if the @p features are supported, else @p false.
		 */
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceAccelerationStructureFeaturesKHR& features, bool required) const;

		/**
         * @brief Only used for RTX. Checks support of the @p vk::PhysicalDeviceRayTracingPipelineFeaturesKHR.
         * @param features The features.
         * @param required True, if the @p features are required, else false.
         * @return @p True, if the @p features are supported, else @p false.
         */
		[[nodiscard]]
		bool checkSupport(const vk::PhysicalDeviceRayTracingPipelineFeaturesKHR& features, bool required) const;
		
		vk::BaseOutStructure* findFeatureStructure(vk::StructureType type) const;
	
	public:
		explicit FeatureManager(vk::PhysicalDevice& physicalDevice);
		
		FeatureManager(const FeatureManager& other) = delete;
		FeatureManager(FeatureManager&& other) noexcept;
		
		~FeatureManager();
		
		FeatureManager& operator=(const FeatureManager& other) = delete;
		FeatureManager& operator=(FeatureManager&& other) noexcept;
		
		[[nodiscard]]
		bool isExtensionSupported(const std::string& extension) const;
		
		bool useExtension(const std::string& extension, bool required = true);
		
		[[nodiscard]]
		bool isExtensionActive(const std::string& extension) const;
		
		[[nodiscard]]
		const std::vector<const char*>& getActiveExtensions() const;
		
		bool useFeatures(const std::function<void(vk::PhysicalDeviceFeatures&)>& featureFunction, bool required = true);
		
		template<typename T>
		bool useFeatures(const std::function<void(T&)>& featureFunction, bool required = true) {
			T features;
			T* features_ptr = reinterpret_cast<T*>(findFeatureStructure(features.sType));
			
			if (features_ptr) {
				features = *features_ptr;
			}
			
			featureFunction(features);
			
			if (!checkSupport(features, required)) {
				if (required) {
					throw std::runtime_error("Required feature is not supported!");
				}
				
				return false;
			}
			
			if (features_ptr) {
				*features_ptr = features;
				return true;
			}
			
			features_ptr = new T(features);
			
			if (m_featuresExtensions.empty()) {
				m_featuresBase.setPNext(features_ptr);
			} else {
				m_featuresExtensions.back()->setPNext(
						reinterpret_cast<vk::BaseOutStructure*>(features_ptr)
				);
			}
			
			m_featuresExtensions.push_back(
					reinterpret_cast<vk::BaseOutStructure*>(features_ptr)
			);
			
			return true;
		}
		
		[[nodiscard]]
		const vk::PhysicalDeviceFeatures2& getFeatures() const;
		
	};
	
}
