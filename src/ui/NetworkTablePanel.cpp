// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NetworkTablePanel.hpp"
#include "TextUtil.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ranges>

namespace ui
{

namespace
{

void SortByMode(std::vector<wifi::Network>& nets, SortMode mode)
{
	switch (mode)
	{
	case SortMode::Signal:
		std::ranges::sort(nets, [](const auto& a, const auto& b)
						  { return a._signalDbm > b._signalDbm; });
		break;
	case SortMode::SSID:
		std::ranges::sort(nets, [](const auto& a, const auto& b)
						  { return a._ssid < b._ssid; });
		break;
	case SortMode::Channel:
		std::ranges::sort(nets, [](const auto& a, const auto& b)
						  { return a._channel < b._channel; });
		break;
	}
}

ftxui::Element BuildHeaderRow(SortMode active)
{
	using namespace ftxui;

	auto sep = []
	{ return text(" | ") | color(theme::Color(theme::UiColor::Muted)); };

	// Returns label with "*" appended when this column is the active sort key
	auto hdrText = [&](const std::string& label, int width, SortMode mode)
	{ return PadRight(active == mode ? label + "*" : label, width); };
	auto hdrColor = [&](SortMode mode) -> Color
	{
		return active == mode ? theme::Color(theme::UiColor::DataValue)
							  : theme::Color(theme::UiColor::ColumnHeader);
	};

	return hbox({
		text("    ") | color(theme::Color(theme::UiColor::Muted)),
		text(hdrText("SSID", 24, SortMode::SSID)) |
			color(hdrColor(SortMode::SSID)) | bold,
		sep(),
		text(PadRight("BSSID", 17)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(hdrText("CH", 4, SortMode::Channel)) |
			color(hdrColor(SortMode::Channel)) | bold,
		sep(),
		text(PadRight("BAND", 4)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(PadRight("W", 3)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(PadRight("STD", 3)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(PadRight("SEC", 7)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(PadRight("RATE", 7)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		sep(),
		text(hdrText("SIGNAL", 8, SortMode::Signal)) |
			color(hdrColor(SortMode::Signal)) | bold,
		sep(),
		text(PadRight("QUAL", 10)) |
			color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
	});
}

ftxui::Element BuildQualityBar(int signalDbm, int quality)
{
	using namespace ftxui;
	constexpr int kQualBarWidth = 10;

	int filled = quality * kQualBarWidth / 100;
	std::string label =
		CenterText(std::to_string(quality) + "%", kQualBarWidth);
	return hbox({
		text(label.substr(0, static_cast<size_t>(filled))) |
			color(Color::Black) | bgcolor(theme::SignalColor(signalDbm)),
		text(label.substr(static_cast<size_t>(filled))) |
			color(theme::SignalColor(signalDbm)) | bgcolor(Color::GrayDark),
	});
}

ftxui::Element BuildDataRow(const wifi::Network& net, bool selected)
{
	using namespace ftxui;

	auto sep = []
	{ return text(" | ") | color(theme::Color(theme::UiColor::Muted)); };

	std::string prefix = net._connected ? "[*] " : " -  ";
	auto prefixColor = net._connected
						   ? theme::Color(theme::UiColor::ConnectedNetwork)
						   : theme::Color(theme::UiColor::Muted);

	std::string ssid =
		net._ssid.empty() ? "???" : SanitizeForTerminal(net._ssid);
	if (ssid.empty())
		ssid = "???";
	auto rowColor = net._connected
						? theme::Color(theme::UiColor::ConnectedNetwork)
						: theme::Color(theme::UiColor::NetworkRow);

	std::string widthStr =
		net._widthMhz > 0 ? std::to_string(net._widthMhz) : "-";
	std::string rateStr =
		net._maxRateMbps > 0 ? std::to_string(net._maxRateMbps) + "M" : "-";

	// Everything up to (and including) the separator before QUAL participates
	// in row inversion. QUAL is appended outside so its hand-painted background
	// stays correct on the selected row.
	auto mainPart = hbox({
		text(prefix) | color(prefixColor),
		text(PadRight(ssid, 24)) | color(rowColor),
		sep(),
		text(PadRight(net._bssid, 17)) |
			color(theme::Color(theme::UiColor::Muted)),
		sep(),
		text(PadRight(std::to_string(net._channel), 4)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(wifi::BandLabel(net._band), 4)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(widthStr, 3)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(wifi::StandardLabel(net._standard), 3)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(wifi::SecurityLabel(net._security), 7)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(rateStr, 7)) |
			color(theme::Color(theme::UiColor::DataValue)),
		sep(),
		text(PadRight(std::to_string(net._signalDbm) + " dBm", 8)) |
			color(theme::SignalColor(net._signalDbm)),
		sep(),
	});

	if (selected)
		mainPart = mainPart | inverted;

	return hbox({
		mainPart,
		BuildQualityBar(net._signalDbm, net.SignalQuality()),
	});
}

} // namespace

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
	if (event == ftxui::Event::Character('s'))
	{
		switch (_sortMode)
		{
		case SortMode::Signal:
			_sortMode = SortMode::SSID;
			break;
		case SortMode::SSID:
			_sortMode = SortMode::Channel;
			break;
		case SortMode::Channel:
			_sortMode = SortMode::Signal;
			break;
		}
		_selectedRow = 0;
		return true;
	}
	return false;
}

ftxui::Element
NetworkTablePanel::Render(const std::vector<wifi::Network>& networks)
{
	using namespace ftxui;

	std::vector<wifi::Network> sorted = networks;
	SortByMode(sorted, _sortMode);

	int rowCount = static_cast<int>(sorted.size());
	if (_selectedRow >= rowCount && rowCount > 0)
		_selectedRow = rowCount - 1;
	if (_selectedRow < 0)
		_selectedRow = 0;

	std::vector<Element> rows;
	rows.push_back(BuildHeaderRow(_sortMode));
	rows.push_back(separator() | color(theme::Color(theme::UiColor::Border)));

	if (sorted.empty())
	{
		rows.push_back(text("  (no networks — try: sudo wifiscan)") |
					   color(theme::Color(theme::UiColor::Muted)));
	}

	for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		bool selected = rowIndex == _selectedRow;
		auto row =
			BuildDataRow(sorted[static_cast<size_t>(rowIndex)], selected);
		if (selected)
			row = row | focus;
		rows.push_back(row);
	}

	return vbox(rows) | yframe;
}

} // namespace ui
