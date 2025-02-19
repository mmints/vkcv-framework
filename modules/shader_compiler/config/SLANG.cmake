
use_git_submodule("${vkcv_shader_compiler_lib_path}/slang" slang_status)

if (${slang_status})
	set(SLANG_USE_SYSTEM_MINIZ ON CACHE INTERNAL "")
	set(SLANG_USE_SYSTEM_LZ4 ON CACHE INTERNAL "")
	set(SLANG_USE_SYSTEM_VULKAN_HEADERS ON CACHE INTERNAL "")
	set(SLANG_USE_SYSTEM_SPIRV_HEADERS ON CACHE INTERNAL "")
	set(SLANG_USE_SYSTEM_UNORDERED_DENSE ON CACHE INTERNAL "")

	set(SLANG_ENABLE_CUDA OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_OPTIX OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_NVAPI OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_XLIB OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_AFTERMATH OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_DX_ON_VK OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_SLANG_RHI OFF CACHE INTERNAL "")
	set(SLANG_EMBED_STDLIB_SOURCE ON CACHE INTERNAL "")
	set(SLANG_EMBED_STDLIB OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_FULL_IR_VALIDATION OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_IR_BREAK_ALLOC OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_ASAN OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_PREBUILT_BINARIES OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_GFX OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_SLANGD OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_SLANGC OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_SLANGRT OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_SLANG_GLSLANG OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_TESTS OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_EXAMPLES OFF CACHE INTERNAL "")
	set(SLANG_ENABLE_REPLAYER OFF CACHE INTERNAL "")
	set(SLANG_LIB_TYPE ${vkcv_build_attribute} CACHE INTERNAL "")
	set(SLANG_SPIRV_HEADERS_INCLUDE_DIR ${spirv_headers_include} CACHE INTERNAL "")

	add_subdirectory(${vkcv_shader_compiler_lib}/slang)

	set(slang_system_includes "")
	list(APPEND slang_system_includes ${spirv_headers_include})
	list(APPEND slang_system_includes ${unordered_dense_include})
	list(APPEND slang_system_includes ${lz4_include})

	target_include_directories(core SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(compiler-core SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-cpp-extractor SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-embed SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-generate SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-lookup-generator SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-capability-generator SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-spirv-embed-generator SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-bootstrap SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(prelude SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-capability-defs SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-capability-lookup SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-lookup-tables SYSTEM BEFORE PRIVATE ${slang_system_includes})
	target_include_directories(slang-no-embedded-stdlib SYSTEM BEFORE PRIVATE ${slang_system_includes})

	set(SLANG_STDLIB_META_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${vkcv_shader_compiler_lib}/slang/source/slang/stdlib-meta)

	target_include_directories(slang BEFORE PUBLIC ${SLANG_STDLIB_META_OUTPUT_DIR})
	
	list(APPEND vkcv_shader_compiler_libraries slang lz4_static miniz unordered_dense::unordered_dense)
	list(APPEND vkcv_shader_compiler_includes ${vkcv_shader_compiler_lib}/slang/include ${slang_system_includes})
endif ()
