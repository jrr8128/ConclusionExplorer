add_executable(tests
  smoke_tests.cpp
  ../third_party/catch2/catch_amalgamated.cpp)

target_include_directories(tests PRIVATE ../third_party/catch2)
target_link_libraries(tests PRIVATE core)
add_test(NAME tests COMMAND tests)
