find_package(GTest REQUIRED)
add_executable(stemdeck-tests
    Smoke.cpp
    CueLight.cpp
    ${CMAKE_SOURCE_DIR}/Source/CueLight.cpp
    WaveformScale.cpp
    WaveformColumns.cpp
    BarPhase.cpp
    ${CMAKE_SOURCE_DIR}/Source/BarPhase.cpp
    CachedGrid.cpp
    ${CMAKE_SOURCE_DIR}/Source/CachedGrid.cpp
    Buses.cpp
    Outputs.cpp
    GridEdit.cpp
    AutoDj.cpp
    MotionLite.cpp
    ${CMAKE_SOURCE_DIR}/Source/MotionLite.cpp
    StemHandover.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemHandover.cpp
    RecordingName.cpp
    Scs3d.cpp
    ${CMAKE_SOURCE_DIR}/Source/Scs3d.cpp
    ${CMAKE_SOURCE_DIR}/Source/AutoDj.cpp
    ${CMAKE_SOURCE_DIR}/Source/GridEdit.cpp
    ProLinkPackets.cpp
    ${CMAKE_SOURCE_DIR}/Source/ProLinkPackets.cpp
    OscTruth.cpp
    ${CMAKE_SOURCE_DIR}/Source/OscTruth.cpp
    Remote.cpp
    RemotePointer.cpp
    ${CMAKE_SOURCE_DIR}/Source/RemotePointer.cpp
    ${CMAKE_SOURCE_DIR}/Source/OscTruthFile.cpp
    TruthKeeper.cpp
    ${CMAKE_SOURCE_DIR}/Source/Remote.cpp
    PioneerClock.cpp
    ${CMAKE_SOURCE_DIR}/Source/PioneerClock.cpp
    FollowLeader.cpp
    ${CMAKE_SOURCE_DIR}/Source/FollowLeader.cpp
    MasterDeck.cpp
    ${CMAKE_SOURCE_DIR}/Source/MasterDeck.cpp
    SyncLabels.cpp
    ${CMAKE_SOURCE_DIR}/Source/SyncLabels.cpp
    BeatScheduler.cpp
    ${CMAKE_SOURCE_DIR}/Source/BeatScheduler.cpp
    LibraryPath.cpp
    ${CMAKE_SOURCE_DIR}/Source/LibraryPath.cpp
    StemNames.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemNames.cpp
    StemJob.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemJob.cpp
    ProcessGroup.cpp
    ${CMAKE_SOURCE_DIR}/Source/ProcessGroup.cpp
    MeterBallistics.cpp
    ${CMAKE_SOURCE_DIR}/Source/MeterBallistics.cpp
    Workspaces.cpp
    ${CMAKE_SOURCE_DIR}/Source/Workspaces.cpp
    KeyboardPolicy.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyboardPolicy.cpp
    FolderChoice.cpp
    ${CMAKE_SOURCE_DIR}/Source/FolderChoice.cpp
    FolderStep.cpp
    ${CMAKE_SOURCE_DIR}/Source/FolderStep.cpp
    DataPaths.cpp
    ${CMAKE_SOURCE_DIR}/Source/DataPaths.cpp
    SectionFeatures.cpp
    Sections.cpp
    Preview.cpp
    ${CMAKE_SOURCE_DIR}/Source/Preview.cpp
    ${CMAKE_SOURCE_DIR}/Source/Sections.cpp
    SurfaceLayout.cpp
    ${CMAKE_SOURCE_DIR}/Source/SurfaceLayout.cpp
    LibraryFolders.cpp
    Tips.cpp
    PanelProtocol.cpp
    ${CMAKE_SOURCE_DIR}/Source/PanelProtocol.cpp
    PanelMap.cpp
    ${CMAKE_SOURCE_DIR}/Source/PanelMap.cpp
    PanelLink.cpp
    ${CMAKE_SOURCE_DIR}/Source/PanelLink.cpp
    ${CMAKE_SOURCE_DIR}/Source/Tips.cpp
    ${CMAKE_SOURCE_DIR}/Source/LibraryFolders.cpp
    KeyLock.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLock.cpp
    KeyLockStretcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLockStretcher.cpp
    Session.cpp
    ${CMAKE_SOURCE_DIR}/Source/Session.cpp
    SpatialLaw.cpp
    ${CMAKE_SOURCE_DIR}/Source/SpatialLaw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-engine/src/a3-engine/Laws.cpp)
target_include_directories(stemdeck-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source ${CMAKE_SOURCE_DIR}/third_party/a3-engine/src)

# The session is XML: juce_core, and nothing else of JUCE, comes into the
# tests -- as a library of its own, JUCE's way for module code outside an app.
add_library(stemdeck-tests-juce STATIC)
target_link_libraries(stemdeck-tests-juce
    PRIVATE juce::juce_core
    PUBLIC juce::juce_recommended_config_flags)
target_compile_definitions(stemdeck-tests-juce PUBLIC JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_STANDALONE_APPLICATION=0)
target_compile_definitions(stemdeck-tests-juce INTERFACE $<TARGET_PROPERTY:stemdeck-tests-juce,COMPILE_DEFINITIONS>)
target_include_directories(stemdeck-tests-juce INTERFACE $<TARGET_PROPERTY:stemdeck-tests-juce,INCLUDE_DIRECTORIES>)

# Debian keeps upstream file times, so make cannot see that libgtest-dev was
# upgraded and links old test objects against the new library (stemdeck#1).
# The stamp header holds a hash of the library and gtest.h; every test object
# includes it, so a different googletest rebuilds them. It is checked on every
# build, which costs a hash, not a rebuild.
get_target_property(gtest_include GTest::gtest INTERFACE_INCLUDE_DIRECTORIES)
set(gtest_stamp_header ${CMAKE_CURRENT_BINARY_DIR}/gtest_stamp.h)
add_custom_target(stemdeck-gtest-stamp
    COMMAND ${CMAKE_COMMAND} -DOUT=${gtest_stamp_header}
            "-DFILES=$<TARGET_FILE:GTest::gtest>$<SEMICOLON>${gtest_include}/gtest/gtest.h"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/gtest-stamp.cmake
    BYPRODUCTS ${gtest_stamp_header}
    VERBATIM)
add_dependencies(stemdeck-tests stemdeck-gtest-stamp)
target_compile_options(stemdeck-tests PRIVATE -include ${gtest_stamp_header})
target_compile_features(stemdeck-tests PRIVATE cxx_std_17)
target_link_libraries(stemdeck-tests PRIVATE GTest::gtest GTest::gtest_main PkgConfig::RUBBERBAND stemdeck-tests-juce)
include(GoogleTest)
gtest_discover_tests(stemdeck-tests)

# The injector hands a touch to a real window, so these tests need JUCE's GUI
# and an X server. Without a DISPLAY ctest starts them under xvfb-run (never
# by unsetting DISPLAY: JUCE then falls back to :0.0, the rig's screen).
add_executable(stemdeck-gui-tests
    PointerInjector.cpp
    StemSpatialPopup.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemSpatialPopup.cpp
    ${CMAKE_SOURCE_DIR}/Source/PointerInjector.cpp
    ${CMAKE_SOURCE_DIR}/Source/RemotePointer.cpp)
target_include_directories(stemdeck-gui-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_features(stemdeck-gui-tests PRIVATE cxx_std_17)
target_compile_definitions(stemdeck-gui-tests PRIVATE
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(stemdeck-gui-tests PRIVATE GTest::gtest GTest::gtest_main
    juce::juce_gui_basics juce::juce_audio_basics juce::juce_recommended_config_flags)
add_dependencies(stemdeck-gui-tests stemdeck-gtest-stamp)
target_compile_options(stemdeck-gui-tests PRIVATE -include ${gtest_stamp_header})

# Always under xvfb-run when it exists, even with a DISPLAY set: on the rig
# that would be the live screen, and the test opens a window.
find_program(XVFB_RUN xvfb-run)
if(XVFB_RUN)
    set_target_properties(stemdeck-gui-tests PROPERTIES CROSSCOMPILING_EMULATOR "${XVFB_RUN};-a")
endif()
gtest_discover_tests(stemdeck-gui-tests)

# a3-engine's IEM nodes, compiled into StemDeck's own JUCE (stemdeck-juce).
# One deck's spatial engine beside them; a3-engine's SpatialConfig reads the
# engine's own bed files for the SteadyBed reference.
add_executable(stemdeck-spatial-tests
    spatial/IemInStemDeck.cpp
    spatial/SteadyBed.cpp
    spatial/DeckSpatial.cpp
    spatial/DeckSpatialAllocation.cpp
    spatial/DeckSpatialTiming.cpp
    spatial/DeckSpatialPorts.cpp
    ${CMAKE_SOURCE_DIR}/Source/SteadyBed.cpp
    ${CMAKE_SOURCE_DIR}/Source/DeckSpatial.cpp
    ${CMAKE_SOURCE_DIR}/Source/DeckSpatialPorts.cpp
    ${CMAKE_SOURCE_DIR}/Source/StemDeckPlayer.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLockStretcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLock.cpp
    ${CMAKE_SOURCE_DIR}/Source/Remote.cpp
    ${CMAKE_SOURCE_DIR}/Source/GridEdit.cpp
    ${CMAKE_SOURCE_DIR}/Source/SpatialLaw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-engine/src/a3-engine/Laws.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-engine/src/a3-engine/SpatialConfig.cpp)
target_include_directories(stemdeck-spatial-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source ${CMAKE_SOURCE_DIR}/third_party/a3-engine/src)
target_compile_definitions(stemdeck-spatial-tests PRIVATE
    STEMDECK_SPATIAL_DATA="${CMAKE_SOURCE_DIR}/third_party/a3-engine/data/spatial")
target_compile_features(stemdeck-spatial-tests PRIVATE cxx_std_17)
target_link_libraries(stemdeck-spatial-tests PRIVATE GTest::gtest GTest::gtest_main a3-iem-sd PkgConfig::RUBBERBAND)
# The player (Stage S2) includes the app's JuceHeader.h: StemDeck generates it.
get_target_property(stemdeck_juce_header_dir StemDeck JUCE_GENERATED_SOURCES_DIRECTORY)
target_include_directories(stemdeck-spatial-tests PRIVATE ${stemdeck_juce_header_dir})
add_dependencies(stemdeck-spatial-tests StemDeck)
add_dependencies(stemdeck-spatial-tests stemdeck-gtest-stamp)
target_compile_options(stemdeck-spatial-tests PRIVATE -include ${gtest_stamp_header})
gtest_discover_tests(stemdeck-spatial-tests)
