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

add_executable(record_routing_tests test_RecordRouting.cpp)
target_include_directories(record_routing_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(record_routing_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RecordRoutingTests COMMAND record_routing_tests)

add_executable(system_audio_loopback_tests test_SystemAudioLoopback.cpp)
target_include_directories(system_audio_loopback_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(system_audio_loopback_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SystemAudioLoopbackTests COMMAND system_audio_loopback_tests)

add_executable(clip_edit_math_tests test_ClipEditMath.cpp)
target_include_directories(clip_edit_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(clip_edit_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ClipEditMathTests COMMAND clip_edit_math_tests)

add_executable(waveform_display_math_tests test_WaveformDisplayMath.cpp)
target_include_directories(waveform_display_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(waveform_display_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME WaveformDisplayMathTests COMMAND waveform_display_math_tests)

add_executable(timeline_scale_math_tests test_TimelineScaleMath.cpp)
target_include_directories(timeline_scale_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(timeline_scale_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TimelineScaleMathTests COMMAND timeline_scale_math_tests)

add_executable(track_effects_label_tests test_TrackEffectsLabel.cpp)
target_include_directories(track_effects_label_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(track_effects_label_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TrackEffectsLabelTests COMMAND track_effects_label_tests)

add_executable(clip_lane_scroll_math_tests test_ClipLaneScrollMath.cpp)
target_include_directories(clip_lane_scroll_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(clip_lane_scroll_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME ClipLaneScrollMathTests COMMAND clip_lane_scroll_math_tests)

add_executable(punch_take_math_tests test_PunchTakeMath.cpp)
target_include_directories(punch_take_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(punch_take_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PunchTakeMathTests COMMAND punch_take_math_tests)

add_executable(comp_math_tests test_CompMath.cpp)
target_include_directories(comp_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(comp_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME CompMathTests COMMAND comp_math_tests)

add_executable(synth_math_tests test_SynthMath.cpp)
target_include_directories(synth_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(synth_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME SynthMathTests COMMAND synth_math_tests)

add_executable(midi_note_display_math_tests test_MidiNoteDisplayMath.cpp)
target_include_directories(midi_note_display_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(midi_note_display_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MidiNoteDisplayMathTests COMMAND midi_note_display_math_tests)

add_executable(piano_roll_edit_math_tests test_PianoRollEditMath.cpp)
target_include_directories(piano_roll_edit_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(piano_roll_edit_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PianoRollEditMathTests COMMAND piano_roll_edit_math_tests)

add_executable(pan_law_math_tests test_PanLawMath.cpp)
target_include_directories(pan_law_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pan_law_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PanLawMathTests COMMAND pan_law_math_tests)

add_executable(automation_math_tests test_AutomationMath.cpp)
target_include_directories(automation_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(automation_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME AutomationMathTests COMMAND automation_math_tests)

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
