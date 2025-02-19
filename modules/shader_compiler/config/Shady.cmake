
use_git_submodule("${vkcv_shader_compiler_lib_path}/shady" shady_status)

if (${shady_status})
	set(EXTERNAL_JSON_C ON CACHE INTERNAL "")
	set(EXTERNAL_SPIRV_HEADERS ON CACHE INTERNAL "")
	set(EXTERNAL_JSON_C_INCLUDE ${JSON_C_INCLUDE_DIRS} CACHE INTERNAL "")

	set(BUILD_RUNTIME OFF CACHE INTERNAL "")
	set(BUILD_SAMPLES OFF CACHE INTERNAL "")
	
	if (vkcv_build_attribute EQUAL "SHARED")
		set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
	else ()
		set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
	endif ()

	set(SHADY_ENABLE_JAVA_BINDINGS OFF CACHE INTERNAL "")

	add_subdirectory(${vkcv_shader_compiler_lib}/shady)
	
	if (vkcv_build_attribute EQUAL "SHARED")
		list(APPEND vkcv_shader_compiler_libraries shady runtime)
	else ()
		list(APPEND vkcv_shader_compiler_libraries common driver)
	endif ()

	list(APPEND vkcv_shader_compiler_includes ${vkcv_shader_compiler_lib}/shady/include)
endif ()
