file(GLOB file_src_list "*.c" "*.cpp")

foreach(file_src ${file_src_list})
	get_filename_component(targetname ${file_src} NAME)
	add_executable(${targetname} ${file_src})
	
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/itu)
	target_include_directories(${targetname} PRIVATE ${CMAKE_SOURCE_DIR}/lib/imgui)

	target_include_directories(${targetname} PRIVATE include/SDL)
	target_link_libraries(${targetname} PRIVATE SDL3::SDL3)
	target_link_libraries(${targetname} PRIVATE SDL3_mixer::SDL3_mixer)
	target_link_libraries(${targetname} PRIVATE box2d::box2d)
	target_link_libraries(${targetname} PRIVATE imgui)

	if (WIN32)
        add_custom_command(TARGET ${targetname} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${targetname}> $<TARGET_FILE_DIR:${targetname}>
            COMMAND_EXPAND_LISTS)
    endif()
endforeach()
