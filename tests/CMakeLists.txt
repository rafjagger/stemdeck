find_package(GTest REQUIRED)
add_executable(stemdeck-tests
    Smoke.cpp
    Buses.cpp
    Outputs.cpp
    GridEdit.cpp
    AutoDj.cpp
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
    ${CMAKE_SOURCE_DIR}/Source/KeyboardPolicy.cpp)
target_include_directories(stemdeck-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_features(stemdeck-tests PRIVATE cxx_std_17)
target_link_libraries(stemdeck-tests PRIVATE GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(stemdeck-tests)
