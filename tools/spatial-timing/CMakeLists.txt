# One deck's spatial DSP timed without a test runner (S1.4): a plain
# executable for machines that have no gtest, JACK or audio device.
add_executable(stemdeck-spatial-timing
    main.cpp
    ${CMAKE_SOURCE_DIR}/Source/SteadyBed.cpp
    ${CMAKE_SOURCE_DIR}/Source/DeckSpatial.cpp
    ${CMAKE_SOURCE_DIR}/Source/SpatialLaw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-core/src/a3-engine/Laws.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-core/src/a3-engine/PurestGain.cpp
    ${CMAKE_SOURCE_DIR}/third_party/a3-core/src/a3-engine/SpatialConfig.cpp)
target_include_directories(stemdeck-spatial-timing PRIVATE
    ${CMAKE_SOURCE_DIR}/Source ${CMAKE_SOURCE_DIR}/third_party/a3-core/src)
target_compile_features(stemdeck-spatial-timing PRIVATE cxx_std_17)
target_link_libraries(stemdeck-spatial-timing PRIVATE a3-iem-sd)
