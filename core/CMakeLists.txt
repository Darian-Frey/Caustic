add_library(caustic-core INTERFACE)
add_library(caustic::core ALIAS caustic-core)

target_include_directories(caustic-core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(caustic-core INTERFACE cxx_std_20)

target_link_libraries(caustic-core INTERFACE nlohmann_json::nlohmann_json)
