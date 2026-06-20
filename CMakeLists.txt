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

option(WIFISCAN_ENABLE_GUI "Build the ImGui graphical UI (--gui)" ON)

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

if(WIFISCAN_ENABLE_GUI)
	pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
	find_package(OpenGL REQUIRED)

	# Dear ImGui core + GLFW/OpenGL3 backends, built as their own target so the
	# project's strict warning flags don't apply to vendored ImGui sources.
	add_library(imgui STATIC
		third_party/imgui/imgui.cpp
		third_party/imgui/imgui_draw.cpp
		third_party/imgui/imgui_tables.cpp
		third_party/imgui/imgui_widgets.cpp
		third_party/imgui/backends/imgui_impl_glfw.cpp
		third_party/imgui/backends/imgui_impl_opengl3.cpp
	)
	target_include_directories(imgui PUBLIC
		third_party/imgui
		third_party/imgui/backends
	)
	target_link_libraries(imgui PUBLIC PkgConfig::GLFW OpenGL::GL)

	target_sources(wifiscan PRIVATE
		src/gui/Theme.cpp
		src/gui/ImGuiExtras.cpp
		src/gui/StatusBarPanel.cpp
		src/gui/NetworkTablePanel.cpp
		src/gui/SpectrumPanel.cpp
		src/gui/App.cpp
	)
	target_link_libraries(wifiscan PRIVATE imgui)
	target_compile_definitions(wifiscan PRIVATE WIFISCAN_GUI)
endif()

include(GNUInstallDirs)
install(TARGETS wifiscan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
