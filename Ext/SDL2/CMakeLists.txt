project(sdl2)

add_library(${PROJECT_NAME} SHARED IMPORTED GLOBAL)
set_property(TARGET ${PROJECT_NAME} PROPERTY IMPORTED_IMPLIB ${CMAKE_CURRENT_SOURCE_DIR}/msvc/lib/SDL2.lib)
set_property(TARGET ${PROJECT_NAME} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/msvc/bin/SDL2.dll)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/msvc/include)

add_custom_target(CopySdl2ToBuild ALL)
add_custom_command(TARGET CopySdl2ToBuild POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${PROJECT_NAME}> ${BUILDBIN}
    )

install(FILES $<TARGET_FILE:${PROJECT_NAME}> DESTINATION bin)

add_library(sdl2main STATIC IMPORTED GLOBAL)
set_property(TARGET sdl2main PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/msvc/lib/SDL2main.lib)
