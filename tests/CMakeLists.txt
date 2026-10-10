find_package(GTest REQUIRED)
add_executable(stemdeck-tests
    Smoke.cpp
    WaveformScale.cpp
    BarPhase.cpp
    ${CMAKE_SOURCE_DIR}/Source/BarPhase.cpp
    CachedGrid.cpp
    ${CMAKE_SOURCE_DIR}/Source/CachedGrid.cpp
    Buses.cpp
    Outputs.cpp
    GridEdit.cpp
    AutoDj.cpp
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
    Pointer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Pointer.cpp
    ${CMAKE_SOURCE_DIR}/Source/Tips.cpp
    ${CMAKE_SOURCE_DIR}/Source/LibraryFolders.cpp
    KeyLock.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLock.cpp
    KeyLockStretcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLockStretcher.cpp
    Session.cpp
    ${CMAKE_SOURCE_DIR}/Source/Session.cpp)
target_include_directories(stemdeck-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

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
