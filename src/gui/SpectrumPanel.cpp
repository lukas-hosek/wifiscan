// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "SpectrumPanel.hpp"
#include "ImGuiExtras.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <array>
#include <imgui.h>
#include <span>
#include <string>
#include <unordered_set>

namespace gui
{

namespace
{

// Minimum on-screen width of a single channel slot, in pixels at 1x DPI (scaled
// by style.FontScaleDpi at use). When the band has more channels than fit at
// this width, the plot child shows a horizontal scrollbar.
constexpr float kMinSlotW = 26.0f;

// --- Channel/coverage helpers: mirror tui/SpectrumPanel.cpp ---

constexpr std::array<int, 14> kChannels24{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
constexpr std::array<int, 25> kChannels5{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132,
	136, 140, 144, 149, 153, 157, 161, 165};
constexpr std::array<int, 59> kChannels6{1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
	81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173,
	177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233};

constexpr std::array<wifi::Band, 3> kBands{wifi::Band::GHz2_4, wifi::Band::GHz5, wifi::Band::GHz6};
constexpr std::array<const char*, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};

std::span<const int> BandChannels(int bandIndex)
{
	switch (bandIndex)
	{
	case 0:
		return kChannels24;
	case 1:
		return kChannels5;
	default:
		return kChannels6;
	}
}

// Returns the 20-MHz channel slots an AP occupies based on its primary channel
// + reported operating width. Returns {primary} when the width is unknown or we
// lack the centre-frequency hint needed to span. Mirrors
// ui::SpectrumPanel::CoveredChannels.
std::vector<int> CoveredChannels(const wifi::Network& net)
{
	int primary = net._channel;
	if (primary <= 0 || net._widthMhz <= 20)
		return {primary};

	if (net._band == wifi::Band::GHz2_4)
		return {primary};

	if (net._centerFreq1Mhz == 0)
		return {primary};

	uint32_t baseMhz = (net._band == wifi::Band::GHz6) ? 5950U : 5000U;
	int centerCh = static_cast<int>((net._centerFreq1Mhz - baseMhz) / 5);

	auto build = [&](std::initializer_list<int> offsets)
	{
		std::vector<int> out;
		out.reserve(offsets.size());
		for (int o : offsets)
			out.push_back(centerCh + o);
		return out;
	};

	switch (net._widthMhz)
	{
	case 40:
		return build({-2, +2});
	case 80:
		return build({-6, -2, +2, +6});
	case 160:
		return build({-14, -10, -6, -2, +2, +6, +10, +14});
	case 320:
		return build({-30, -26, -22, -18, -14, -10, -6, -2, +2, +6, +10, +14, +18, +22, +26, +30});
	default:
		return {primary};
	}
}

int SlotOf(std::span<const int> channels, int channel)
{
	int count = static_cast<int>(channels.size());
	for (int i = 0; i < count; ++i)
		if (channels[static_cast<size_t>(i)] == channel)
			return i;
	return -1;
}

} // namespace

void SpectrumPanel::RenderBandSelector()
{
	int bandCount = static_cast<int>(kBands.size());

	// Tab / Shift+Tab cycle the band (global shortcut).
	if (ImGui::IsKeyPressed(ImGuiKey_Tab))
	{
		if (ImGui::GetIO().KeyShift)
			_activeBandIndex = (_activeBandIndex + bandCount - 1) % bandCount;
		else
			_activeBandIndex = (_activeBandIndex + 1) % bandCount;
	}

	// Segmented "pill" selector. _activeBandIndex is the single source of truth, so the
	// keyboard and the widget never fight over an internal selection.
	SegmentedControl("##bandSelector", std::span<const char* const>{kBandLabels}, _activeBandIndex);
}

void SpectrumPanel::RenderYAxis(float leftMarginW, float availH, float topMargin, float barAreaH, float scale) const
{
	ImGui::BeginChild(
		"yAxis", ImVec2(leftMarginW, availH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImVec2 lp = ImGui::GetWindowPos();
	ImDrawList* ld = ImGui::GetWindowDrawList();
	float baselineAbsY = lp.y + topMargin + barAreaH;

	ld->AddLine(ImVec2(lp.x + leftMarginW - 1.0f, lp.y), ImVec2(lp.x + leftMarginW - 1.0f, baselineAbsY),
		theme::Color(theme::UiColor::Border), 1.0f);

	for (int dbm = 0; dbm >= -100; dbm -= 20)
	{
		float fraction = (dbm + 100) / 100.0f;
		float y = baselineAbsY - fraction * barAreaH;
		std::string label = std::to_string(dbm);
		ImVec2 ts = ImGui::CalcTextSize(label.c_str());
		ld->AddText(ImVec2(lp.x + leftMarginW - ts.x - 4.0f * scale, y - ts.y * 0.5f),
			theme::Color(theme::UiColor::Muted), label.c_str());
	}

	ImGui::Dummy(ImVec2(leftMarginW, availH));
	ImGui::EndChild();
}

void SpectrumPanel::DrawGrid(ImDrawList* draw, ImVec2 origin, float plotW, float baselineAbsY, float barAreaH) const
{
	constexpr ImU32 kGridColor = IM_COL32(60, 60, 60, 200);
	for (int dbm = 0; dbm >= -100; dbm -= 20)
	{
		float fraction = (dbm + 100) / 100.0f;
		float y = baselineAbsY - fraction * barAreaH;
		draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + plotW, y), kGridColor, 1.0f);
	}
}

void SpectrumPanel::DrawChannelTicks(ImDrawList* draw, std::span<const int> channels, float slotW, ImVec2 origin,
	float baselineAbsY, const std::unordered_set<int>& primaries) const
{
	int channelCount = static_cast<int>(channels.size());
	draw->AddLine(ImVec2(origin.x, baselineAbsY), ImVec2(origin.x + slotW * channelCount, baselineAbsY),
		theme::Color(theme::UiColor::Border), 1.0f);
	for (int i = 0; i < channelCount; ++i)
	{
		int channel = channels[static_cast<size_t>(i)];
		std::string label = std::to_string(channel);
		ImVec2 ts = ImGui::CalcTextSize(label.c_str());
		float cx = origin.x + i * slotW + slotW * 0.5f;
		ImU32 c = primaries.count(channel) ? theme::Color(theme::UiColor::Accent) : theme::Color(theme::UiColor::Muted);
		draw->AddText(ImVec2(cx - ts.x * 0.5f, baselineAbsY + 2.0f), c, label.c_str());
	}
}

void SpectrumPanel::DrawNetworkBars(ImDrawList* draw, const std::vector<wifi::Network>& networks, wifi::Band band,
	std::span<const int> channels, float slotW, ImVec2 origin, float barAreaH, float baselineAbsY, float lineH) const
{
	int channelCount = static_cast<int>(channels.size());
	for (const wifi::Network& net : networks)
	{
		if (net._band != band)
			continue;

		int minSlot = channelCount;
		int maxSlot = -1;
		for (int ch : CoveredChannels(net))
		{
			int s = SlotOf(channels, ch);
			if (s < 0)
				continue;
			minSlot = std::min(minSlot, s);
			maxSlot = std::max(maxSlot, s);
		}
		if (maxSlot < 0)
			continue;

		float xLeft = origin.x + minSlot * slotW;
		float xRight = origin.x + (maxSlot + 1) * slotW;
		int clampedDbm = std::clamp((int)net._signalDbm, -100, 0);
		float barH = (clampedDbm + 100) / 100.0f * barAreaH;
		float barTop = baselineAbsY - barH;
		ImU32 color = theme::SignalColor(net._signalDbm);

		draw->AddRect(ImVec2(xLeft, barTop), ImVec2(xRight, baselineAbsY), color, 0.0f, 0, 2.0f);

		std::string ssid = net._ssid.empty() ? "???" : net._ssid;
		ImVec2 ts = ImGui::CalcTextSize(ssid.c_str());
		float labelX = (xLeft + xRight) * 0.5f - ts.x * 0.5f;
		float labelY = barTop - lineH;
		draw->AddText(ImVec2(labelX, labelY), color, ssid.c_str());
	}
}

void SpectrumPanel::Render(const std::vector<wifi::Network>& networks)
{
	RenderBandSelector();

	std::span<const int> channels = BandChannels(_activeBandIndex);
	int channelCount = static_cast<int>(channels.size());
	wifi::Band band = kBands[static_cast<size_t>(_activeBandIndex)];

	// Shared geometry — computed once so both children stay vertically aligned.
	float availH = ImGui::GetContentRegionAvail().y;
	float lineH = ImGui::GetTextLineHeight();
	float scale = ImGui::GetStyle().FontScaleDpi;
	float scrollH = ImGui::GetStyle().ScrollbarSize;
	float topMargin = lineH + 4.0f;
	float bottomAxis = lineH + 4.0f;
	// AlwaysHorizontalScrollbar always reserves scrollH at the bottom of the
	// right child; subtracting it here keeps label positions in sync with bar
	// heights.
	float barAreaH = std::max(1.0f, availH - topMargin - bottomAxis - scrollH);
	float leftMarginW = ImGui::CalcTextSize("-100").x + 8.0f * scale;

	RenderYAxis(leftMarginW, availH, topMargin, barAreaH, scale);
	ImGui::SameLine(0.0f, 0.0f);

	// --- Right child: scrollable spectrum ---
	if (!ImGui::BeginChild("spectrumPlot", ImVec2(0.0f, availH), false,
			ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar))
	{
		ImGui::EndChild();
		return;
	}

	ImVec2 avail = ImGui::GetContentRegionAvail();
	float minSlotW = kMinSlotW * scale;
	float slotW = std::max(minSlotW, avail.x / std::max(1, channelCount));
	float plotW = slotW * channelCount;

	// Left/Right arrow keys scroll the spectrum by one slot (global shortcut).
	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
		ImGui::SetScrollX(ImGui::GetScrollX() + slotW);
	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
		ImGui::SetScrollX(ImGui::GetScrollX() - slotW);

	ImVec2 origin = ImGui::GetCursorScreenPos();
	float baselineAbsY = ImGui::GetWindowPos().y + topMargin + barAreaH;
	ImDrawList* draw = ImGui::GetWindowDrawList();

	DrawGrid(draw, origin, plotW, baselineAbsY, barAreaH);

	std::unordered_set<int> primaries;
	for (const wifi::Network& net : networks)
		if (net._band == band)
			primaries.insert(net._channel);

	DrawChannelTicks(draw, channels, slotW, origin, baselineAbsY, primaries);
	DrawNetworkBars(draw, networks, band, channels, slotW, origin, barAreaH, baselineAbsY, lineH);

	// Reserve the content extent so the child's scrollbars/ranges are correct.
	// Use avail.y (inner content height) rather than availH so the dummy never
	// exceeds the child's content region and a vertical scrollbar is not shown.
	ImGui::Dummy(ImVec2(plotW, avail.y));

	ImGui::EndChild();
}

} // namespace gui
