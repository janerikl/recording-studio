cmake_minimum_required(VERSION 3.21)
project(recording_studio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(RTAUDIO REQUIRED IMPORTED_TARGET rtaudio)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

add_executable(recording_studio
    src/main.cpp
    src/audio/AudioEngine.cpp
    src/audio/Mixer.cpp
    src/audio/Effects.cpp
    src/command/CommandStack.cpp
    src/io/AudioFileIO.cpp
    src/io/SessionIO.cpp
    src/waveform/WaveformCache.cpp
    src/ui/MainWindow.cpp
    src/ui/WaveformWidget.cpp
    src/ui/ClipLaneWidget.cpp
    src/ui/TrackWidgets.cpp
    src/ui/TakeLaneWidget.cpp
    src/ui/TimelineView.cpp
    src/ui/TimeRulerWidget.cpp
    src/ui/LevelMeterWidget.cpp
    src/ui/SettingsDialog.cpp
    src/ui/MediaLibraryPanel.cpp
    src/ui/EffectsRackPanel.cpp
    src/ui/PianoKeyboardWidget.cpp
    src/ui/InstrumentPanel.cpp
    src/ui/PianoRollGridWidget.cpp
    src/ui/PianoRollPanel.cpp
    src/ui/AutomationLaneWidget.cpp
    src/ui/LoopBrowserPanel.cpp
)

target_include_directories(recording_studio PRIVATE src)

target_link_libraries(recording_studio PRIVATE
    Qt6::Widgets
    PkgConfig::RTAUDIO
    PkgConfig::SNDFILE
)

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
