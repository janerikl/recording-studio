find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)

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
target_compile_definitions(edit_commands_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(edit_commands_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
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
target_compile_definitions(effect_commands_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(effect_commands_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME EffectCommandsTests COMMAND effect_commands_tests)

add_executable(punch_recording_tests
    test_PunchRecording.cpp
    ${CMAKE_SOURCE_DIR}/src/command/CommandStack.cpp
)
target_include_directories(punch_recording_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(punch_recording_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(punch_recording_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME PunchRecordingTests COMMAND punch_recording_tests)

add_executable(playback_loop_tests test_PlaybackLoop.cpp)
target_include_directories(playback_loop_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(playback_loop_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PlaybackLoopTests COMMAND playback_loop_tests)

add_executable(record_routing_tests test_RecordRouting.cpp)
target_include_directories(record_routing_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(record_routing_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(record_routing_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
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

add_executable(popover_positioning_tests test_PopoverPositioning.cpp)
target_include_directories(popover_positioning_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(popover_positioning_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PopoverPositioningTests COMMAND popover_positioning_tests)

add_executable(timeline_ticks_tests test_TimelineTicks.cpp)
target_include_directories(timeline_ticks_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(timeline_ticks_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME TimelineTicksTests COMMAND timeline_ticks_tests)

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

add_executable(piano_key_map_tests test_PianoKeyMap.cpp)
target_include_directories(piano_key_map_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(piano_key_map_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PianoKeyMapTests COMMAND piano_key_map_tests)

add_executable(note_naming_tests test_NoteNaming.cpp)
target_include_directories(note_naming_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(note_naming_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME NoteNamingTests COMMAND note_naming_tests)

add_executable(metronome_math_tests test_MetronomeMath.cpp)
target_include_directories(metronome_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(metronome_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MetronomeMathTests COMMAND metronome_math_tests)

add_executable(metronome_tests test_Metronome.cpp)
target_include_directories(metronome_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(metronome_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MetronomeTests COMMAND metronome_tests)

add_executable(practice_exercise_tests test_PracticeExercise.cpp)
target_include_directories(practice_exercise_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(practice_exercise_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PracticeExerciseTests COMMAND practice_exercise_tests)

add_executable(practice_math_tests test_PracticeMath.cpp)
target_include_directories(practice_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(practice_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PracticeMathTests COMMAND practice_math_tests)

add_executable(gm_instruments_tests test_GMInstruments.cpp)
target_include_directories(gm_instruments_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gm_instruments_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME GMInstrumentsTests COMMAND gm_instruments_tests)

add_executable(gm_drum_map_tests test_GMDrumMap.cpp)
target_include_directories(gm_drum_map_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gm_drum_map_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME GMDrumMapTests COMMAND gm_drum_map_tests)

add_executable(synth_tests test_Synth.cpp)
target_include_directories(synth_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(synth_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(synth_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME SynthTests COMMAND synth_tests)

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

add_executable(bus_mix_math_tests test_BusMixMath.cpp)
target_include_directories(bus_mix_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(bus_mix_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME BusMixMathTests COMMAND bus_mix_math_tests)

add_executable(preview_playback_math_tests test_PreviewPlaybackMath.cpp)
target_include_directories(preview_playback_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(preview_playback_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME PreviewPlaybackMathTests COMMAND preview_playback_math_tests)

add_executable(session_mixer_tests
    test_SessionMixer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/SessionMixer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Mixer.cpp
)
target_include_directories(session_mixer_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(session_mixer_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(session_mixer_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME SessionMixerTests COMMAND session_mixer_tests)

add_executable(offline_renderer_tests
    test_OfflineRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/OfflineRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/SessionMixer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Mixer.cpp
)
target_include_directories(offline_renderer_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(offline_renderer_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(offline_renderer_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME OfflineRendererTests COMMAND offline_renderer_tests)

add_executable(track_kind_color_tests test_TrackKindColor.cpp)
target_include_directories(track_kind_color_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(track_kind_color_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(track_kind_color_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME TrackKindColorTests COMMAND track_kind_color_tests)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

add_executable(session_io_tests
    test_SessionIO.cpp
    ${CMAKE_SOURCE_DIR}/src/io/SessionIO.cpp
    ${CMAKE_SOURCE_DIR}/src/io/AudioFileIO.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/Effects.cpp
    ${CMAKE_SOURCE_DIR}/src/model/LibraryFolder.cpp
)
target_include_directories(session_io_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(session_io_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(session_io_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::SNDFILE PkgConfig::FLUIDSYNTH)
add_test(NAME SessionIOTests COMMAND session_io_tests)

add_executable(media_preview_toggle_math_tests test_MediaPreviewToggleMath.cpp)
target_include_directories(media_preview_toggle_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(media_preview_toggle_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME MediaPreviewToggleMathTests COMMAND media_preview_toggle_math_tests)

add_executable(rhythm_pattern_tests test_RhythmPattern.cpp)
target_include_directories(rhythm_pattern_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rhythm_pattern_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RhythmPatternTests COMMAND rhythm_pattern_tests)

add_executable(rhythm_math_tests test_RhythmMath.cpp)
target_include_directories(rhythm_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rhythm_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME RhythmMathTests COMMAND rhythm_math_tests)

add_executable(rhythm_click_track_tests
    test_RhythmClickTrack.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/RhythmClickTrack.cpp
)
target_include_directories(rhythm_click_track_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(rhythm_click_track_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(rhythm_click_track_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME RhythmClickTrackTests COMMAND rhythm_click_track_tests)

add_executable(edit_clipboard_tests test_EditClipboard.cpp)
target_include_directories(edit_clipboard_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(edit_clipboard_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME EditClipboardTests COMMAND edit_clipboard_tests)

add_executable(shortcut_manager_tests test_ShortcutManager.cpp ${CMAKE_SOURCE_DIR}/src/ui/ShortcutManager.cpp)
target_include_directories(shortcut_manager_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(shortcut_manager_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME ShortcutManagerTests COMMAND shortcut_manager_tests)

add_executable(dialog_geometry_tests test_DialogGeometry.cpp ${CMAKE_SOURCE_DIR}/src/ui/DialogGeometry.cpp)
target_include_directories(dialog_geometry_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(dialog_geometry_tests PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME DialogGeometryTests COMMAND dialog_geometry_tests)

add_executable(library_item_tests test_LibraryItem.cpp)
target_include_directories(library_item_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(library_item_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME LibraryItemTests COMMAND library_item_tests)

add_executable(library_folder_tests
    test_LibraryFolder.cpp
    ${CMAKE_SOURCE_DIR}/src/model/LibraryFolder.cpp
)
target_include_directories(library_folder_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(library_folder_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME LibraryFolderTests COMMAND library_folder_tests)

add_executable(library_filter_math_tests
    test_LibraryFilterMath.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LibraryFilterMath.cpp
)
target_include_directories(library_filter_math_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(library_filter_math_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME LibraryFilterMathTests COMMAND library_filter_math_tests)

add_executable(library_item_ops_tests
    test_LibraryItemOps.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/LibraryItemOps.cpp
)
target_include_directories(library_item_ops_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(library_item_ops_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME LibraryItemOpsTests COMMAND library_item_ops_tests)

add_executable(rhythm_piano_playback_tests
    test_RhythmPianoPlayback.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/RhythmClickTrack.cpp
)
target_include_directories(rhythm_piano_playback_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(rhythm_piano_playback_tests PRIVATE RSD_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(rhythm_piano_playback_tests PRIVATE Qt6::Core Qt6::Test PkgConfig::FLUIDSYNTH)
add_test(NAME RhythmPianoPlaybackTests COMMAND rhythm_piano_playback_tests)
