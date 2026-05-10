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
