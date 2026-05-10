add_executable(test_vec2 test_vec2.cpp)
target_link_libraries(test_vec2 PRIVATE
    caustic::core
    doctest::doctest
)

add_test(NAME test_vec2 COMMAND test_vec2)
