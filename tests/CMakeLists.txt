find_package(GTest REQUIRED)
add_executable(stemdeck-tests
    Smoke.cpp
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
    SurfaceLayout.cpp
    ${CMAKE_SOURCE_DIR}/Source/SurfaceLayout.cpp
    LibraryFolders.cpp
    ${CMAKE_SOURCE_DIR}/Source/LibraryFolders.cpp)
target_include_directories(stemdeck-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

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
target_link_libraries(stemdeck-tests PRIVATE GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(stemdeck-tests)
