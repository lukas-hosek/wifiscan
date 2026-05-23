// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "IPanel.hpp"

namespace ui
{

enum class ColumnType
{
	SSID,
	BSSID,
	Channel,
	Band,
	Width,
	Standard,
	Security,
	Rate,
	Signal,
	Quality
};

class NetworkTablePanel : public IPanel
{
public:
	[[nodiscard]] ftxui::Element
	Render(const std::vector<wifi::Network>& networks, int allocatedHeight) override;
	[[nodiscard]] std::string GetTitle() const override { return "Networks"; }
	bool HandleEvent(ftxui::Event event) override;

private:
	// Index of the currently highlighted row in the network table
	int _selectedRow{0};

	// Active sort column; cycled by pressing 's'. Always equals one of the
	// types listed in kSortableColumns (in NetworkTablePanel.cpp).
	ColumnType _sortColumn{ColumnType::Signal};
};

} // namespace ui
