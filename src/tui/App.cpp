// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "App.hpp"
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
	auto wallTime = std::chrono::system_clock::now() + (timePoint - std::chrono::steady_clock::now());
	auto timet = std::chrono::system_clock::to_time_t(wallTime);
	struct tm localTime{};
	localtime_r(&timet, &localTime);
	return fmt::format("{:02d}:{:02d}:{:02d}", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
}

App::App(wifi::IScanner& scanner) : _scanner(scanner)
{
	_spectrumPanel = std::make_unique<SpectrumPanel>();
	_networkTablePanel = std::make_unique<NetworkTablePanel>();
	_statusBarPanel = std::make_unique<StatusBarPanel>(_scanner);

	_scanThread = std::jthread(
		[this](std::stop_token stopToken)
		{
			ScanLoop(stopToken);
		});
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
			if (lastError.empty())
				_statusMsg = "Last update: " + FormatTime(scanEnd);
			else
				_statusMsg = "Error: " + lastError;
		}

		// Post() goes through FTXUI's mutex-guarded task queue; PostEvent()
		// pushes into an unsynchronized buffer and data-races the render thread.
		_screen.Post(ftxui::Event::Custom);

		_scanner.TriggerScan(stopToken);
	}
}

void App::Run()
{
	auto component = ftxui::Renderer(
		[this]
		{
			return Render();
		});

	component = ftxui::CatchEvent(component,
		[&](ftxui::Event event)
		{
			if (event == ftxui::Event::Character('q') || event == ftxui::Event::Character('Q'))
			{
				_screen.ExitLoopClosure()();
				return true;
			}
			if (_spectrumPanel->HandleEvent(event))
				return true;
			if (_networkTablePanel->HandleEvent(event))
				return true;
			return false;
		});

	// While Loop() is running, FTXUI owns the alternate screen buffer — any
	// std::print / printf / std::cout from any thread will corrupt the
	// display. Surface diagnostic messages through _statusMsg (rendered by
	// StatusBarPanel) instead.
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
		return ftxui::separator() | ftxui::color(theme::Color(theme::UiColor::Border));
	};

	// Fixed rows: 1 separator + status bar = 2 lines
	int available = std::max(0, ftxui::Terminal::Size().dimy - 2);
	int spectrumHeight = std::max(12, available * 2 / 5);
	int tableHeight = std::max(0, available - spectrumHeight);

	_statusBarPanel->SetStatus(statusMsg);

	return ftxui::vbox({
			   _spectrumPanel->Render(networks, spectrumHeight) |
				   ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, spectrumHeight),
			   _networkTablePanel->Render(networks, tableHeight) |
				   ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, tableHeight),
			   sep(),
			   _statusBarPanel->Render(networks, 1),
		   }) |
		ftxui::bgcolor(theme::Color(theme::UiColor::AppBackground));
}

} // namespace ui
