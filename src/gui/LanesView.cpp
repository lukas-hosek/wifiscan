// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "LanesView.hpp"
#include "Fonts.hpp"
#include "ImGuiExtras.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <string>
#include <utility>

namespace gui
{

namespace
{

using theme::UiColor;

// Layout in px at 1x scale, from the top of the plot
constexpr float kTickRowHeight = 18.0f;
constexpr float kCongestionTop = 22.0f;
constexpr float kCongestionHeight = 22.0f;
// Everything above this stays pinned while the lanes scroll under it
constexpr float kPinnedHeight = 50.0f;
constexpr float kFirstLaneTop = 54.0f;
constexpr float kLanePitch = 27.0f;
constexpr float kBarHeight = 22.0f;
constexpr float kBarRounding = 3.0f;
constexpr float kPrimaryStripHeight = 4.0f;
// Minimum horizontal gap between two bars (with their labels) in one lane
constexpr float kLaneGap = 8.0f;
constexpr float kGutterLabelInset = 6.0f;

// A slot counts an AP when their overlap is more than this fraction of the slot, so a 2.4 GHz span that only
// grazes a slot edge doesn't count.
constexpr float kMinSlotOverlap = 0.05f;

constexpr float kBarFillAlpha = 0.19f;
constexpr float kFocusedBarFillAlpha = 0.30f;
constexpr float kBarBorderAlpha = 0.70f;
constexpr float kFadedAlpha = 0.2f;
// Congestion cell opacity: starts at kCongestionBaseAlpha for one AP, adds kCongestionStepAlpha per extra AP
constexpr float kCongestionBaseAlpha = 0.16f;
constexpr float kCongestionStepAlpha = 0.16f;
constexpr float kCongestionMaxAlpha = 0.8f;

void DrawRowCaption(ImDrawList* draw, ImVec2 pos, float width, float top, float rowHeight, const char* caption)
{
	ImVec2 textSize = ImGui::CalcTextSize(caption);
	draw->AddText(ImVec2(pos.x + width - kGutterLabelInset * DpiScale() - textSize.x,
					  pos.y + top + (rowHeight - textSize.y) * 0.5f),
		theme::Color(UiColor::Muted), caption);
}

} // namespace

float LanesView::LaneTop(int lane) { return (kFirstLaneTop + static_cast<float>(lane) * kLanePitch) * DpiScale(); }

void LanesView::RenderGutter(float /*plotHeight*/)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	float width = ImGui::GetContentRegionAvail().x;
	float scale = DpiScale();

	fonts::ScopedFont caption(fonts::FontStyle::Regular, fonts::kCaptionSize);
	DrawRowCaption(draw, pos, width, 0.0f, kTickRowHeight * scale, "CH");
	DrawRowCaption(draw, pos, width, kCongestionTop * scale, kCongestionHeight * scale, "APs");
}

void LanesView::RenderPlot(const ChannelAxis& axis, std::span<const BandAp> aps, const BandAp* focused,
	float plotHeight, bool revealFocused, std::string& selectedBssid)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	// origin scrolls with the content; windowPos stays put and anchors the pinned rows.
	ImVec2 origin = ImGui::GetCursorScreenPos();
	ImVec2 windowPos = ImGui::GetWindowPos();
	float scale = DpiScale();
	float pinnedHeight = kPinnedHeight * scale;
	float barHeight = kBarHeight * scale;

	int laneCount = 0;
	std::vector<PlacedBar> bars = PackLanes(axis, aps, laneCount);

	// Slot dividers behind the bars, over the visible lane area
	for (size_t slot = 1; slot < axis.channels.size(); ++slot)
	{
		float x = origin.x + axis.SlotX(static_cast<float>(slot));
		draw->AddLine(ImVec2(x, windowPos.y + pinnedHeight), ImVec2(x, windowPos.y + plotHeight),
			theme::Color(UiColor::SlotDivider), 1.0f);
	}

	const PlacedBar* focusedBar = nullptr;
	for (const PlacedBar& bar : bars)
	{
		if (bar.ap == focused)
			focusedBar = &bar;
		else
			DrawBar(draw, origin, bar, false, focused != nullptr ? kFadedAlpha : 1.0f);
	}
	if (focusedBar != nullptr)
		DrawBar(draw, origin, *focusedBar, true, 1.0f);

	// Pinned rows on an opaque band so the lanes scroll under them. The window clip rect keeps the band off the
	// scrollbars.
	draw->AddRectFilled(windowPos, ImVec2(windowPos.x + ImGui::GetWindowWidth(), windowPos.y + pinnedHeight),
		theme::Color(UiColor::AppBackground));
	axis.DrawTicks(draw, origin, windowPos.y, kTickRowHeight * scale);
	DrawCongestionRow(draw, origin, windowPos.y + kCongestionTop * scale, axis, aps);

	// Clicking a bar (or its label) toggles focus on its AP. Clicks on the pinned rows don't reach the lanes.
	ImVec2 mouse = ImGui::GetIO().MousePos;
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		mouse.y > windowPos.y + pinnedHeight)
	{
		for (const PlacedBar& bar : bars)
		{
			float top = origin.y + LaneTop(bar.lane);
			if (mouse.x < origin.x + bar.reservedLeft || mouse.x > origin.x + bar.reservedRight || mouse.y < top ||
				mouse.y > top + barHeight)
				continue;
			const std::string& bssid = bar.ap->network->_bssid;
			selectedBssid = (selectedBssid == bssid) ? std::string() : bssid;
			break;
		}
	}

	if (revealFocused && focusedBar != nullptr)
	{
		axis.ScrollIntoView(focused->span);

		// Vertically, the lanes are visible between the pinned rows and the bottom of the plot.
		float laneTop = LaneTop(focusedBar->lane);
		float scrollY = ImGui::GetScrollY();
		if (laneTop < scrollY + pinnedHeight || laneTop + barHeight > scrollY + plotHeight)
			ImGui::SetScrollY(laneTop - pinnedHeight - (plotHeight - pinnedHeight - barHeight) * 0.5f);
	}

	// Reserve the content extent: the plot width, and all lanes so a crowded band scrolls vertically.
	ImGui::Dummy(ImVec2(axis.plotWidth, LaneTop(laneCount)));
}

std::vector<LanesView::PlacedBar> LanesView::PackLanes(
	const ChannelAxis& axis, std::span<const BandAp> aps, int& laneCount)
{
	float gap = kLaneGap * DpiScale();
	// Occupied [left, right] intervals per lane
	std::vector<std::vector<std::pair<float, float>>> lanes;
	std::vector<PlacedBar> bars;
	bars.reserve(aps.size());

	for (const BandAp& ap : aps)
	{
		float barLeft = axis.ClippedLeft(ap.span);
		float barRight = axis.ClippedRight(ap.span);
		if (barRight <= barLeft)
			continue;

		// A narrow 20 MHz bar still reserves room for its name; near the right edge the label shifts left to stay
		// inside the plot.
		ApLabel label(*ap.network);
		float labelWidth = label.BoxWidth();
		float reservedLeft = std::max(0.0f, std::min(barLeft, axis.plotWidth - labelWidth));
		float reservedRight = std::max(barRight, reservedLeft + labelWidth);

		auto fits = [&](const std::vector<std::pair<float, float>>& lane)
		{
			return std::none_of(lane.begin(), lane.end(),
				[&](const std::pair<float, float>& taken)
				{
					return reservedLeft < taken.second + gap && reservedRight > taken.first - gap;
				});
		};
		size_t lane = 0;
		while (lane < lanes.size() && !fits(lanes[lane]))
			++lane;
		if (lane == lanes.size())
			lanes.emplace_back();
		lanes[lane].emplace_back(reservedLeft, reservedRight);

		float primaryLeft = std::max(0.0f, axis.SlotX(ap.span.primaryLo));
		float primaryRight = std::min(axis.plotWidth, axis.SlotX(ap.span.primaryHi));
		bars.push_back(PlacedBar{&ap, label, static_cast<int>(lane), reservedLeft, reservedRight, barLeft, barRight,
			primaryLeft, primaryRight});
	}

	laneCount = static_cast<int>(lanes.size());
	return bars;
}

void LanesView::DrawBar(ImDrawList* draw, ImVec2 origin, const PlacedBar& bar, bool isFocused, float alpha)
{
	float scale = DpiScale();
	float top = origin.y + LaneTop(bar.lane);
	float bottom = top + kBarHeight * scale;
	float rounding = kBarRounding * scale;
	ImU32 color = theme::SignalColor(bar.ap->network->_signalDbm);

	ImVec2 barMin(origin.x + bar.barLeft, top);
	ImVec2 barMax(origin.x + bar.barRight, bottom);
	draw->AddRectFilled(
		barMin, barMax, theme::WithAlpha(color, (isFocused ? kFocusedBarFillAlpha : kBarFillAlpha) * alpha), rounding);
	draw->AddRect(barMin, barMax, theme::WithAlpha(color, (isFocused ? 1.0f : kBarBorderAlpha) * alpha), rounding, 0,
		(isFocused ? 2.0f : 1.0f) * scale);

	// The primary 20 MHz inside a wider block
	if (bar.primaryRight > bar.primaryLeft)
		draw->AddRectFilled(ImVec2(origin.x + bar.primaryLeft, bottom - kPrimaryStripHeight * scale),
			ImVec2(origin.x + bar.primaryRight, bottom), theme::WithAlpha(color, alpha), rounding,
			ImDrawFlags_RoundCornersBottom);

	// Text is centred in the part of the bar above the primary strip
	float textY = top + (kBarHeight * scale - kPrimaryStripHeight * scale - fonts::kSmallSize * scale) * 0.5f;
	bar.label.Draw(draw, ImVec2(origin.x + bar.reservedLeft + ApLabel::kPadding * scale, textY), alpha);
}

void LanesView::DrawCongestionRow(
	ImDrawList* draw, ImVec2 origin, float top, const ChannelAxis& axis, std::span<const BandAp> aps)
{
	float scale = DpiScale();
	float height = kCongestionHeight * scale;
	fonts::ScopedFont font(fonts::FontStyle::Regular, fonts::kSmallSize);

	for (size_t slot = 0; slot < axis.channels.size(); ++slot)
	{
		float slotLo = static_cast<float>(slot);
		auto count = std::count_if(aps.begin(), aps.end(),
			[slotLo](const BandAp& ap)
			{
				return std::min(ap.span.hi, slotLo + 1.0f) - std::max(ap.span.lo, slotLo) > kMinSlotOverlap;
			});

		// Cells sit 1 px apart so neighbouring counts stay distinct
		ImVec2 cellMin(origin.x + axis.SlotX(slotLo), top);
		ImVec2 cellMax(cellMin.x + axis.slotWidth - scale, top + height);
		ImU32 background = theme::Color(UiColor::EmptySlot);
		if (count > 0)
		{
			float opacity = std::min(
				kCongestionMaxAlpha, kCongestionBaseAlpha + kCongestionStepAlpha * static_cast<float>(count - 1));
			background = theme::WithAlpha(theme::Color(UiColor::Accent), opacity);
		}
		draw->AddRectFilled(cellMin, cellMax, background);

		std::string text = count > 0 ? std::to_string(count) : "\xC2\xB7"; // U+00B7 middle dot
		ImU32 textColor = count > 0 ? IM_COL32_WHITE : theme::Color(UiColor::Muted);
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
		draw->AddText(ImVec2(cellMin.x + (axis.slotWidth - textSize.x) * 0.5f, top + (height - textSize.y) * 0.5f),
			textColor, text.c_str());
	}
}

} // namespace gui
