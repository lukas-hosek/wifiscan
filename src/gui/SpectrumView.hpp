// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "ApLabel.hpp"
#include "IChannelView.hpp"
#include <vector>

namespace gui
{

// Spectrum mode: each AP is a translucent hump over the channels it occupies, its plateau at the AP's signal
// strength, labelled "SSID dBm". Labels are placed so they never overlap; one pushed away from its peak gets a
// leader line back down to it. Clicking a label focuses that AP and fades the others.
class SpectrumView : public IChannelView
{
public:
	void RenderGutter(float plotHeight) override;
	void RenderPlot(const ChannelAxis& axis, std::span<const BandAp> aps, const BandAp* focused, float plotHeight,
		bool revealFocused, std::string& selectedBssid) override;

private:
	// Vertical layout in px relative to the top of the plot
	struct ChartGeometry
	{
		float chartTop;
		// The bottom line of the dBm axis, where humps stand and the channel ticks start
		float baseline;

		[[nodiscard]] float YOf(int signalDbm) const;
	};

	// A label box after placement, in px relative to the plot's content origin
	struct PlacedLabel
	{
		const BandAp* ap;
		ApLabel text;
		ImVec2 min;
		ImVec2 max;
		// Where its leader line meets the hump
		float anchorX;
		float peakY;
	};

	[[nodiscard]] static ChartGeometry Geometry(float plotHeight);

	// Greedy placement, weakest AP first: each label starts just above its peak and climbs over any label it
	// collides with, so where two labels compete for space the stronger AP's ends up above. Returned in placement
	// order, which is also the draw order.
	[[nodiscard]] static std::vector<PlacedLabel> PlaceLabels(
		const ChannelAxis& axis, std::span<const BandAp> aps, const ChartGeometry& geometry);

	static void DrawGrid(ImDrawList* draw, ImVec2 origin, float plotWidth, const ChartGeometry& geometry);
	static void DrawHump(ImDrawList* draw, ImVec2 origin, const ChannelAxis& axis, const BandAp& ap,
		const ChartGeometry& geometry, bool isFocused, float alpha);
	// The vertical line from a label pushed off its peak back down to the peak; nothing when it sits on the peak
	static void DrawLeader(ImDrawList* draw, ImVec2 origin, const PlacedLabel& label, float alpha);
	static void DrawLabelBox(ImDrawList* draw, ImVec2 origin, const PlacedLabel& label, float alpha);
};

} // namespace gui
