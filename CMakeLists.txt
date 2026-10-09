cmake_minimum_required(VERSION 3.22)

project(StemDeck VERSION 1.0.0)

# ccache, when it is installed (2026-10-03): a fresh worktree rebuilt all of
# JUCE and every source -- some twenty minutes on the rig. The base directory
# makes the paths of different worktrees compare equal, so one worktree's
# compile is another's cache hit.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    # The checkout is the base: seen from its build/ every source path is the
    # same in every worktree, and JUCE, outside it, stays absolute and equal.
    set(CCACHE_LAUNCHER "${CMAKE_COMMAND}" -E env "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
        "CCACHE_NOHASHDIR=true"
        # What ccache needs to cache a compile that reads a precompiled header.
        "CCACHE_SLOPPINESS=pch_defines,time_macros,include_file_mtime,include_file_ctime"
        "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_LAUNCHER})
endif()

find_package(JUCE CONFIG REQUIRED)

# Distro JUCE packages (e.g. Debian's "+ds" build) strip the bundled FLAC/Ogg
# codecs and expect the system libraries to be linked instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUDIO_CODECS REQUIRED IMPORTED_TARGET flac vorbisenc vorbisfile vorbis ogg)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)

juce_add_gui_app(StemDeck
    PRODUCT_NAME "StemDeck")

juce_generate_juce_header(StemDeck)

# A precompiled JuceHeader.h (2026-10-03): every source parsed all of JUCE's
# headers anew. JUCE's own module units are left out: they set JUCE's
# configuration before they include it, and a header put in front of them
# would come first.
get_target_property(juce_core_sources juce::juce_core INTERFACE_SOURCES)
list(GET juce_core_sources 0 juce_core_unit)
get_filename_component(juce_modules_dir "${juce_core_unit}" DIRECTORY)
get_filename_component(juce_modules_dir "${juce_modules_dir}" DIRECTORY)
file(GLOB juce_module_units
    "${juce_modules_dir}/*/*.cpp" "${juce_modules_dir}/*/*.c" "${juce_modules_dir}/*/*.mm")
set_source_files_properties(${juce_module_units} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
get_target_property(stemdeck_library_code StemDeck JUCE_GENERATED_SOURCES_DIRECTORY)
target_precompile_headers(StemDeck PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${stemdeck_library_code}/JuceHeader.h>")

target_sources(StemDeck PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/OscTruth.cpp
    Source/OscTruthFile.cpp
    Source/Remote.cpp
    Source/RemoteLink.cpp
    Source/TruthKeeperLink.cpp
    Source/Theme.cpp
    Source/StemSet.cpp
    Source/StemDeckPlayer.cpp
    Source/Waveforms.cpp
    Source/DeckPanel.cpp
    Source/JogWheel.cpp
    Source/TempoAnalysis.cpp
    Source/BarPhase.cpp
    Source/MixerPanel.cpp
    Source/StemLibrary.cpp
    Source/SettingsPanel.cpp
    Source/FolderChoice.cpp
    Source/FolderStep.cpp
    Source/AudioPanel.cpp
    Source/Workspaces.cpp
    Source/KeyboardPolicy.cpp
    Source/OnScreenKeyboard.cpp
    Source/WorkspacePanel.cpp
    Source/JackOutput.cpp
    Source/ProLinkPackets.cpp
    Source/PioneerClock.cpp
    Source/FollowLeader.cpp
    Source/ProLinkReceiver.cpp
    Source/MasterDeck.cpp
    Source/SyncLabels.cpp
    Source/ProLinkSender.cpp
    Source/BeatScheduler.cpp
    Source/LibraryPath.cpp
    Source/StemNames.cpp
    Source/StemJob.cpp
    Source/StemCreator.cpp
    Source/ProcessGroup.cpp
    Source/MeterBallistics.cpp
    Source/Session.cpp
    Source/GridEdit.cpp
    Source/AutoDj.cpp
    Source/Recorder.cpp
    Source/DataPaths.cpp
    Source/Scs3d.cpp
    Source/Scs3dDevice.cpp
    Source/SurfaceLayout.cpp
    Source/LibraryFolders.cpp)

target_compile_definitions(StemDeck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0 # JACK output is handled by JackOutput (fixed 8 ports)
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:StemDeck,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:StemDeck,JUCE_VERSION>")

target_link_libraries(StemDeck
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_osc
        juce::juce_dsp
        PkgConfig::AUDIO_CODECS
        PkgConfig::JACK
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(STEMDECK_TESTS "Build the unit tests" OFF)
if(STEMDECK_TESTS)
    enable_testing()
    add_subdirectory(tests)
    add_subdirectory(tools/downbeat-eval)
endif()
