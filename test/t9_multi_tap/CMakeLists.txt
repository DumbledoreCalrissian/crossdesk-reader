add_executable(T9MultiTapTest T9MultiTapTest.cpp ../../src/util/T9MultiTap.cpp)
target_include_directories(T9MultiTapTest PRIVATE ../../src)
target_link_libraries(T9MultiTapTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(T9MultiTapTest)
