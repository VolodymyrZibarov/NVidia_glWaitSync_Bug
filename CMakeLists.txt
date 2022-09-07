cmake_minimum_required(VERSION 3.20)

project(NVidia_glWaitSync_Bug)
set(CMAKE_CXX_STANDARD 20)
set(BUILDBIN "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_subdirectory(Ext)
add_subdirectory(SyncTest)
include(InstallRequiredSystemLibraries)
