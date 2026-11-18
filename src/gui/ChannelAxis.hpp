// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/Network.hpp"
#include <imgui.h>
#include <span>
#include <unordered_set>

namespace gui
{

// An AP's extent on a band's channel axis in slot units: slot i covers [i, i + 1). Values can be fractional
// (2.4 GHz slots are only 5 MHz wide) and can reach past the first or last slot.
struct SlotSpan
{
	// The whole occupied spectrum
	float lo;
	float hi;
	// The primary channel's slot
	float primaryLo;
	float primaryHi;
};

// One AP placed on the active band's channel axis
struct BandAp
{
	const wifi::Network* network;
	SlotSpan span;
};

// Horizontal geometry of the active band's channel axis, shared by every chart view. X values are in px relative
// to the plot's content origin (left edge of the first slot).
struct ChannelAxis
{
	std::span<const int> channels;
	float slotWidth;
	float plotWidth;
	// Primary channels of the APs on the band; their tick labels are highlighted
	std::unordered_set<int> primaryChannels;

	[[nodiscard]] float SlotX(float slot) const { return slot * slotWidth; }

	// The AP's occupied range clipped to the plot. Empty (left >= right) when it lies entirely off the axis.
	[[nodiscard]] float ClippedLeft(const SlotSpan& span) const;
	[[nodiscard]] float ClippedRight(const SlotSpan& span) const;

	// Channel numbers centred in their slots, in a row starting at top: primary channels in the accent color and
	// semibold, the rest muted.
	void DrawTicks(ImDrawList* draw, ImVec2 origin, float top, float rowHeight) const;

	// Scrolls the current window horizontally to centre the span, unless it is already fully visible.
	void ScrollIntoView(const SlotSpan& span) const;
};

} // namespace gui
