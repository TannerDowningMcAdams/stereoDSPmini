cmake_minimum_required(VERSION 3.22)

# Before project(): pins ninja for the superbuild itself. No languages, so no toolchain here.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/clt.cmake")
project(stereoDSPmini LANGUAGES NONE)

# ExternalProject_Add() captures CMAKE_COMMAND at call time; the VS Code extension sets it to cube-cmake.
set(CMAKE_COMMAND "${CLT_CMAKE}")
message(STATUS "Sub-builds use ${CMAKE_COMMAND} and ${CMAKE_MAKE_PROGRAM}")

include(ExternalProject)
include("${CMAKE_CURRENT_SOURCE_DIR}/local-projects.cmake")
