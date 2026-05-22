#include "App.hpp"
#include "BannerPanel.hpp"
#include "SpectrumPanel.hpp"
#include "NetworkTablePanel.hpp"
#include "Theme.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <chrono>
#include <format>
#include <ctime>

namespace ui
{

static std::string FormatTime(std::chrono::steady_clock::time_point timePoint)
{
	auto wallTime = std::chrono::system_clock::now() +
	    (timePoint - std::chrono::steady_clock::now());
	auto timet = std::chrono::system_clock::to_time_t(wallTime);
	struct tm localTime{};
	localtime_r(&timet, &localTime);
	return std::format("{:02d}:{:02d}:{:02d}", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
}

App::App(wifi::IScanner& scanner)
	: _scanner(scanner)
{
	_panels.push_back(std::make_unique<BannerPanel>(_scanner.GetInterface()));
	_panels.push_back(std::make_unique<SpectrumPanel>());
	_panels.push_back(std::make_unique<NetworkTablePanel>());

	_scanThread = std::jthread([this](std::stop_token stopToken) { ScanLoop(stopToken); });
}

void App::ScanLoop(std::stop_token stopToken)
{
	while (!stopToken.stop_requested())
	{
		auto freshNetworks = _scanner.GetNetworks();
		auto scanEnd = std::chrono::steady_clock::now();

		std::ranges::sort(freshNetworks, [](const wifi::Network& lhs, const wifi::Network& rhs)
		{
			return lhs._signalDbm > rhs._signalDbm;
		});

		{
			std::lock_guard lock(_mutex);
			_networks  = std::move(freshNetworks);
			_lastScan  = scanEnd;

			const std::string& lastError = _scanner.GetLastError();
			if (!lastError.empty())
				_statusMsg = "Error: " + lastError;
			else
				_statusMsg = "Last scan: " + FormatTime(scanEnd);
		}

		_screen.PostEvent(ftxui::Event::Custom);
		auto wakeAt = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (!stopToken.stop_requested() && std::chrono::steady_clock::now() < wakeAt)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			_screen.PostEvent(ftxui::Event::Custom);
		}
	}
}

void App::Run()
{
	auto component = ftxui::Renderer([this] { return Render(); });

	component = ftxui::CatchEvent(component, [&](ftxui::Event event)
	{
		if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q'))
		{
			_screen.ExitLoopClosure()();
			return true;
		}
		for (auto& panel : _panels)
			if (panel->HandleEvent(event)) return true;
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
		networks  = _networks;
		statusMsg = _statusMsg;
	}

	auto sep = [&]{ return ftxui::separator() | ftxui::color(theme::Color(theme::UiColor::Border)); };

	// Fixed rows: banner + 3 separators + status bar = 5 lines
	int available = std::max(6, ftxui::Terminal::Size().dimy - 5);
	int spectrumHeight = available / 3;
	int tableHeight    = available - spectrumHeight;

	return ftxui::vbox({
		_panels[0]->Render(networks),
		sep(),
		_panels[1]->Render(networks) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, spectrumHeight),
		sep(),
		_panels[2]->Render(networks) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, tableHeight),
		sep(),
		RenderStatusBar(),
	}) | ftxui::bgcolor(theme::Color(theme::UiColor::AppBackground));
}

ftxui::Element App::RenderStatusBar()
{
	using namespace ftxui;

	std::string statusMsg;
	int secondsUntilNext = 5;
	{
		std::lock_guard lock(_mutex);
		statusMsg = _statusMsg;
		if (_lastScan.time_since_epoch().count() > 0)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
			    std::chrono::steady_clock::now() - _lastScan).count();
			secondsUntilNext = std::max(0LL, 5LL - elapsed);
		}
	}

	return hbox({
		text(" " + statusMsg + " ") | color(theme::Color(theme::UiColor::StatusText)),
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text("next scan: " + std::to_string(secondsUntilNext) + "s") | color(theme::Color(theme::UiColor::Muted)),
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text("iface: " + _scanner.GetInterface()) | color(theme::Color(theme::UiColor::DataValue)),
		filler(),
		text(" [q] quit  [↑↓] scroll  [←→] spectrum  [Tab] band ") | color(theme::Color(theme::UiColor::Muted)) | dim,
	});
}

} // namespace ui
