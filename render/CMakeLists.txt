add_library(caustic-render INTERFACE)
add_library(caustic::render ALIAS caustic-render)

target_include_directories(caustic-render INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(caustic-render INTERFACE caustic::core)
