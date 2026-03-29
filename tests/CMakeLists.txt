# Test Executables
# O target antigo test_dimension_core continua existindo para testes de estatos e inicialização
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

# Offline Runner Executable
add_executable(offline_runner offline_runner.c)
target_link_libraries(offline_runner PRIVATE dimension_core)
add_test(NAME offline_runner_smoke_test COMMAND offline_runner)

# Nova Suite de Testes do Core DSP
add_executable(test_dsp_core test_dsp_core.c)
target_link_libraries(test_dsp_core PRIVATE dimension_core)
add_test(NAME dsp_core_mathematical_tests COMMAND test_dsp_core)
