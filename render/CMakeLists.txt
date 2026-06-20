# SVG renderer + plotter (G-code / HPGL) renderer — always available,
# headless, no raylib dependency. The CLI in Phase 7 links against this
# without pulling raylib in.
add_library(caustic-render-svg STATIC
    svg_renderer.cpp
    plotter_renderer.cpp
)
add_library(caustic::render-svg ALIAS caustic-render-svg)

target_include_directories(caustic-render-svg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(caustic-render-svg PUBLIC
    caustic::core
)

# Raylib renderer — only when building the GUI app.
if(CAUSTIC_BUILD_APP)
    add_library(caustic-render-raylib STATIC
        raylib_renderer.cpp
    )
    add_library(caustic::render-raylib ALIAS caustic-render-raylib)

    target_include_directories(caustic-render-raylib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_link_libraries(caustic-render-raylib PUBLIC
        caustic::core
        raylib
    )
endif()
