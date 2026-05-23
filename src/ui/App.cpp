// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "App.hpp"
#include "NetworkTablePanel.hpp"
#include "SpectrumPanel.hpp"
#include "Theme.hpp"
#include <chrono>
#include <ctime>
#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

namespace ui
{

static std::string FormatTime(std::chrono::steady_clock::time_point timePoint)
{
	auto wallTime = std::chrono::system_clock::now() +
					(timePoint - std::chrono::steady_clock::now());
	auto timet = std::chrono::system_clock::to_time_t(wallTime);
	struct tm localTime{};
	localtime_r(&timet, &localTime);
	return fmt::format("{:02d}:{:02d}:{:02d}", localTime.tm_hour,
					   localTime.tm_min, localTime.tm_sec);
}

App::App(wifi::IScanner& scanner) : _scanner(scanner)
{
	_panels.push_back(std::make_unique<SpectrumPanel>());
	_panels.push_back(std::make_unique<NetworkTablePanel>());

	_scanThread = std::jthread([this](std::stop_token stopToken)
							   { ScanLoop(stopToken); });
}

void App::ScanLoop(std::stop_token stopToken)
{
	while (!stopToken.stop_requested())
	{
		auto freshNetworks = _scanner.GetNetworks();
		auto scanEnd = std::chrono::steady_clock::now();

		{
			std::lock_guard lock(_mutex);
			_networks = std::move(freshNetworks);

			const std::string& lastError = _scanner.GetLastError();
			wifi::ScanFetchState fetchState = _scanner.GetLastFetchState();
			if (fetchState == wifi::ScanFetchState::Unknown &&
				!lastError.empty())
				_statusMsg = "Error: " + lastError;
			else
				_statusMsg = "Last update: " + FormatTime(scanEnd);
		}

		_screen.PostEvent(ftxui::Event::Custom);
		auto wakeAt =
			std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!stopToken.stop_requested() &&
			   std::chrono::steady_clock::now() < wakeAt)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			_screen.PostEvent(ftxui::Event::Custom);
		}
	}
}

void App::Run()
{
	auto component = ftxui::Renderer([this] { return Render(); });

	component =
		ftxui::CatchEvent(component,
						  [&](ftxui::Event event)
						  {
							  if (event == ftxui::Event::Character('q') ||
								  event == ftxui::Event::Character('Q'))
							  {
								  _screen.ExitLoopClosure()();
								  return true;
							  }
							  for (auto& panel : _panels)
								  if (panel->HandleEvent(event))
									  return true;
							  return false;
						  });

	_screen.Loop(component);
	// _scanThread destructor fires here: request_stop() + join()
}

ftxui::Element App::Render()
{
	std::vector<wifi::Network> networks;
	std::string statusMsg;
	{
		std::lock_guard lock(_mutex);
		networks = _networks;
		statusMsg = _statusMsg;
	}

	auto sep = [&]
	{
		return ftxui::separator() |
			   ftxui::color(theme::Color(theme::UiColor::Border));
	};

	// Fixed rows: 2 separators + status bar = 3 lines
	int available = std::max(0, ftxui::Terminal::Size().dimy - 3);
	int spectrumHeight = std::max(12, available * 2 / 5);
	int tableHeight = std::max(0, available - spectrumHeight);

	return ftxui::vbox({
			   _panels[0]->Render(networks, spectrumHeight) |
				   ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, spectrumHeight),
			   sep(),
			   _panels[1]->Render(networks, tableHeight) |
				   ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, tableHeight),
			   sep(),
			   RenderStatusBar(),
		   }) |
		   ftxui::bgcolor(theme::Color(theme::UiColor::AppBackground));
}

ftxui::Element App::RenderStatusBar()
{
	using namespace ftxui;

	std::string statusMsg;
	{
		std::lock_guard lock(_mutex);
		statusMsg = _statusMsg;
	}

	static constexpr std::string_view kHintsFull =
		" [q] quit  [↑↓] scroll  [←→] spectrum  [Tab] band  [s] sort  [e] "
		"hide-connected ";
	static constexpr std::string_view kHintsCompact = " [q] [↑↓] [←→] [Tab] [s] [e] ";
	// Arrow glyphs are 3 UTF-8 bytes but 1 display column each; there are 4 of
	// them in each hint string.
	static constexpr int kHintColsFull =
		static_cast<int>(kHintsFull.size()) - 4 * 2;

	std::string iface = "iface: " + _scanner.GetInterface();
	int ifaceCols = static_cast<int>(iface.size());
	// "statusMsg " = statusMsg.size() + 1 display col
	int statusCols = static_cast<int>(statusMsg.size()) + 1;
	int termWidth = Terminal::Size().dimx;
	bool showFull = termWidth >= ifaceCols + kHintColsFull;
	bool showStatus = showFull && termWidth >= ifaceCols + kHintColsFull + statusCols;

	std::string_view hints = showFull ? kHintsFull : kHintsCompact;

	std::vector<Element> items;
	if (showStatus)
	{
		items.push_back(text(statusMsg + " ") |
						color(theme::Color(theme::UiColor::StatusText)));
	}
	items.push_back(text(iface) |
					color(theme::Color(theme::UiColor::DataValue)));
	items.push_back(filler());
	items.push_back(text(std::string(hints)) |
					color(theme::Color(theme::UiColor::ShortcutHint)));

	return hbox(items);
}

} // namespace ui
