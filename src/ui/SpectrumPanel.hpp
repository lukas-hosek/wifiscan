// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "IPanel.hpp"

namespace ui
{

class SpectrumPanel : public IPanel
{
public:
	[[nodiscard]] ftxui::Element
	Render(const std::vector<wifi::Network>& networks, int allocatedHeight) override;
	[[nodiscard]] std::string GetTitle() const override { return "Spectrum"; }
	[[nodiscard]] bool HandleEvent(ftxui::Event event) override;

private:
	// Index into {GHz2_4, GHz5, GHz6}; cycled by Tab / Shift+Tab
	int _activeBandIndex{0};
	// First visible channel index within the current band's full channel list
	int _scrollOffset{0};
	// When true, omit the currently associated AP from the spectrum so its bar
	// (typically the tallest) doesn't dwarf neighbouring networks. Toggled with
	// 'e'.
	bool _hideConnected{false};
};

} // namespace ui
