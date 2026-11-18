// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "SpectrumPanel.hpp"
#include "ImGuiExtras.hpp"
#include <algorithm>
#include <array>
#include <imgui.h>
#include <span>

namespace gui
{

namespace
{

// Same order as wifi::kSupportedBands
constexpr std::array<const char*, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};
// Same order as SpectrumPanel::DisplayMode
constexpr std::array<const char*, 2> kModeLabels{"Spectrum", "Lanes"};

// Width of the pinned column left of the plot, in px at 1x scale
constexpr float kGutterWidth = 40.0f;

// Minimum on-screen width of a single channel slot, in px at 1x scale. When the band has more channels than fit at
// this width, the plot scrolls horizontally.
constexpr float kMinSlotWidth = 26.0f;

} // namespace

void SpectrumPanel::HandleKeyboard()
{
	int bandCount = static_cast<int>(wifi::kSupportedBands.size());
	if (ImGui::IsKeyPressed(ImGuiKey_Tab))
	{
		if (ImGui::GetIO().KeyShift)
			_activeBandIndex = (_activeBandIndex + bandCount - 1) % bandCount;
		else
			_activeBandIndex = (_activeBandIndex + 1) % bandCount;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_M, false))
		_modeIndex = (_modeIndex + 1) % static_cast<int>(kModeLabels.size());
}

void SpectrumPanel::RenderToolbar()
{
	SegmentedControl("##band", std::span<const char* const>{kBandLabels}, _activeBandIndex);

	// The mode toggle sits at the right edge of the panel.
	float modeWidth = SegmentedControlWidth(std::span<const char* const>{kModeLabels});
	ImGui::SameLine();
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, ImGui::GetContentRegionAvail().x - modeWidth));
	SegmentedControl("##mode", std::span<const char* const>{kModeLabels}, _modeIndex);
}

bool SpectrumPanel::FollowSelection(const std::vector<wifi::Network>& networks, const std::string& selectedBssid)
{
	if (selectedBssid == _lastSeenSelection || selectedBssid.empty())
		return false;

	auto selected = std::find_if(networks.begin(), networks.end(),
		[&](const wifi::Network& net)
		{
			return net._bssid == selectedBssid;
		});
	if (selected == networks.end())
		return false;

	auto band = std::find(wifi::kSupportedBands.begin(), wifi::kSupportedBands.end(), selected->_band);
	if (band != wifi::kSupportedBands.end())
		_activeBandIndex = static_cast<int>(band - wifi::kSupportedBands.begin());
	return true;
}

std::vector<BandAp> SpectrumPanel::CollectBandAps(
	const std::vector<wifi::Network>& networks, const wifi::ChannelPlan& plan)
{
	std::vector<BandAp> aps;
	for (const wifi::Network& net : networks)
	{
		if (std::optional<wifi::SlotSpan> span = plan.SpanOf(net))
			aps.push_back(BandAp{&net, *span});
	}
	std::stable_sort(aps.begin(), aps.end(),
		[](const BandAp& a, const BandAp& b)
		{
			return a.network->_signalDbm > b.network->_signalDbm;
		});
	return aps;
}

IChannelView& SpectrumPanel::ActiveView()
{
	if (static_cast<DisplayMode>(_modeIndex) == DisplayMode::Lanes)
		return _lanesView;
	return _spectrumView;
}

void SpectrumPanel::Render(const std::vector<wifi::Network>& networks, std::string& selectedBssid)
{
	HandleKeyboard();
	bool revealSelection = FollowSelection(networks, selectedBssid);
	RenderToolbar();

	wifi::ChannelPlan plan(wifi::kSupportedBands[static_cast<size_t>(_activeBandIndex)]);
	std::vector<BandAp> aps = CollectBandAps(networks, plan);

	// The selected AP is focused only while it is on the band being shown.
	const BandAp* focused = nullptr;
	if (!selectedBssid.empty())
	{
		auto it = std::find_if(aps.begin(), aps.end(),
			[&](const BandAp& ap)
			{
				return ap.network->_bssid == selectedBssid;
			});
		if (it != aps.end())
			focused = &*it;
	}

	float scale = DpiScale();
	float availHeight = ImGui::GetContentRegionAvail().y;
	// The plot always shows its horizontal scrollbar, which takes this much off the visible plot height. The
	// gutter uses the same value so its dBm labels line up with the plot.
	float plotHeight = std::max(1.0f, availHeight - ImGui::GetStyle().ScrollbarSize);
	IChannelView& view = ActiveView();

	ImGui::BeginChild("gutter", ImVec2(kGutterWidth * scale, availHeight), ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	view.RenderGutter(plotHeight);
	ImGui::EndChild();
	ImGui::SameLine(0.0f, 0.0f);

	// One plot window for both modes, so switching modes keeps the horizontal scroll position. Lanes mode can
	// also scroll vertically; Spectrum mode never has content taller than the window.
	if (ImGui::BeginChild("plot", ImVec2(0.0f, availHeight), ImGuiChildFlags_None,
			ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		std::span<const int> channels = plan.Channels();
		int channelCount = std::max(1, static_cast<int>(channels.size()));
		float slotWidth = std::max(kMinSlotWidth * scale, ImGui::GetContentRegionAvail().x / channelCount);

		ChannelAxis axis{channels, slotWidth, slotWidth * static_cast<float>(channelCount), {}};
		for (const BandAp& ap : aps)
			axis.primaryChannels.insert(ap.network->_channel);

		// Left/Right arrow keys scroll by one slot (global shortcut).
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
			ImGui::SetScrollX(ImGui::GetScrollX() + slotWidth);
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
			ImGui::SetScrollX(ImGui::GetScrollX() - slotWidth);

		view.RenderPlot(axis, aps, focused, plotHeight, revealSelection, selectedBssid);
	}
	ImGui::EndChild();

	_lastSeenSelection = selectedBssid;
}

} // namespace gui
