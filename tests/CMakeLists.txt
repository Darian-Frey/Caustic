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
    test_svg_renderer.cpp
    test_array_tools.cpp
    test_rose.cpp
    test_superformula.cpp
    test_phyllotaxis.cpp
    test_polygon.cpp
    test_linear_envelope.cpp
    test_envelope.cpp
    test_attractors.cpp
    test_diamond_stack.cpp
    test_custom_chord.cpp
    test_hybrids.cpp
)

target_link_libraries(test_caustic PRIVATE
    caustic::core
    caustic::render-svg
    doctest::doctest
)

add_test(NAME test_caustic COMMAND test_caustic)

# ---------------------------------------------------------------------------
# CLI smoke tests — exercise the binary end-to-end on bundled presets and
# the SPEC.md §5 error exit codes.

if(TARGET caustic-cli)
    add_test(NAME cli_version COMMAND $<TARGET_FILE:caustic-cli> --version)
    set_tests_properties(cli_version PROPERTIES
        PASS_REGULAR_EXPRESSION "caustic-cli")

    add_test(NAME cli_help COMMAND $<TARGET_FILE:caustic-cli> --help)
    set_tests_properties(cli_help PROPERTIES
        PASS_REGULAR_EXPRESSION "Usage: caustic-cli")

    # Missing required args → exit 1.
    add_test(NAME cli_missing_args COMMAND $<TARGET_FILE:caustic-cli>)
    set_tests_properties(cli_missing_args PROPERTIES WILL_FAIL TRUE)

    # Preset file not found → exit 2.
    add_test(NAME cli_file_not_found
             COMMAND $<TARGET_FILE:caustic-cli> /no/such/preset.json -o ${CMAKE_BINARY_DIR}/x.svg)
    set_tests_properties(cli_file_not_found PROPERTIES WILL_FAIL TRUE)

    # Render every bundled preset.
    foreach(preset cardioid_classic times_tables_51 spirograph_classic cardioid_epi bowtie
                   rose_five_petal superformula_starfish phyllotaxis_sunflower
                   corner_fan envelope_bowtie deltoid hexagram four_bowties rgb_triangle
                   clifford_butterfly de_jong_classic tinkerbell
                   swoop_pair bowtie_hourglass
                   diamond_stack_classic diamond_stack_two_colour
                   diamond_star_neon square_grid_2x3
                   custom_starburst
                   maurer_rose_classic lissajous_chord_classic
                   superformula_chord_starfish)
        add_test(
            NAME cli_render_${preset}
            COMMAND $<TARGET_FILE:caustic-cli>
                    ${CMAKE_SOURCE_DIR}/presets/${preset}.json
                    -o ${CMAKE_BINARY_DIR}/cli_render_${preset}.svg
        )
    endforeach()

    # Plotter-mode smoke check.
    add_test(
        NAME cli_render_plotter
        COMMAND $<TARGET_FILE:caustic-cli>
                ${CMAKE_SOURCE_DIR}/presets/spirograph_classic.json
                --plotter
                -o ${CMAKE_BINARY_DIR}/cli_plotter.svg
    )
endif()
