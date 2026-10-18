cmake_minimum_required(VERSION 3.16)
project(css_v34_menu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "compile as Win32 (x86)!")
endif()

add_library(css_v34_menu SHARED dllmain.cpp)

target_include_directories(css_v34_menu PRIVATE 
    ${CMAKE_SOURCE_DIR}/imgui 
    ${CMAKE_SOURCE_DIR}/imgui/backends
)

target_sources(css_v34_menu PRIVATE
    ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_dx9.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_win32.cpp
)

target_link_libraries(css_v34_menu PRIVATE d3d9 dwmapi gdi32)
