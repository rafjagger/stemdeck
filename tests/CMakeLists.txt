find_package(GTest REQUIRED)
add_executable(stemdeck-tests
    Smoke.cpp
    ProLinkPackets.cpp
    ${CMAKE_SOURCE_DIR}/Source/ProLinkPackets.cpp)
target_include_directories(stemdeck-tests PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_features(stemdeck-tests PRIVATE cxx_std_17)
target_link_libraries(stemdeck-tests PRIVATE GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(stemdeck-tests)
