// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "ChannelAxis.hpp"
#include <span>
#include <string>

namespace gui
{

// One way of drawing a band's APs over its channel axis (Spectrum or Lanes mode). SpectrumPanel owns the band,
// the scrolling windows and the selection; a view draws inside the windows the panel opens.
class IChannelView
{
public:
	virtual ~IChannelView() = default;

	// The pinned column left of the plot (dBm axis or row captions), drawn into a child window at its cursor.
	// plotHeight is the visible plot height without the horizontal scrollbar, the same value RenderPlot gets.
	virtual void RenderGutter(float plotHeight) = 0;

	// Draws the APs inside the plot's scrolling child window, starting at the cursor. aps is sorted strongest
	// first; focused is the selected AP when it is on this band. A click on an AP toggles selectedBssid. With
	// revealFocused set, the view scrolls the focused AP into sight if it isn't already.
	virtual void RenderPlot(const ChannelAxis& axis, std::span<const BandAp> aps, const BandAp* focused,
		float plotHeight, bool revealFocused, std::string& selectedBssid) = 0;
};

} // namespace gui
