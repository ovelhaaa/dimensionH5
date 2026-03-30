# Test Executables
add_executable(test_audio_engine test_main.c test_audio_engine.c)
target_link_libraries(test_audio_engine PRIVATE dimension_core)
add_test(NAME audio_engine_tests COMMAND test_audio_engine)

# Offline Runner Executable
add_executable(offline_runner offline_runner.c test_signals.c wav_writer.c wav_reader.c)
target_link_libraries(offline_runner PRIVATE dimension_core)
add_test(NAME offline_runner_smoke_test COMMAND offline_runner)

# Nova Suite de Testes do Core DSP
add_executable(test_dsp_core test_dsp_core.c test_signals.c)
target_link_libraries(test_dsp_core PRIVATE dimension_core)
add_test(NAME dsp_core_mathematical_tests COMMAND test_dsp_core)
