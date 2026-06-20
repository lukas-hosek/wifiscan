// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/Network.hpp"
#include <imgui.h>
#include <span>
#include <unordered_set>
#include <vector>

namespace gui
{

// WiFi-analyzer-style spectrum: each network is drawn as an unfilled outlined
// bar spanning the channels it covers, peaking at its signal strength, with its
// SSID centered above. Deliberately diverges from the per-channel-bar TUI view;
// it only reuses the channel/coverage data helpers from ui::SpectrumPanel.
class SpectrumPanel
{
public:
	void Render(const std::vector<wifi::Network>& networks);

private:
	// Index into {GHz2_4, GHz5, GHz6}; selected via the band buttons or the Tab
	// key. Single source of truth (no internal tab-bar state to fight with).
	int _activeBandIndex{0};

	void RenderBandSelector();
	void RenderYAxis(float leftMarginW, float availH, float topMargin,
		float barAreaH, float scale) const;
	void DrawGrid(ImDrawList* draw, ImVec2 origin, float plotW,
		float baselineAbsY, float barAreaH) const;
	void DrawChannelTicks(ImDrawList* draw, std::span<const int> channels,
		float slotW, ImVec2 origin, float baselineAbsY,
		const std::unordered_set<int>& primaries) const;
	void DrawNetworkBars(ImDrawList* draw,
		const std::vector<wifi::Network>& networks, wifi::Band band,
		std::span<const int> channels, float slotW, ImVec2 origin,
		float barAreaH, float baselineAbsY, float lineH) const;
};

} // namespace gui
