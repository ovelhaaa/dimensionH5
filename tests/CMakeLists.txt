# Test Executables
add_executable(test_dimension_core test_main.c test_dimension_core.c)
target_link_libraries(test_dimension_core PRIVATE dimension_core)
add_test(NAME dimension_core_tests COMMAND test_dimension_core)

add_executable(test_interp test_main.c test_interp.c)
target_link_libraries(test_interp PRIVATE dimension_core)
add_test(NAME interp_tests COMMAND test_interp)

add_executable(test_biquad test_main.c test_biquad.c)
target_link_libraries(test_biquad PRIVATE dimension_core)
add_test(NAME biquad_tests COMMAND test_biquad)

add_executable(test_audio_engine test_main.c test_audio_engine.c)
target_link_libraries(test_audio_engine PRIVATE dimension_core)
add_test(NAME audio_engine_tests COMMAND test_audio_engine)

# WAV Renderer Executable
add_executable(render_wav render_wav_main.c wav_writer.c)
target_link_libraries(render_wav PRIVATE dimension_core)
add_test(NAME render_wav_smoke_test COMMAND render_wav)
