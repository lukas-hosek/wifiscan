// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "ApLabel.hpp"
#include "IChannelView.hpp"
#include <vector>

namespace gui
{

// Lanes mode: one bar per AP over the channels it occupies, packed into rows so that no bars or labels overlap.
// Signal is shown by the bar color and the dBm number rather than by height. A strip under each bar marks the
// primary 20 MHz channel, and a congestion row above the lanes counts the APs on every channel. The channel and
// congestion rows stay pinned while the lanes scroll vertically.
class LanesView : public IChannelView
{
public:
	void RenderGutter(float plotHeight) override;
	void RenderPlot(const ChannelAxis& axis, std::span<const BandAp> aps, const BandAp* focused, float plotHeight,
		bool revealFocused, std::string& selectedBssid) override;

private:
	// An AP's bar after lane packing. X values are in px relative to the plot's content origin.
	struct PlacedBar
	{
		const BandAp* ap;
		ApLabel label;
		int lane;
		// The bar plus room for its label; this is what packing keeps apart and what a click hits
		float reservedLeft;
		float reservedRight;
		float barLeft;
		float barRight;
		float primaryLeft;
		float primaryRight;
	};

	// First-fit packing, strongest AP first, so the strongest APs end up in the top lanes.
	[[nodiscard]] static std::vector<PlacedBar> PackLanes(
		const ChannelAxis& axis, std::span<const BandAp> aps, int& laneCount);

	[[nodiscard]] static float LaneTop(int lane);

	static void DrawBar(ImDrawList* draw, ImVec2 origin, const PlacedBar& bar, bool isFocused, float alpha);

	// For every channel slot, how many APs overlap it; darker blue means busier
	static void DrawCongestionRow(
		ImDrawList* draw, ImVec2 origin, float top, const ChannelAxis& axis, std::span<const BandAp> aps);
};

} // namespace gui
