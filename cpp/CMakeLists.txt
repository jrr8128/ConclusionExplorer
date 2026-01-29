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

# Enable LTO/IPO for Release
set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# CPU‑specific tuning (per‑machine build)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(app PRIVATE $<$<CONFIG:Release>:-march=native>)
endif()