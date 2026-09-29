cmake_minimum_required(VERSION 3.22)

project(StemDeck VERSION 1.0.0)

find_package(JUCE CONFIG REQUIRED)

# Distro JUCE packages (e.g. Debian's "+ds" build) strip the bundled FLAC/Ogg
# codecs and expect the system libraries to be linked instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUDIO_CODECS REQUIRED IMPORTED_TARGET flac vorbisenc vorbisfile vorbis ogg)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack)

juce_add_gui_app(StemDeck
    PRODUCT_NAME "StemDeck")

juce_generate_juce_header(StemDeck)

target_sources(StemDeck PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Theme.cpp
    Source/StemSet.cpp
    Source/StemDeckPlayer.cpp
    Source/Waveforms.cpp
    Source/DeckPanel.cpp
    Source/JogWheel.cpp
    Source/TempoAnalysis.cpp
    Source/MixerPanel.cpp
    Source/StemLibrary.cpp
    Source/JackOutput.cpp
    Source/ProLinkPackets.cpp
    Source/PioneerClock.cpp
    Source/FollowLeader.cpp
    Source/ProLinkReceiver.cpp
    Source/MasterDeck.cpp
    Source/ProLinkSender.cpp
    Source/BeatScheduler.cpp)

target_compile_definitions(StemDeck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_JACK=0 # JACK output is handled by JackOutput (fixed 8 ports)
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:StemDeck,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:StemDeck,JUCE_VERSION>")

target_link_libraries(StemDeck
    PRIVATE
        juce::juce_audio_utils
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
endif()
