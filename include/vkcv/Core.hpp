#pragma once
/**
 * @file src/vkcv/Core.hpp
 * @brief Handling of global states regarding dependencies
 */

#include <memory>
#include <vulkan/vulkan.hpp>

#include "Context.hpp"
#include "Swapchain.hpp"
#include "Window.hpp"
#include "PassConfig.hpp"
#include "Handles.hpp"
#include "Buffer.hpp"
#include "Image.hpp"
#include "GraphicsPipelineConfig.hpp"
#include "ComputePipelineConfig.hpp"
#include "CommandResources.hpp"
#include "SyncResources.hpp"
#include "Result.hpp"
#include "vkcv/DescriptorConfig.hpp"
#include "Sampler.hpp"
#include "DescriptorWrites.hpp"
#include "Event.hpp"
#include "DrawcallRecording.hpp"
#include "CommandRecordingFunctionTypes.hpp"
#include "../../src/vkcv/WindowManager.hpp"
#include "../../src/vkcv/SwapchainManager.hpp"


namespace vkcv
{

    // forward declarations
    class PassManager;
    class GraphicsPipelineManager;
    class ComputePipelineManager;
    class DescriptorManager;
    class BufferManager;
    class SamplerManager;
    class ImageManager;
	class CommandStreamManager;
	class WindowManager;
	class SwapchainManager;

	struct SubmitInfo {
		QueueType queueType;
		std::vector<vk::Semaphore> waitSemaphores;
		std::vector<vk::Semaphore> signalSemaphores;
	};

    class Core final
    {
    private:

        /**
         * Constructor of #Core requires an @p context.
         *
         * @param context encapsulates various Vulkan objects
         */
        Core(Context &&context, const CommandResources& commandResources, const SyncResources& syncResources) noexcept;
        // explicit destruction of default constructor
        Core() = delete;

		Result acquireSwapchainImage(const SwapchainHandle &swapchainHandle);

        Context m_Context;

        std::unique_ptr<PassManager>             m_PassManager;
        std::unique_ptr<GraphicsPipelineManager> m_PipelineManager;
        std::unique_ptr<ComputePipelineManager>  m_ComputePipelineManager;
        std::unique_ptr<DescriptorManager>       m_DescriptorManager;
        std::unique_ptr<BufferManager>           m_BufferManager;
        std::unique_ptr<SamplerManager>          m_SamplerManager;
        std::unique_ptr<ImageManager>            m_ImageManager;
        std::unique_ptr<CommandStreamManager>    m_CommandStreamManager;
        std::unique_ptr<WindowManager>           m_WindowManager;
        std::unique_ptr<SwapchainManager>        m_SwapchainManager;

		CommandResources    m_CommandResources;
		SyncResources       m_SyncResources;
		uint32_t            m_currentSwapchainImageIndex;

		/**
		 * sets up swapchain images
		 * @param swapchainHandles of swapchain
		 */
		void setSwapchainImages(SwapchainHandle handle);

    public:
        /**
         * Destructor of #Core destroys the Vulkan objects contained in the core's context.
         */
        ~Core() noexcept;

        /**
         * Copy-constructor of #Core is deleted!
         *
         * @param other Other instance of #Context
         */
        Core(const Core& other) = delete;

        /**
         * Move-constructor of #Core uses default behavior!
         *
         * @param other Other instance of #Context
         */
        Core(Core &&other) = delete; // move-ctor

        /**
         * Copy assignment operator of #Core is deleted!
         *
         * @param other Other instance of #Context
         * @return Reference to itself
         */
        Core & operator=(const Core &other) = delete;

        /**
         * Move assignment operator of #Core uses default behavior!
         *
         * @param other Other instance of #Context
         * @return Reference to itself
         */
        Core & operator=(Core &&other) = delete;

        [[nodiscard]]
        const Context &getContext() const;

        /**
             * Creates a #Core with given @p applicationName and @p applicationVersion for your application.
             *
             * It is also possible to require a specific amount of queues, ask for specific queue-flags or
             * extensions. This function will take care of the required arguments as best as possible.
             *
             * To pass a valid version for your application, you should use #VK_MAKE_VERSION().
             *
             * @param[in] applicationName Name of the application
             * @param[in] applicationVersion Version of the application
             * @param[in] queueFlags (optional) Requested flags of queues
             * @param[in] instanceExtensions (optional) Requested instance extensions
             * @param[in] deviceExtensions (optional) Requested device extensions
             * @return New instance of #Context
             */
        static Core create(const char *applicationName,
                           uint32_t applicationVersion,
                           const std::vector<vk::QueueFlagBits>& queueFlags    = {},
						   const Features& features = {},
						   const std::vector<const char *>& instanceExtensions = {});

        /**
         * Creates a basic vulkan graphics pipeline using @p config from the pipeline config class and returns it using the @p handle.
         * Fixed Functions for pipeline are set with standard values.
         *
         * @param config a pipeline config object from the pipeline config class
         * @param handle a handle to return the created vulkan handle
         * @return True if pipeline creation was successful, False if not
         */
        [[nodiscard]]
		GraphicsPipelineHandle createGraphicsPipeline(const GraphicsPipelineConfig &config);

        /**
         * Creates a basic vulkan compute pipeline using @p shader program and returns it using the @p handle.
         * Fixed Functions for pipeline are set with standard values.
         *
         * @param config Contains the compiles compute shader and the corresponding descriptor set layout
         * @return True if pipeline creation was successful, False if not
         */
        [[nodiscard]]
        ComputePipelineHandle createComputePipeline(const ComputePipelineConfig &config);

        /**
         * Creates a basic vulkan render pass using @p config from the render pass config class and returns it using the @p handle.
         * Fixed Functions for pipeline are set with standard values.
         *
         * @param config a render pass config object from the render pass config class
         * @param handle a handle to return the created vulkan handle
         * @return True if render pass creation was successful, False if not
         */
        [[nodiscard]]
        PassHandle createPass(const PassConfig &config);

        /**
            * Creates a #Buffer with data-type T and @p bufferType
            * @param type Type of Buffer created
            * @param count Count of elements of type T
            * @param memoryType Type of Buffers memory
            * return Buffer-Object
            */
        template<typename T>
        Buffer<T> createBuffer(vkcv::BufferType type, size_t count, BufferMemoryType memoryType = BufferMemoryType::DEVICE_LOCAL, bool supportIndirect = false) {
        	return Buffer<T>::create(m_BufferManager.get(), type, count, memoryType, supportIndirect);
        }
        
        /**
         * Creates a Sampler with given attributes.
         *
         * @param magFilter Magnifying filter
         * @param minFilter Minimizing filter
         * @param mipmapMode Mipmapping filter
         * @param addressMode Address mode
         * @param mipLodBias Mip level of detail bias
         * @return Sampler handle
         */
        [[nodiscard]]
        SamplerHandle createSampler(SamplerFilterType magFilter, SamplerFilterType minFilter,
									SamplerMipmapMode mipmapMode, SamplerAddressMode addressMode,
									float mipLodBias = 0.0f);

        /**
         * Creates an #Image with a given format, width, height and depth.
         *
         * @param format Image format
         * @param width Image width
         * @param height Image height
         * @param depth Image depth
         * @return Image-Object
         */
        [[nodiscard]]
        Image createImage(
			vk::Format      format,
			uint32_t        width,
			uint32_t        height,
			uint32_t        depth = 1,
			bool            createMipChain = false,
			bool            supportStorage = false,
			bool            supportColorAttachment = false,
			Multisampling   multisampling = Multisampling::None);

        /**
         * creates a new window and returns it's handle
         * @param applicationName window name
         * @param windowWidth
         * @param windowHeight
         * @param resizeable resizeability bool
         * @return windowHandle
         */
		[[nodiscard]]
		WindowHandle createWindow(
				const char *applicationName,
				uint32_t windowWidth,
				uint32_t windowHeight,
				bool resizeable);

		/**
		 * getter for window reference
		 * @param handle of the window
		 * @return the window
		 */
		[[nodiscard]]
		Window& getWindow(const WindowHandle& handle );

		/**
		 * gets the swapchain of the current focused window
		 * @return swapchain
		 */
		[[nodiscard]]
		Swapchain& getSwapchainOfCurrentWindow();

		/**
		 * returns the swapchain reference
		 * @param handle of the swapchain
		 * @return swapchain
		 */
		[[nodiscard]]
		Swapchain& getSwapchain(const SwapchainHandle& handle);

		/**
		 * gets the swapchain handle from the window
		 * @param handle of the window
		 * @return the swapchain from getSwapchain( SwapchainHandle )
		 */
		[[nodiscard]]
		Swapchain& getSwapchain(const WindowHandle& handle);

		/**
		 * returns the image width
		 * @param image handle
		 * @return imageWidth
		 */
        [[nodiscard]]
        uint32_t getImageWidth(const ImageHandle& image);

        /**
         * returns the image height
         * @param image handle
         * @return imageHeight
         */
        [[nodiscard]]
        uint32_t getImageHeight(const ImageHandle& image);

        /**
         * returns the image format of the image
         * @param image handle
         * @return imageFormat
         */
		[[nodiscard]]
		vk::Format getImageFormat(const ImageHandle& image);

		/** TODO:
		 * @param bindings
		 * @return
		 */
		[[nodiscard]]
		DescriptorSetLayoutHandle createDescriptorSetLayout(const std::unordered_map<uint32_t, DescriptorBinding> &bindingsMap);
		DescriptorSetLayout getDescriptorSetLayout(const DescriptorSetLayoutHandle handle) const;

		// TODO: existsDescriptorSetLayout function that checks and returns fitting layout upon existence.

        /** TODO:
         *   @param setDescriptions
         *   @return
         */
        [[nodiscard]]
        DescriptorSetHandle createDescriptorSet(const DescriptorSetLayoutHandle &layoutHandle);
		void writeDescriptorSet(DescriptorSetHandle handle, const DescriptorWrites& writes);
		DescriptorSet getDescriptorSet(const DescriptorSetHandle handle) const;


		/**
		 * @brief start recording command buffers and increment frame index
		*/
		bool beginFrame(uint32_t& width, uint32_t& height, const WindowHandle &windowHandle);

		void recordDrawcallsToCmdStream(
			const CommandStreamHandle&      cmdStreamHandle,
			const PassHandle&               renderpassHandle,
			const GraphicsPipelineHandle    &pipelineHandle,
			const PushConstants             &pushConstants,
			const std::vector<DrawcallInfo> &drawcalls,
			const std::vector<ImageHandle>  &renderTargets,
			const WindowHandle              &windowHandle);

		void recordMeshShaderDrawcalls(
			const CommandStreamHandle&              cmdStreamHandle,
			const PassHandle&                       renderpassHandle,
			const GraphicsPipelineHandle            &pipelineHandle,
			const PushConstants&                    pushConstantData,
            const std::vector<MeshShaderDrawcall>&  drawcalls,
			const std::vector<ImageHandle>&         renderTargets,
			const WindowHandle&                     windowHandle);

		void recordComputeDispatchToCmdStream(
			CommandStreamHandle cmdStream,
            ComputePipelineHandle computePipeline,
			const uint32_t dispatchCount[3],
			const std::vector<DescriptorSetUsage> &descriptorSetUsages,
			const PushConstants& pushConstants);
		
		void recordBeginDebugLabel(const CommandStreamHandle &cmdStream,
								   const std::string& label,
								   const std::array<float, 4>& color);
		
		void recordEndDebugLabel(const CommandStreamHandle &cmdStream);

		void recordComputeIndirectDispatchToCmdStream(
			const CommandStreamHandle               cmdStream,
			const ComputePipelineHandle             computePipeline,
			const vkcv::BufferHandle                buffer,
			const size_t                            bufferArgOffset,
			const std::vector<DescriptorSetUsage>&  descriptorSetUsages,
			const PushConstants&                    pushConstants);

		/**
		 * @brief end recording and present image
		*/
		void endFrame( const WindowHandle& windowHandle );

		/**
		 * Submit a command buffer to any queue of selected type. The recording can be customized by a
		 * custom record-command-function. If the command submission has finished, an optional finish-function
		 * will be called.
		 *
		 * @param submitInfo Submit information
		 * @param record Record-command-function
		 * @param finish Finish-command-function or nullptr
		 */
		void recordAndSubmitCommandsImmediate(
			const SubmitInfo            &submitInfo, 
			const RecordCommandFunction &record, 
			const FinishCommandFunction &finish);

		CommandStreamHandle createCommandStream(QueueType queueType);

		void recordCommandsToStream(
			const CommandStreamHandle   cmdStreamHandle,
			const RecordCommandFunction &record,
			const FinishCommandFunction &finish);

		void submitCommandStream(const CommandStreamHandle& handle);
		void prepareSwapchainImageForPresent(const CommandStreamHandle& handle);
		void prepareImageForSampling(const CommandStreamHandle& cmdStream, const ImageHandle& image);
		void prepareImageForStorage(const CommandStreamHandle& cmdStream, const ImageHandle& image);

		// normally layout transitions for attachments are handled by the core
		// however for manual vulkan use, e.g. ImGui integration, this function is exposed
		// this is also why the command buffer is passed directly, instead of the command stream handle
		void prepareImageForAttachmentManually(const vk::CommandBuffer& cmdBuffer, const ImageHandle& image);

		// if manual vulkan work, e.g. ImGui integration, changes an image layout this function must be used
		// to update the internal image state
		void updateImageLayoutManual(const vkcv::ImageHandle& image, const vk::ImageLayout layout);

		void recordImageMemoryBarrier(const CommandStreamHandle& cmdStream, const ImageHandle& image);
		void recordBufferMemoryBarrier(const CommandStreamHandle& cmdStream, const BufferHandle& buffer);
		void resolveMSAAImage(const CommandStreamHandle& cmdStream, const ImageHandle& src, const ImageHandle& dst);

		[[nodiscard]]
		vk::ImageView getSwapchainImageView() const;
	
		void recordMemoryBarrier(const CommandStreamHandle& cmdStream);
		
		void recordBlitImage(const CommandStreamHandle& cmdStream, const ImageHandle& src, const ImageHandle& dst,
							 SamplerFilterType filterType);
	
		void setDebugLabel(const BufferHandle &handle, const std::string &label);
		void setDebugLabel(const PassHandle &handle, const std::string &label);
		void setDebugLabel(const GraphicsPipelineHandle &handle, const std::string &label);
		void setDebugLabel(const ComputePipelineHandle &handle, const std::string &label);
		void setDebugLabel(const DescriptorSetHandle &handle, const std::string &label);
		void setDebugLabel(const SamplerHandle &handle, const std::string &label);
		void setDebugLabel(const ImageHandle &handle, const std::string &label);
		void setDebugLabel(const CommandStreamHandle &handle, const std::string &label);
		
    };
}
