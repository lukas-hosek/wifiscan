#pragma once
#include "IPanel.hpp"
#include "wifi/IScanner.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include <memory>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>

namespace ui
{

class App
{
public:
	// Takes ownership of nothing; scanner must outlive App
	explicit App(wifi::IScanner& scanner);

	// Enters the FTXUI fullscreen loop; blocks until the user presses 'q' or Ctrl+C
	void Run();

private:
	// Assembles the full element tree from all panels plus the status bar
	ftxui::Element Render();

	// Renders the bottom status bar (interface, last scan time, next scan countdown, quit hint)
	ftxui::Element RenderStatusBar();

	// Background thread body: calls GetNetworks() every 2 seconds and wakes the UI.
	// Exits when the stop token is signalled (by jthread's destructor on App teardown).
	void ScanLoop(std::stop_token stopToken);

	// The data source; may be a live nl80211 scanner or any other IScanner implementation
	wifi::IScanner& _scanner;

	// Fullscreen interactive terminal managed by FTXUI
	ftxui::ScreenInteractive _screen{ftxui::ScreenInteractive::Fullscreen()};

	// Most recent scan results shared between the scan thread and the render thread
	std::vector<wifi::Network> _networks;

	// Guards _networks and _statusMsg against concurrent access
	std::mutex _mutex;

	// Human-readable status line updated after each scan (e.g. "Last scan: 14:32:01")
	std::string _statusMsg{"Scanning..."};

	// Timestamp of the most recent completed scan, used to compute next-scan countdown
	std::chrono::steady_clock::time_point _lastScan{};

	// Ordered list of panels rendered top-to-bottom; add new IPanel impls here for new views
	std::vector<std::unique_ptr<IPanel>> _panels;

	// Runs ScanLoop() concurrently with the FTXUI render loop.
	// Declared last so it is destroyed first: its destructor calls request_stop() + join(),
	// guaranteeing ScanLoop exits before any other member is torn down.
	std::jthread _scanThread;
};

} // namespace ui
