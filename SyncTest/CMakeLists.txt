project(SyncTest)
add_executable(${PROJECT_NAME}
    main.cpp
    Shader.cpp Shader.h
    )
target_link_libraries(${PROJECT_NAME} PRIVATE
    glad
    sdl2
    sdl2main
    )
set_target_properties(${PROJECT_NAME}
    PROPERTIES WIN32_EXECUTABLE 1
    )

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${BUILDBIN}
    )
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
