// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NetworkTablePanel.hpp"
#include "Theme.hpp"
#include "utils/TextUtil.hpp"
#include <algorithm>
#include <array>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <functional>
#include <ranges>

namespace tui
{

using namespace utils;

namespace
{

// Per-column cell renderers. Each must produce an element exactly the column's
// declared width.

ftxui::Element RenderSsid(const wifi::Network& net)
{
	using namespace ftxui;
	std::string ssid = net._ssid.empty() ? "???" : net._ssid;
	if (ssid.empty())
		ssid = "???";
	if (net._connected)
	{
		return hbox({
			text("* ") | color(theme::Color(theme::UiColor::ConnectedNetwork)),
			text(PadRight(ssid, 26)) | color(theme::Color(theme::UiColor::ConnectedNetwork)),
		});
	}
	return text("  " + PadRight(ssid, 26)) | color(theme::Color(theme::UiColor::NetworkRow));
}

ftxui::Element RenderBssid(const wifi::Network& net)
{
	return ftxui::text(PadRight(net._bssid, 17)) | ftxui::color(theme::Color(theme::UiColor::Muted));
}

ftxui::Element RenderChannel(const wifi::Network& net)
{
	return ftxui::text(PadRight(std::to_string(net._channel), 4)) |
		ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderBand(const wifi::Network& net)
{
	return ftxui::text(PadRight(wifi::BandLabel(net._band), 4)) | ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderWidth(const wifi::Network& net)
{
	std::string s = net._widthMhz > 0 ? std::to_string(net._widthMhz) : "-";
	return ftxui::text(PadRight(s, 3)) | ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderStandard(const wifi::Network& net)
{
	return ftxui::text(PadRight(wifi::StandardLabel(net._standard), 3)) |
		ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderSecurity(const wifi::Network& net)
{
	return ftxui::text(PadRight(wifi::SecurityLabel(net._security), 7)) |
		ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderRate(const wifi::Network& net)
{
	std::string s = net._maxRateMbps > 0 ? std::to_string(net._maxRateMbps) + "M" : "-";
	return ftxui::text(PadRight(s, 7)) | ftxui::color(theme::Color(theme::UiColor::DataValue));
}

ftxui::Element RenderSignal(const wifi::Network& net)
{
	return ftxui::text(PadRight(std::to_string(net._signalDbm) + " dBm", 8)) |
		ftxui::color(theme::SignalColor(net._signalDbm));
}

ftxui::Element RenderQuality(const wifi::Network& net)
{
	using namespace ftxui;
	constexpr int kQualBarWidth = 10;

	int quality = net.SignalQuality();
	int filled = quality * kQualBarWidth / 100;
	std::string label = CenterText(std::to_string(quality) + "%", kQualBarWidth);
	return hbox({
		text(label.substr(0, static_cast<size_t>(filled))) | color(Color::Black) |
			bgcolor(theme::SignalColor(net._signalDbm)),
		text(label.substr(static_cast<size_t>(filled))) | color(theme::SignalColor(net._signalDbm)) |
			bgcolor(Color::GrayDark),
	});
}

struct ColumnDescriptor
{
	ColumnType type;
	// Header label
	const char* header;
	// Content width in display columns; the " | " separator between visible
	// columns is NOT counted here
	int width;
	// False for columns whose cell paints its own background (currently only
	// QUAL) — those are appended outside the row-inversion wrapper so row
	// selection does not flip their bar. Such columns must sit at the END of
	// the display order.
	bool participatesRowHighlight;
	ftxui::Element (*renderCell)(const wifi::Network&);
};

// Display order. The SSID descriptor's width (28) covers "* " + 26-char SSID
// for connected networks, or the full 28-char SSID for others.
static const ColumnDescriptor kColumns[] = {
	{ColumnType::SSID, "  SSID", 28, true, RenderSsid},
	{ColumnType::BSSID, "BSSID", 17, true, RenderBssid},
	{ColumnType::Channel, "CH", 4, true, RenderChannel},
	{ColumnType::Band, "BAND", 4, true, RenderBand},
	{ColumnType::Width, "W", 3, true, RenderWidth},
	{ColumnType::Standard, "STD", 3, true, RenderStandard},
	{ColumnType::Security, "SEC", 7, true, RenderSecurity},
	{ColumnType::Rate, "RATE", 7, true, RenderRate},
	{ColumnType::Signal, "SIGNAL", 8, true, RenderSignal},
	{ColumnType::Quality, "QUAL", 10, false, RenderQuality},
};

// Drop order: first entry is the first to vanish as the terminal narrows.
// Any ColumnType not listed here is permanent and never hidden.
static constexpr ColumnType kDropOrder[] = {
	ColumnType::Rate,
	ColumnType::Quality,
	ColumnType::BSSID,
	ColumnType::Security,
};

// Sortable columns paired with comparators, in cycle order for the 's' key.
// Membership in this array defines "this column is sortable"; the descriptor
// has no sortable field. _sortColumn is only ever assigned to one of these
// types, so the header renderer can compare col.type == _sortColumn directly.
struct SortableColumn
{
	ColumnType type;
	bool (*less)(const wifi::Network&, const wifi::Network&);
};

static const SortableColumn kSortableColumns[] = {
	{ColumnType::Signal,
		[](const wifi::Network& a, const wifi::Network& b)
		{
			return a._signalDbm != b._signalDbm ? a._signalDbm > b._signalDbm : a._ssid < b._ssid;
		}},
	{ColumnType::SSID,
		[](const wifi::Network& a, const wifi::Network& b)
		{
			return a._ssid < b._ssid;
		}},
	{ColumnType::Channel,
		[](const wifi::Network& a, const wifi::Network& b)
		{
			if (a._band != b._band)
				return a._band < b._band;
			if (a._channel != b._channel)
				return a._channel < b._channel;
			return a._ssid < b._ssid;
		}},
};

using VisibilityMap = std::array<bool, std::size(kColumns)>;

int FindColumnIndex(ColumnType type)
{
	for (size_t i = 0; i < std::size(kColumns); ++i)
	{
		if (kColumns[i].type == type)
			return static_cast<int>(i);
	}
	return -1;
}

VisibilityMap ComputeVisibility(int termWidth)
{
	VisibilityMap visible;
	visible.fill(true);

	auto requiredWidth = [&]
	{
		int w = 0;
		bool first = true;
		for (size_t i = 0; i < std::size(kColumns); ++i)
		{
			if (!visible[i])
				continue;
			if (!first)
				w += 3; // " | "
			w += kColumns[i].width;
			first = false;
		}
		return w;
	};

	for (ColumnType type : kDropOrder)
	{
		if (requiredWidth() <= termWidth)
			break;
		int idx = FindColumnIndex(type);
		if (idx >= 0)
			visible[static_cast<size_t>(idx)] = false;
	}
	return visible;
}

ftxui::Element Separator()
{
	using namespace ftxui;
	return text(" | ") | color(theme::Color(theme::UiColor::Muted));
}

ftxui::Element BuildHeaderRow(ColumnType activeSort, const VisibilityMap& visible)
{
	using namespace ftxui;

	std::vector<Element> items;
	bool first = true;
	for (size_t i = 0; i < std::size(kColumns); ++i)
	{
		if (!visible[i])
			continue;
		if (!first)
			items.push_back(Separator());
		first = false;

		const auto& col = kColumns[i];
		bool active = col.type == activeSort;
		std::string label = active ? std::string(col.header) + "*" : col.header;
		Color c = active ? theme::Color(theme::UiColor::DataValue) : theme::Color(theme::UiColor::ColumnHeader);
		items.push_back(text(PadRight(label, col.width)) | color(c) | bold);
	}
	return hbox(items);
}

ftxui::Element BuildDataRow(const wifi::Network& net, bool selected, const VisibilityMap& visible)
{
	using namespace ftxui;

	std::vector<Element> mainItems;
	std::vector<Element> tailItems;
	bool first = true;
	for (size_t i = 0; i < std::size(kColumns); ++i)
	{
		if (!visible[i])
			continue;

		const auto& col = kColumns[i];
		// Separator always goes into mainItems so it inverts with the row,
		// even when the cell itself ends up in the tail.
		if (!first)
			mainItems.push_back(Separator());
		first = false;

		if (col.participatesRowHighlight)
			mainItems.push_back(col.renderCell(net));
		else
			tailItems.push_back(col.renderCell(net));
	}

	auto main = hbox(mainItems);
	if (selected)
		main = main | inverted;
	if (tailItems.empty())
		return main;

	std::vector<Element> outer{main};
	outer.insert(outer.end(), tailItems.begin(), tailItems.end());
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
		auto it = std::ranges::find(kSortableColumns, _sortColumn, &SortableColumn::type);
		size_t current = it == std::end(kSortableColumns) ? 0 : static_cast<size_t>(it - std::begin(kSortableColumns));
		size_t next = (current + 1) % std::size(kSortableColumns);
		_sortColumn = kSortableColumns[next].type;
		_selectedRow = 0;
		return true;
	}
	return false;
}

ftxui::Element NetworkTablePanel::Render(const std::vector<wifi::Network>& networks, int /*allocatedHeight*/)
{
	using namespace ftxui;

	std::vector<wifi::Network> sorted = networks;
	auto sortIt = std::ranges::find(kSortableColumns, _sortColumn, &SortableColumn::type);
	if (sortIt != std::end(kSortableColumns))
		std::ranges::sort(sorted, sortIt->less);

	int rowCount = static_cast<int>(sorted.size());
	if (_selectedRow >= rowCount && rowCount > 0)
		_selectedRow = rowCount - 1;
	if (_selectedRow < 0)
		_selectedRow = 0;

	VisibilityMap visible = ComputeVisibility(ftxui::Terminal::Size().dimx);

	std::vector<Element> rows;
	rows.push_back(BuildHeaderRow(_sortColumn, visible));
	rows.push_back(separator() | color(theme::Color(theme::UiColor::Border)));

	if (sorted.empty())
	{
		rows.push_back(text("  (no networks — try: sudo wifiscan)") | color(theme::Color(theme::UiColor::Muted)));
	}

	for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		bool selected = rowIndex == _selectedRow;
		auto row = BuildDataRow(sorted[static_cast<size_t>(rowIndex)], selected, visible);
		if (selected)
			row = row | focus;
		rows.push_back(row);
	}

	return vbox(rows) | yframe;
}

} // namespace tui
