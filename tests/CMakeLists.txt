add_executable(test_caustic
    test_main.cpp
    test_vec2.cpp
    test_circle.cpp
    test_modular_chord.cpp
    test_sampler.cpp
    test_hypotrochoid.cpp
    test_epitrochoid.cpp
    test_lissajous.cpp
    test_color.cpp
    test_indexer.cpp
    test_preset_io.cpp
)

target_link_libraries(test_caustic PRIVATE
    caustic::core
    doctest::doctest
)

add_test(NAME test_caustic COMMAND test_caustic)
