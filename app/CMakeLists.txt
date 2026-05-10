add_executable(caustic
    main.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)

target_include_directories(caustic PRIVATE ${rlimgui_SOURCE_DIR})

target_link_libraries(caustic PRIVATE
    caustic::core
    caustic::render-raylib
    raylib
    imgui
)
