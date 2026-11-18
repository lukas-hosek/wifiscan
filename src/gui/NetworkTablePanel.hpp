// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/Network.hpp"
#include <imgui.h>
#include <string>
#include <vector>

namespace gui
{

// Scrollable, sortable network table. Columns drop in a fixed order as the panel narrows. The selected row is the
// BSSID shared with the spectrum panel: picking a row focuses that AP in the spectrum, and the other way round.
class NetworkTablePanel
{
public:
	// Table columns, in display order
	enum class ColumnType
	{
		SSID,
		BSSID,
		Channel,
		Width,
		Band,
		Standard,
		Security,
		Rate,
		Signal
	};

	void Render(const std::vector<wifi::Network>& networks, std::string& selectedBssid);

private:
	// Screen-space box of one table cell
	struct CellBox
	{
		ImVec2 min;
		ImVec2 max;

		// Top y that centres something of this height in the cell
		[[nodiscard]] float CenteredY(float height) const { return min.y + (max.y - min.y - height) * 0.5f; }
	};

	[[nodiscard]] std::vector<wifi::Network> SortedNetworks(const std::vector<wifi::Network>& networks) const;

	// Up/Down move the selection through the sorted rows. Returns true when it changed.
	static bool HandleKeyboard(const std::vector<wifi::Network>& sorted, std::string& selectedBssid);

	// Sortable headers are buttons: clicking the active column flips the direction, another column starts with
	// its natural direction.
	void RenderHeaderCell(ColumnType type, const char* header, bool sortable, bool rightAligned, const CellBox& cell);

	// 1 px rule under the pinned header, across the whole table. Call from the first header cell.
	static void DrawHeaderRule(const CellBox& headerCell);

	// Full-row hit area and selection highlight; must be the first item in the row.
	static void RenderRowSelectable(
		const wifi::Network& net, bool isSelected, bool revealSelection, float rowHeight, std::string& selectedBssid);

	// Draws one data cell with the draw list, vertically centred in the row
	static void RenderCell(ColumnType type, const wifi::Network& net, const CellBox& cell);

	ColumnType _sortColumn{ColumnType::Signal};
	bool _sortAscending{false};

	// The selection as of the end of the last frame. A change made elsewhere (spectrum click) scrolls the row
	// into view.
	std::string _lastSeenSelection;
};

} // namespace gui
