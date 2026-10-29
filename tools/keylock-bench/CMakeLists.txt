# KEY quality offline: both key lock engines timed on one deck's four stems,
# optionally written out to compare by ear. No JUCE, JACK or audio device.
add_executable(stemdeck-keylock-bench
    main.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLockStretcher.cpp
    ${CMAKE_SOURCE_DIR}/Source/KeyLock.cpp)
target_include_directories(stemdeck-keylock-bench PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_features(stemdeck-keylock-bench PRIVATE cxx_std_17)
target_link_libraries(stemdeck-keylock-bench PRIVATE PkgConfig::RUBBERBAND)
