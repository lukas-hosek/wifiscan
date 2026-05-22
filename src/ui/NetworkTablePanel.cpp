#include "NetworkTablePanel.hpp"
#include "Theme.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <format>
#include <algorithm>

namespace ui
{

static std::string PadRight(const std::string& str, int width)
{
	if (static_cast<int>(str.size()) >= width)
		return str.substr(0, static_cast<size_t>(width));
	return str + std::string(static_cast<size_t>(width - static_cast<int>(str.size())), ' ');
}

bool NetworkTablePanel::HandleEvent(ftxui::Event event)
{
	if (event == ftxui::Event::ArrowUp)
	{
		if (_selectedRow > 0)
			_selectedRow--;
		return true;
	}
	if (event == ftxui::Event::ArrowDown)
	{
		_selectedRow++;
		return true;
	}
	return false;
}

ftxui::Element NetworkTablePanel::Render(const std::vector<wifi::Network>& networks)
{
	using namespace ftxui;

	// Clamp selection to valid range
	int rowCount = static_cast<int>(networks.size());
	if (_selectedRow >= rowCount && rowCount > 0)
		_selectedRow = rowCount - 1;
	if (_selectedRow < 0)
		_selectedRow = 0;

	auto headerRow = hbox({
		text(" ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("SSID", 24))   | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("BSSID", 17))  | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("CH", 4))      | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("BAND", 5))    | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("FREQ", 7))    | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("SIGNAL", 8))  | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("BARS", 7))    | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text("QUAL") | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
	});

	std::vector<Element> rows;
	rows.push_back(headerRow);
	rows.push_back(separator() | color(theme::Color(theme::UiColor::Border)));

	if (networks.empty())
	{
		rows.push_back(
		    text("  (no networks — try: sudo wifiscan)") | color(theme::Color(theme::UiColor::Muted))
		);
	}

	for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		const auto& network = networks[static_cast<size_t>(rowIndex)];

		std::string prefix = network._connected ? "[*] " : " -  ";
		auto prefixColor = network._connected ? theme::Color(theme::UiColor::ConnectedNetwork) : theme::Color(theme::UiColor::Muted);

		int bars = network.SignalBars();
		std::string ssid = network._ssid.empty() ? "???" : network._ssid;
		auto rowColor = network._connected ? theme::Color(theme::UiColor::ConnectedNetwork) : theme::Color(theme::UiColor::NetworkRow);

		auto dataRow = hbox({
			text(prefix) | color(prefixColor),
			text(PadRight(ssid, 24)) | color(rowColor),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(network._bssid, 17)) | color(theme::Color(theme::UiColor::Muted)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._channel), 4)) | color(theme::Color(theme::UiColor::DataValue)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(wifi::BandLabel(network._band), 5)) | color(theme::Color(theme::UiColor::DataValue)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._frequency) + "M", 7)) | color(theme::Color(theme::UiColor::Muted)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._signalDbm) + " dBm", 8)) | color(theme::SignalColor(network._signalDbm)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			hbox({
				text(std::string(static_cast<size_t>(bars), '*')) | color(theme::SignalColor(network._signalDbm)),
				text(std::string(static_cast<size_t>(5 - bars), '-')) | color(theme::Color(theme::UiColor::Muted)),
			}),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(std::to_string(network.SignalQuality()) + "%") | color(theme::SignalColor(network._signalDbm)),
		});

		if (rowIndex == _selectedRow)
			rows.push_back(dataRow | inverted | focus);
		else
			rows.push_back(dataRow);
	}

	return vbox(rows) | frame;
}

} // namespace ui
