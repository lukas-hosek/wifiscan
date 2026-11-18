// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "ChannelAxis.hpp"
#include "LanesView.hpp"
#include "SpectrumView.hpp"
#include "wifi/ChannelPlan.hpp"
#include "wifi/Network.hpp"
#include <string>
#include <vector>

namespace gui
{

// Every AP of one band over its channel axis, shown in one of two modes: Spectrum (signal humps) or Lanes (packed
// bars plus a congestion row). The panel owns the band and mode toggles, the scrolling windows and following the
// shared selection; the active IChannelView does the drawing.
class SpectrumPanel
{
public:
	void Render(const std::vector<wifi::Network>& networks, std::string& selectedBssid);

private:
	// The display modes, in the order of the mode toggle
	enum class DisplayMode
	{
		Spectrum,
		Lanes
	};

	// Tab / Shift+Tab cycle the band, M toggles the mode (global shortcuts).
	void HandleKeyboard();
	void RenderToolbar();

	// When the selection changed outside this panel (e.g. a table row), switch to the selected AP's band. Returns
	// true when the selected AP should be scrolled into view.
	bool FollowSelection(const std::vector<wifi::Network>& networks, const std::string& selectedBssid);

	// The APs that sit on the plan's axis, strongest first
	[[nodiscard]] static std::vector<BandAp> CollectBandAps(
		const std::vector<wifi::Network>& networks, const wifi::ChannelPlan& plan);

	[[nodiscard]] IChannelView& ActiveView();

	// Index into wifi::kSupportedBands; the band toggle edits it in place, so it is the single source of truth.
	int _activeBandIndex{0};
	// DisplayMode as an index, for the same reason
	int _modeIndex{static_cast<int>(DisplayMode::Spectrum)};
	// The selection as of the end of the last frame. Changes made by this panel's own views are already in it,
	// so only changes from elsewhere are followed.
	std::string _lastSeenSelection;

	SpectrumView _spectrumView;
	LanesView _lanesView;
};

} // namespace gui
