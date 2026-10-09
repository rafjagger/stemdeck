# Measures the downbeat detection against grids corrected by hand: not a
# test (it needs real tracks, which stay out of the repository) and not
# built by default.
#   stemdeck-downbeat-eval <analysis.xml> <stems folder> [decoded-audio folder]
juce_add_console_app(stemdeck-downbeat-eval PRODUCT_NAME "stemdeck-downbeat-eval")
juce_generate_juce_header(stemdeck-downbeat-eval)
set_target_properties(stemdeck-downbeat-eval PROPERTIES EXCLUDE_FROM_ALL ON)

target_sources(stemdeck-downbeat-eval PRIVATE
    Main.cpp
    ${CMAKE_SOURCE_DIR}/Source/TempoAnalysis.cpp
    ${CMAKE_SOURCE_DIR}/Source/BarPhase.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemSet.cpp
    ${CMAKE_SOURCE_DIR}/Source/LibraryPath.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemNames.cpp)
target_include_directories(stemdeck-downbeat-eval PRIVATE ${CMAKE_SOURCE_DIR}/Source)

target_compile_definitions(stemdeck-downbeat-eval PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(stemdeck-downbeat-eval
    PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
        PkgConfig::AUDIO_CODECS
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
