project(glad)

add_library(${PROJECT_NAME} STATIC
    include/glad/gl.h
    include/KHR/khrplatform.h
    src/gl.c
    )

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PUBLIC opengl32)
