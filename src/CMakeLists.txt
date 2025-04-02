file(GLOB PROJECT_MAIN *.cpp
                       *.h)

file(GLOB PROJECT_RENDERING rendering/*.cpp
                            rendering/*.h)

file(GLOB PROJECT_THIRDPARTY third-party/*.cpp
			     third-party/*.h)

file(GLOB PROJECT_SHADERS shaders/*.comp
                          shaders/*.frag
                          shaders/*.geom
                          shaders/*.vert)

source_group("Source Files/Rendering" FILES ${PROJECT_RENDERING})
source_group("Source Files/Shaders" FILES ${PROJECT_SHADERS})
source_group("Source Files/Third-party" FILES ${PROJECT_THIRDPARTY})


add_executable(main_program

  ${PROJECT_MAIN}

  ${PROJECT_RENDERING}

  ${PROJECT_THIRDPARTY}

  ${PROJECT_SHADERS}
)


target_link_libraries(main_program)


add_custom_command(
    TARGET main_program POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/src/shaders
)