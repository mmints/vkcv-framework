
use_git_submodule("${vkcv_lib_path}/VulkanMemoryAllocator-Hpp" vma_hpp_status)

if (${vma_hpp_status})
	set(VMA_HPP_PATH "${vkcv_lib_path}/VulkanMemoryAllocator-Hpp" CACHE INTERNAL "")
	
	set(VMA_RECORDING_ENABLED OFF CACHE INTERNAL "")
	set(VMA_USE_STL_CONTAINERS OFF CACHE INTERNAL "")
	set(VMA_STATIC_VULKAN_FUNCTIONS ON CACHE INTERNAL "")
	set(VMA_DYNAMIC_VULKAN_FUNCTIONS OFF CACHE INTERNAL "")
	set(VMA_DEBUG_ALWAYS_DEDICATED_MEMORY OFF CACHE INTERNAL "")
	set(VMA_DEBUG_INITIALIZE_ALLOCATIONS OFF CACHE INTERNAL "")
	set(VMA_DEBUG_GLOBAL_MUTEX OFF CACHE INTERNAL "")
	set(VMA_DEBUG_DONT_EXCEED_MAX_MEMORY_ALLOCATION_COUNT OFF CACHE INTERNAL "")
	
	add_subdirectory(${vkcv_config_lib}/vma)
	
	list(APPEND vkcv_libraries VulkanMemoryAllocator)
	list(APPEND vkcv_includes ${vkcv_lib_path}/VulkanMemoryAllocator-Hpp)
	
	message(${vkcv_config_msg} " VMA     - ")
endif ()
