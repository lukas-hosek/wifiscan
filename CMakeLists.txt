cmake_minimum_required(VERSION 3.22)
project(wifiscan CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Symlink compile_commands.json into the project root so IntelliSense picks it up
# without requiring a manual copy after every configure.
file(CREATE_LINK
	"${CMAKE_BINARY_DIR}/compile_commands.json"
	"${CMAKE_SOURCE_DIR}/compile_commands.json"
	SYMBOLIC
)

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

set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/fmt)

add_executable(wifiscan
	src/main.cpp
	src/wifi/Network.cpp
	src/wifi/NL80211Scanner.cpp
	src/ui/Theme.cpp
	src/utils/TextUtil.cpp
	src/ui/SpectrumPanel.cpp
	src/ui/NetworkTablePanel.cpp
	src/ui/NonInteractiveOutput.cpp
	src/ui/StatusBarPanel.cpp
	src/ui/App.cpp
)

target_include_directories(wifiscan PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(wifiscan PRIVATE
	PkgConfig::NL3 PkgConfig::NL_GENL
	ftxui::component ftxui::dom ftxui::screen
	fmt::fmt
)
target_compile_options(wifiscan PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS wifiscan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
