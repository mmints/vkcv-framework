
#include "vkcv/upscaling/FSR2Upscaling.hpp"

#include <cmath>

#define FFX_GCC
#include <ffx_fsr2_vk.h>
#undef FFX_GCC

namespace vkcv::upscaling {
	
	void getFSR2Resolution(FSR2QualityMode mode,
						   uint32_t outputWidth, uint32_t outputHeight,
						   uint32_t &inputWidth, uint32_t &inputHeight) {
		float scale;
		
		switch (mode) {
			case FSR2QualityMode::QUALITY:
				scale = 1.5f;
				break;
			case FSR2QualityMode::BALANCED:
				scale = 1.7f;
				break;
			case FSR2QualityMode::PERFORMANCE:
				scale = 2.0f;
				break;
			case FSR2QualityMode::ULTRA_PERFORMANCE:
				scale = 3.0f;
				break;
			default:
				scale = 1.0f;
				break;
		}
		
		inputWidth = static_cast<uint32_t>(
				std::round(static_cast<float>(outputWidth) / scale)
		);
		
		inputHeight = static_cast<uint32_t>(
				std::round(static_cast<float>(outputHeight) / scale)
		);
	}
	
	float getFSR2LodBias(FSR2QualityMode mode) {
		switch (mode) {
			case FSR2QualityMode::QUALITY:
				return -1.58f;
			case FSR2QualityMode::BALANCED:
				return -1.76f;
			case FSR2QualityMode::PERFORMANCE:
				return -2.0f;
			case FSR2QualityMode::ULTRA_PERFORMANCE:
				return -2.58f;
			default:
				return 0.0f;
		}
	}
	
	void FSR2Upscaling::createFSR2Context(uint32_t displayWidth,
									 uint32_t displayHeight,
									 uint32_t renderWidth,
									 uint32_t renderHeight) {
		m_description.displaySize.width = displayWidth;
		m_description.displaySize.height = displayHeight;
		
		m_description.maxRenderSize.width = renderWidth;
		m_description.maxRenderSize.height = renderHeight;
		
		m_description.flags = FFX_FSR2_ENABLE_AUTO_EXPOSURE;
		
		if (m_hdr) {
			m_description.flags |= FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE;
		}
		
		assert(ffxFsr2ContextCreate(&m_context, &m_description) == FFX_OK);
	}
	
	void FSR2Upscaling::destroyFSR2Context() {
		m_core.getContext().getDevice().waitIdle();
		
		assert(ffxFsr2ContextDestroy(&m_context) == FFX_OK);
		
		m_frameIndex = 0;
	}
	
	FSR2Upscaling::FSR2Upscaling(Core &core) : Upscaling(core) {
		const auto& physicalDevice = core.getContext().getPhysicalDevice();
		
		memset(&m_description, 0, sizeof(m_description));
		
		m_scratchBuffer.resize(ffxFsr2GetScratchMemorySizeVK(physicalDevice));
		
		assert(ffxFsr2GetInterfaceVK(
				&(m_description.callbacks),
				m_scratchBuffer.data(),
				m_scratchBuffer.size(),
				physicalDevice,
				vkGetDeviceProcAddr
		) == FFX_OK);
		
		m_description.device = ffxGetDeviceVK(core.getContext().getDevice());
		
		createFSR2Context(0, 0, 0, 0);
	}
	
	FSR2Upscaling::~FSR2Upscaling() {
		destroyFSR2Context();
		
		m_scratchBuffer.clear();
		m_description.callbacks.scratchBuffer = nullptr;
	}
	
	void FSR2Upscaling::update(float deltaTime, bool reset) {
		if (reset) {
			m_frameIndex = 0;
		}
		
		m_frameDeltaTime = deltaTime;
		m_reset = reset;
	}
	
	void FSR2Upscaling::calcJitterOffset(uint32_t renderWidth,
										 uint32_t renderHeight,
										 float &jitterOffsetX,
										 float &jitterOffsetY) {
		const int32_t phaseCount = ffxFsr2GetJitterPhaseCount(
				renderWidth,
				renderHeight
		);
		
		const int32_t phaseIndex = (static_cast<int32_t>(m_frameIndex) % phaseCount);
		
		assert(ffxFsr2GetJitterOffset(
				&jitterOffsetX,
				&jitterOffsetY,
				phaseIndex,
				phaseCount
		) == FFX_OK);
	}
	
	void FSR2Upscaling::bindDepthBuffer(const ImageHandle &depthInput) {
		m_depth = depthInput;
	}
	
	void FSR2Upscaling::bindVelocityBuffer(const ImageHandle &velocityInput) {
		m_velocity = velocityInput;
	}
	
	void FSR2Upscaling::recordUpscaling(const CommandStreamHandle &cmdStream, const ImageHandle &colorInput,
										const ImageHandle &output) {
		FfxFsr2DispatchDescription dispatch;
		memset(&dispatch, 0, sizeof(dispatch));
		
		const uint32_t inputWidth = m_core.getImageWidth(colorInput);
		const uint32_t inputHeight = m_core.getImageHeight(colorInput);
		
		const uint32_t outputWidth = m_core.getImageWidth(output);
		const uint32_t outputHeight = m_core.getImageHeight(output);
		
		if ((m_description.displaySize.width != outputWidth) ||
			(m_description.displaySize.height != outputHeight) ||
			(m_description.maxRenderSize.width < inputWidth) ||
			(m_description.maxRenderSize.height < inputHeight) ||
			(m_hdr != ((m_description.flags & FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE) != 0))) {
			destroyFSR2Context();
			
			createFSR2Context(
					outputWidth,
					outputHeight,
					inputWidth,
					inputHeight
			);
		}
		
		const bool sharpeningEnabled = (
				(m_sharpness > +0.0f) &&
				((inputWidth < outputWidth) || (inputHeight < outputHeight))
		);
		
		dispatch.color = ffxGetTextureResourceVK(
				&m_context,
				m_core.getVulkanImage(colorInput),
				m_core.getVulkanImageView(colorInput),
				inputWidth,
				inputHeight,
				static_cast<VkFormat>(m_core.getImageFormat(colorInput))
		);
		
		dispatch.depth = ffxGetTextureResourceVK(
				&m_context,
				m_core.getVulkanImage(m_depth),
				m_core.getVulkanImageView(m_depth),
				m_core.getImageWidth(m_depth),
				m_core.getImageHeight(m_depth),
				static_cast<VkFormat>(m_core.getImageFormat(m_depth))
		);
		
		dispatch.motionVectors = ffxGetTextureResourceVK(
				&m_context,
				m_core.getVulkanImage(m_velocity),
				m_core.getVulkanImageView(m_velocity),
				m_core.getImageWidth(m_velocity),
				m_core.getImageHeight(m_velocity),
				static_cast<VkFormat>(m_core.getImageFormat(m_velocity))
		);
		
		dispatch.exposure = ffxGetTextureResourceVK(
				&m_context,
				nullptr,
				nullptr,
				1,
				1,
				VK_FORMAT_UNDEFINED
		);
		
		dispatch.reactive = ffxGetTextureResourceVK(
				&m_context,
				nullptr,
				nullptr,
				1,
				1,
				VK_FORMAT_UNDEFINED
		);
		
		dispatch.transparencyAndComposition = ffxGetTextureResourceVK(
				&m_context,
				nullptr,
				nullptr,
				1,
				1,
				VK_FORMAT_UNDEFINED
		);
		
		dispatch.output = ffxGetTextureResourceVK(
				&m_context,
				m_core.getVulkanImage(output),
				m_core.getVulkanImageView(output),
				outputWidth,
				outputHeight,
				static_cast<VkFormat>(m_core.getImageFormat(output))
		);
		
		calcJitterOffset(
				inputWidth,
				inputHeight,
				dispatch.jitterOffset.x,
				dispatch.jitterOffset.y
		);
		
		dispatch.motionVectorScale.x = static_cast<float>(inputWidth);
		dispatch.motionVectorScale.y = static_cast<float>(inputHeight);
		
		dispatch.renderSize.width = inputWidth;
		dispatch.renderSize.height = inputHeight;
		
		dispatch.enableSharpening = sharpeningEnabled;
		dispatch.sharpness = m_sharpness;
		
		dispatch.frameTimeDelta = m_frameDeltaTime * 1000.0f; // from seconds to milliseconds
		dispatch.preExposure = 1.0f;
		dispatch.reset = m_reset;
		
		dispatch.cameraNear = m_near;
		dispatch.cameraFar = m_far;
		dispatch.cameraFovAngleVertical = m_fov;
		
		m_core.recordCommandsToStream(cmdStream, [&dispatch](const vk::CommandBuffer& cmdBuffer) {
			dispatch.commandList = ffxGetCommandListVK(cmdBuffer);
		}, [&]() {
			assert(ffxFsr2ContextDispatch(
					&m_context,
					&dispatch
			) == FFX_OK);
			
			m_frameIndex++;
			m_reset = false;
		});
	}
	
	void FSR2Upscaling::setCamera(float near, float far, float fov) {
		m_near = near;
		m_far = far;
		m_fov = fov;
	}
	
	bool FSR2Upscaling::isHdrEnabled() const {
		return m_hdr;
	}
	
	void FSR2Upscaling::setHdrEnabled(bool enabled) {
		m_hdr = enabled;
	}
	
	float FSR2Upscaling::getSharpness() const {
		return m_sharpness;
	}
	
	void FSR2Upscaling::setSharpness(float sharpness) {
		m_sharpness = (sharpness < 0.0f ? 0.0f : (sharpness > 1.0f ? 1.0f : sharpness));
	}
	
}