// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/Network.hpp"
#include <vector>

namespace gui
{

// Scrollable, sortable network table. The column-visibility structures and
// algorithm mirror tui::NetworkTablePanel (src/tui/NetworkTablePanel.cpp): columns
// drop in a fixed order as the available width shrinks.
class NetworkTablePanel
{
public:
	void Render(const std::vector<wifi::Network>& networks);

private:
	// Index of the highlighted row (position in the sorted list); driven by the
	// Up/Down arrow keys and mouse selection.
	int _selectedRow{0};
};

} // namespace gui
