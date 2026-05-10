add_executable(caustic-cli main.cpp)
target_compile_features(caustic-cli PRIVATE cxx_std_20)

# Architectural invariant §1: CLI must link only core + the SVG renderer.
# No raylib, no rlImGui. Verified by building with CAUSTIC_BUILD_APP=OFF.
target_link_libraries(caustic-cli PRIVATE
    caustic::core
    caustic::render-svg
)
