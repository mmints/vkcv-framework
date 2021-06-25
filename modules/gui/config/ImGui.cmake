
if (EXISTS "${vkcv_gui_lib_path}/imgui")
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/backends/imgui_impl_glfw.cpp)
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/backends/imgui_impl_vulkan.cpp )
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/imgui.cpp)
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/imgui_draw.cpp)
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/imgui_demo.cpp)
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/imgui_tables.cpp)
	list(APPEND vkcv_imgui_sources ${vkcv_gui_lib_path}/imgui/imgui_widgets.cpp)
	
	list(APPEND vkcv_imgui_includes ${vkcv_gui_lib}/imgui)
	list(APPEND vkcv_imgui_includes ${vkcv_gui_lib}/imgui/backend)
	
	list(APPEND vkcv_gui_include ${vkcv_gui_lib})
else()
	message(WARNING "IMGUI is required..! Update the submodules!")
endif ()