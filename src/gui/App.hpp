// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "NetworkTablePanel.hpp"
#include "SpectrumPanel.hpp"
#include "StatusBarPanel.hpp"
#include "wifi/IScanner.hpp"
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

struct GLFWwindow;

namespace gui
{

// Graphical front-end (wifiscan --gui). Mirrors ui::App: owns the background
// scan thread + shared state and runs the render loop on the main thread. Uses
// a GLFW + OpenGL3 Dear ImGui backend instead of FTXUI.
class App
{
public:
	explicit App(wifi::IScanner& scanner);

	// Opens the window and runs the ImGui loop; blocks until the window is
	// closed or the user presses 'q' / Escape.
	void Run();

private:
	// Builds one frame's UI from the latest snapshot.
	void RenderFrame();

	// Background thread body: mirrors ui::App::ScanLoop but wakes the render
	// loop via glfwPostEmptyEvent() (thread-safe) instead of FTXUI's task
	// queue.
	void ScanLoop(std::stop_token stopToken);

	wifi::IScanner& _scanner;

	GLFWwindow* _window{nullptr};

	// Most recent scan results shared between the scan thread and the render
	// thread
	std::vector<wifi::Network> _networks;

	// Guards _networks and _statusMsg
	std::mutex _mutex;

	// Human-readable status line updated after each scan
	std::string _statusMsg{"Scanning..."};

	SpectrumPanel _spectrumPanel;
	NetworkTablePanel _networkTablePanel;
	StatusBarPanel _statusBarPanel;

	// Set when the user presses 'q' / Escape
	bool _quit{false};

	// Declared last so it is destroyed first: its destructor
	// request_stop()+join()s before the shared state it touches is torn down
	// (same invariant as ui::App).
	std::jthread _scanThread;
};

} // namespace gui
