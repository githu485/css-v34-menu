cmake_minimum_required(VERSION 3.16)
project(css_v34_menu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "compile as Win32 (x86)!")
endif()

add_library(css_v34_menu SHARED dllmain.cpp)
set_target_properties(css_v34_menu PROPERTIES PREFIX "" SUFFIX ".dll")

# Линкуем системные библиотеки Windows для GUI
target_link_libraries(css_v34_menu PRIVATE user32 gdi32 comctl32)
