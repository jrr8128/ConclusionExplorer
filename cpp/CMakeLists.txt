cmake_minimum_required(VERSION 3.20)
project(my_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(app ${APP_SOURCES})
target_include_directories(app PRIVATE "${CMAKE_SOURCE_DIR}/src")

include(CTest)
enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.cpp")
list(FILTER TEST_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")

add_executable(tests
  tests/smoke_tests.cpp
  third_party/catch2/catch_amalgamated.cpp
  ${TEST_SOURCES}
)

target_include_directories(tests PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/catch2"
)

add_test(NAME tests COMMAND tests)


