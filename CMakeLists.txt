cmake_minimum_required(VERSION 3.16)
project(css_v34_menu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Разрешаем старому коду компилироваться без строгих ошибок
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -w")

# Включаем статическую линковку, чтобы DLL не требовала лишних файлов при инжекте
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")

# Проверка, что собираем 32-битную версию (x86)
if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "compile as Win32 (x86)!")
endif()

add_library(css_v34_menu SHARED dllmain.cpp)

# Убираем префикс "lib" и ставим расширение ".dll"
set_target_properties(css_v34_menu PROPERTIES PREFIX "" SUFFIX ".dll")

# Подключаем папки с ImGui и со скачанным DX8 SDK
target_include_directories(css_v34_menu PRIVATE 
    ${CMAKE_SOURCE_DIR}/imgui 
    ${CMAKE_SOURCE_DIR}/imgui/backends
    ${CMAKE_SOURCE_DIR}/min-dx8-sdk-master/include
)

# Исходники нашего проекта и ImGui
target_sources(css_v34_menu PRIVATE
    ${CMAKE_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_SOURCE_DIR}/imgui_impl_dx8.cpp
)

# Линкуем DirectX 8 и системные библиотеки
target_link_libraries(css_v34_menu PRIVATE d3d8 dwmapi gdi32)
