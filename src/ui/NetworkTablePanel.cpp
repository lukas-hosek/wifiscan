// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NetworkTablePanel.hpp"
#include "TextUtil.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <functional>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
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

// Display width of the always-visible columns (prefix + SSID + CH + BAND + W +
// STD + SIGNAL, including their inter-column separators): 65.
static constexpr int kPermanentWidth = 65;

struct CollapsibleCol
{
	const char* header;
	int width;
	// Drop priority: lower collapseRank = first to vanish as the terminal
	// narrows. Values must be unique across entries.
	int collapseRank;
	// True when this column appears right after SSID in the row layout.
	bool afterSsid;
	// True when this column appears after SIGNAL in the row layout.
	bool afterSignal;
	// True when the element must be rendered outside the row-inversion group
	// (needed for columns with hand-painted backgrounds, e.g. QUAL).
	// Its separator is still emitted inside the inversion group.
	bool outsideHighlight;
	std::function<ftxui::Element(const wifi::Network&)> data;
};

// Edit this array to change which columns collapse and in what order.
// Entries are in DISPLAY ORDER. collapseRank controls drop priority.
// Each entry contributes (width + 3) display columns (content + separator).
// afterSsid=true: rendered right after SSID; afterSignal=true: after SIGNAL.
static const CollapsibleCol kCollapsibleCols[] = {
	{
		"BSSID", 17, 3, true, false, false,
		[](const wifi::Network& n)
		{
			return ftxui::text(PadRight(n._bssid, 17)) |
				   ftxui::color(theme::Color(theme::UiColor::Muted));
		},
	},
	{
		"SEC", 7, 4, false, false, false,
		[](const wifi::Network& n)
		{
			return ftxui::text(PadRight(wifi::SecurityLabel(n._security), 7)) |
				   ftxui::color(theme::Color(theme::UiColor::DataValue));
		},
	},
	{
		"RATE", 7, 1, false, false, false,
		[](const wifi::Network& n)
		{
			std::string s =
				n._maxRateMbps > 0 ? std::to_string(n._maxRateMbps) + "M" : "-";
			return ftxui::text(PadRight(s, 7)) |
				   ftxui::color(theme::Color(theme::UiColor::DataValue));
		},
	},
	{
		"QUAL", 10, 2, false, true, true,
		[](const wifi::Network& n)
		{
			return BuildQualityBar(n._signalDbm, n.SignalQuality());
		},
	},
};

// Returns the highest collapseRank that is currently being suppressed.
// Columns with collapseRank <= threshold are hidden; those above it are shown.
int CollapseThreshold(int termWidth)
{
	std::vector<std::pair<int, int>> drops; // (collapseRank, width contribution)
	drops.reserve(std::size(kCollapsibleCols));
	for (const auto& col : kCollapsibleCols)
		drops.push_back({col.collapseRank, col.width + 3});
	std::ranges::sort(drops);

	int width = kPermanentWidth;
	for (auto [rank, w] : drops)
		width += w;

	int threshold = 0;
	for (auto [rank, w] : drops)
	{
		if (width <= termWidth)
			break;
		threshold = rank;
		width -= w;
	}
	return threshold;
}

ftxui::Element BuildHeaderRow(SortMode active, int threshold)
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

	std::vector<Element> items;
	items.push_back(text("    ") | color(theme::Color(theme::UiColor::Muted)));
	items.push_back(text(hdrText("SSID", 24, SortMode::SSID)) |
					color(hdrColor(SortMode::SSID)) | bold);
	for (const auto& col : kCollapsibleCols)
	{
		if (!col.afterSsid || col.collapseRank <= threshold)
			continue;
		items.push_back(sep());
		items.push_back(text(PadRight(col.header, col.width)) |
						color(theme::Color(theme::UiColor::ColumnHeader)) | bold);
	}
	items.push_back(sep());
	items.push_back(text(hdrText("CH", 4, SortMode::Channel)) |
					color(hdrColor(SortMode::Channel)) | bold);
	items.push_back(sep());
	items.push_back(text(PadRight("BAND", 4)) |
					color(theme::Color(theme::UiColor::ColumnHeader)) | bold);
	items.push_back(sep());
	items.push_back(text(PadRight("W", 3)) |
					color(theme::Color(theme::UiColor::ColumnHeader)) | bold);
	items.push_back(sep());
	items.push_back(text(PadRight("STD", 3)) |
					color(theme::Color(theme::UiColor::ColumnHeader)) | bold);

	for (const auto& col : kCollapsibleCols)
	{
		if (col.afterSsid || col.afterSignal || col.collapseRank <= threshold)
			continue;
		items.push_back(sep());
		items.push_back(text(PadRight(col.header, col.width)) |
						color(theme::Color(theme::UiColor::ColumnHeader)) | bold);
	}

	items.push_back(sep());
	items.push_back(text(hdrText("SIGNAL", 8, SortMode::Signal)) |
					color(hdrColor(SortMode::Signal)) | bold);

	for (const auto& col : kCollapsibleCols)
	{
		if (!col.afterSignal || col.collapseRank <= threshold)
			continue;
		items.push_back(sep());
		items.push_back(text(PadRight(col.header, col.width)) |
						color(theme::Color(theme::UiColor::ColumnHeader)) | bold);
	}

	return hbox(items);
}

ftxui::Element BuildDataRow(const wifi::Network& net, bool selected, int threshold)
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

	// Fixed columns and before-signal collapsibles go into mainPart, which
	// receives the row-inversion decorator when selected. outsideHighlight
	// columns (QUAL) are appended after inversion so their painted background
	// is not corrupted; their separator is still inside mainPart so it inverts.
	std::vector<Element> mainItems;
	mainItems.push_back(text(prefix) | color(prefixColor));
	mainItems.push_back(text(PadRight(ssid, 24)) | color(rowColor));
	for (const auto& col : kCollapsibleCols)
	{
		if (!col.afterSsid || col.collapseRank <= threshold)
			continue;
		mainItems.push_back(sep());
		mainItems.push_back(col.data(net));
	}
	mainItems.push_back(sep());
	mainItems.push_back(text(PadRight(std::to_string(net._channel), 4)) |
						color(theme::Color(theme::UiColor::DataValue)));
	mainItems.push_back(sep());
	mainItems.push_back(text(PadRight(wifi::BandLabel(net._band), 4)) |
						color(theme::Color(theme::UiColor::DataValue)));
	mainItems.push_back(sep());
	mainItems.push_back(text(PadRight(widthStr, 3)) |
						color(theme::Color(theme::UiColor::DataValue)));
	mainItems.push_back(sep());
	mainItems.push_back(text(PadRight(wifi::StandardLabel(net._standard), 3)) |
						color(theme::Color(theme::UiColor::DataValue)));

	for (const auto& col : kCollapsibleCols)
	{
		if (col.afterSsid || col.afterSignal || col.collapseRank <= threshold)
			continue;
		mainItems.push_back(sep());
		mainItems.push_back(col.data(net));
	}

	mainItems.push_back(sep());
	mainItems.push_back(
		text(PadRight(std::to_string(net._signalDbm) + " dBm", 8)) |
		color(theme::SignalColor(net._signalDbm)));

	for (const auto& col : kCollapsibleCols)
	{
		if (!col.afterSignal || col.collapseRank <= threshold)
			continue;
		mainItems.push_back(sep());
		if (!col.outsideHighlight)
			mainItems.push_back(col.data(net));
	}

	auto mainPart = hbox(mainItems);
	if (selected)
		mainPart = mainPart | inverted;

	std::vector<Element> outer = {mainPart};
	for (const auto& col : kCollapsibleCols)
	{
		if (!col.afterSignal || !col.outsideHighlight || col.collapseRank <= threshold)
			continue;
		outer.push_back(col.data(net));
	}
	return hbox(outer);
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
NetworkTablePanel::Render(const std::vector<wifi::Network>& networks,
						  int /*allocatedHeight*/)
{
	using namespace ftxui;

	std::vector<wifi::Network> sorted = networks;
	SortByMode(sorted, _sortMode);

	int rowCount = static_cast<int>(sorted.size());
	if (_selectedRow >= rowCount && rowCount > 0)
		_selectedRow = rowCount - 1;
	if (_selectedRow < 0)
		_selectedRow = 0;

	int threshold = CollapseThreshold(ftxui::Terminal::Size().dimx);

	std::vector<Element> rows;
	rows.push_back(BuildHeaderRow(_sortMode, threshold));
	rows.push_back(separator() | color(theme::Color(theme::UiColor::Border)));

	if (sorted.empty())
	{
		rows.push_back(text("  (no networks — try: sudo wifiscan)") |
					   color(theme::Color(theme::UiColor::Muted)));
	}

	for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		bool selected = rowIndex == _selectedRow;
		auto row = BuildDataRow(sorted[static_cast<size_t>(rowIndex)], selected,
								threshold);
		if (selected)
			row = row | focus;
		rows.push_back(row);
	}

	return vbox(rows) | yframe;
}

} // namespace ui
