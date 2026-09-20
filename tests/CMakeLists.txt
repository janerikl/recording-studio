find_package(Qt6 COMPONENTS Core Test REQUIRED)

# Model/command tests are pure Core (no Widgets/RtAudio/sndfile needed), so
# they build fast and don't require a display to run.
add_executable(command_stack_tests
    test_CommandStack.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandStack.cpp
)
target_include_directories(command_stack_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(command_stack_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME CommandStackTests COMMAND command_stack_tests)

add_executable(edit_commands_tests
    test_EditCommands.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandStack.cpp
)
target_include_directories(edit_commands_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(edit_commands_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME EditCommandsTests COMMAND edit_commands_tests)

add_executable(mixer_tests
    test_Mixer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Mixer.cpp
)
target_include_directories(mixer_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mixer_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MixerTests COMMAND mixer_tests)

add_executable(effects_tests
    test_Effects.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
)
target_include_directories(effects_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(effects_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME EffectsTests COMMAND effects_tests)

add_executable(effect_commands_tests
    test_EffectCommands.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandStack.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
)
target_include_directories(effect_commands_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(effect_commands_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME EffectCommandsTests COMMAND effect_commands_tests)

add_executable(punch_recording_tests
    test_PunchRecording.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandStack.cpp
)
target_include_directories(punch_recording_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(punch_recording_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PunchRecordingTests COMMAND punch_recording_tests)

add_executable(clip_edit_math_tests test_ClipEditMath.cpp)
target_include_directories(clip_edit_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(clip_edit_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ClipEditMathTests COMMAND clip_edit_math_tests)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

add_executable(session_io_tests
    test_SessionIO.cpp
    ${CMAKE_SOURCE_DIR}/src/io/SessionIO.cpp
    ${CMAKE_SOURCE_DIR}/src/io/AudioFileIO.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
)
target_include_directories(session_io_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(session_io_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::SNDFILE)
add_test(NAME SessionIOTests COMMAND session_io_tests)
