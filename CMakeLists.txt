cmake_minimum_required(VERSION 3.28)
project(wifiscan CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(NL3     REQUIRED IMPORTED_TARGET libnl-3.0)
pkg_check_modules(NL_GENL REQUIRED IMPORTED_TARGET libnl-genl-3.0)

set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/ftxui)

add_executable(wifiscan
	src/main.cpp
	src/wifi/Network.cpp
	src/wifi/NL80211Scanner.cpp
	src/ui/Theme.cpp
	src/ui/TextUtil.cpp
	src/ui/SpectrumPanel.cpp
	src/ui/NetworkTablePanel.cpp
	src/ui/App.cpp
)

target_include_directories(wifiscan PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(wifiscan PRIVATE
	PkgConfig::NL3 PkgConfig::NL_GENL
	ftxui::component ftxui::dom ftxui::screen
)
target_compile_options(wifiscan PRIVATE -Wall -Wextra -Wpedantic)
