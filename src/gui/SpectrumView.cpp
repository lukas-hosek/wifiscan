// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "SpectrumView.hpp"
#include "Fonts.hpp"
#include "ImGuiExtras.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <string>

namespace gui
{

namespace
{

using theme::UiColor;

// dBm range of the vertical axis, and the grid step
constexpr int kAxisTopDbm = -20;
constexpr int kAxisBottomDbm = -100;
constexpr int kGridStepDbm = 20;

// Layout in px at 1x scale
constexpr float kTopMargin = 10.0f;
constexpr float kTickRowHeight = 18.0f;
constexpr float kGutterLabelInset = 6.0f;
constexpr float kLabelHeight = 18.0f;
constexpr float kLabelRounding = 3.0f;
// Gap between a hump's plateau and the label resting on it
constexpr float kLabelGapAbovePeak = 4.0f;
// Labels closer than this count as colliding
constexpr float kLabelMarginX = 4.0f;
constexpr float kLabelMarginY = 2.0f;
// Gap left when a label climbs over another
constexpr float kLabelStackGap = 3.0f;
// Keeps labels off the very edges of the plot
constexpr float kLabelEdgeInset = 2.0f;
constexpr float kAnchorEdgeInset = 4.0f;
// A label pushed further than this above its peak gets a leader line
constexpr float kLeaderMinLength = 6.0f;

// Hump fill and outline. The focused AP is drawn stronger; while one is focused the others fade.
constexpr float kHumpFillAlpha = 0.10f;
constexpr float kFocusedHumpFillAlpha = 0.25f;
constexpr float kHumpStroke = 1.5f;
constexpr float kFocusedHumpStroke = 2.5f;
constexpr float kFadedAlpha = 0.2f;
constexpr float kLabelBackgroundAlpha = 0.88f;
constexpr float kLabelBorderAlpha = 0.55f;
constexpr float kLeaderAlpha = 0.6f;

float FadeFor(const BandAp* ap, const BandAp* focused)
{
	return (focused != nullptr && ap != focused) ? kFadedAlpha : 1.0f;
}

} // namespace

float SpectrumView::ChartGeometry::YOf(int signalDbm) const
{
	int clamped = std::clamp(signalDbm, kAxisBottomDbm, kAxisTopDbm);
	float fromTop = static_cast<float>(kAxisTopDbm - clamped) / static_cast<float>(kAxisTopDbm - kAxisBottomDbm);
	return chartTop + fromTop * (baseline - chartTop);
}

SpectrumView::ChartGeometry SpectrumView::Geometry(float plotHeight)
{
	float scale = DpiScale();
	float chartTop = kTopMargin * scale;
	float baseline = std::max(chartTop + 1.0f, plotHeight - kTickRowHeight * scale);
	return ChartGeometry{chartTop, baseline};
}

void SpectrumView::RenderGutter(float plotHeight)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	float width = ImGui::GetContentRegionAvail().x;
	float scale = DpiScale();
	ChartGeometry geometry = Geometry(plotHeight);

	float edgeX = pos.x + width - 1.0f;
	draw->AddLine(ImVec2(edgeX, pos.y), ImVec2(edgeX, pos.y + geometry.baseline), theme::Color(UiColor::Border), 1.0f);

	fonts::ScopedFont font(fonts::FontStyle::Regular, fonts::kSmallSize);
	for (int dbm = kAxisTopDbm; dbm >= kAxisBottomDbm; dbm -= kGridStepDbm)
	{
		std::string label = std::to_string(dbm);
		ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
		float y = pos.y + geometry.YOf(dbm);
		draw->AddText(ImVec2(pos.x + width - kGutterLabelInset * scale - textSize.x, y - textSize.y * 0.5f),
			theme::Color(UiColor::Muted), label.c_str());
	}
}

void SpectrumView::RenderPlot(const ChannelAxis& axis, std::span<const BandAp> aps, const BandAp* focused,
	float plotHeight, bool revealFocused, std::string& selectedBssid)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 origin = ImGui::GetCursorScreenPos();
	float scale = DpiScale();
	ChartGeometry geometry = Geometry(plotHeight);

	DrawGrid(draw, origin, axis.plotWidth, geometry);

	// Widest first, then weakest first, so narrow strong APs stay on top. The focused AP goes last of all.
	std::vector<const BandAp*> humpOrder;
	humpOrder.reserve(aps.size());
	for (const BandAp& ap : aps)
		humpOrder.push_back(&ap);
	std::stable_sort(humpOrder.begin(), humpOrder.end(),
		[focused](const BandAp* a, const BandAp* b)
		{
			if ((a == focused) != (b == focused))
				return b == focused;
			float widthA = a->span.hi - a->span.lo;
			float widthB = b->span.hi - b->span.lo;
			if (widthA != widthB)
				return widthA > widthB;
			return a->network->_signalDbm < b->network->_signalDbm;
		});
	for (const BandAp* ap : humpOrder)
		DrawHump(draw, origin, axis, *ap, geometry, ap == focused, FadeFor(ap, focused));

	// Labels draw in placement order with the focused one last. Two passes: all leader lines first, then all
	// boxes, so a leader running down past other labels stays visible through their translucent boxes instead of
	// being drawn over them.
	std::vector<PlacedLabel> labels = PlaceLabels(axis, aps, geometry);
	std::vector<const PlacedLabel*> labelOrder;
	labelOrder.reserve(labels.size());
	for (const PlacedLabel& label : labels)
		labelOrder.push_back(&label);
	std::stable_partition(labelOrder.begin(), labelOrder.end(),
		[focused](const PlacedLabel* label)
		{
			return label->ap != focused;
		});
	for (const PlacedLabel* label : labelOrder)
		DrawLeader(draw, origin, *label, FadeFor(label->ap, focused));
	for (const PlacedLabel* label : labelOrder)
		DrawLabelBox(draw, origin, *label, FadeFor(label->ap, focused));

	axis.DrawTicks(draw, origin, origin.y + geometry.baseline, kTickRowHeight * scale);

	// Clicking a label toggles focus on its AP
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		ImVec2 mouse(ImGui::GetIO().MousePos.x - origin.x, ImGui::GetIO().MousePos.y - origin.y);
		// Last drawn is on top, so it gets the click where labels overlap (only possible when pinned at the top).
		for (auto labelIt = labels.rbegin(); labelIt != labels.rend(); ++labelIt)
		{
			const PlacedLabel& label = *labelIt;
			if (mouse.x < label.min.x || mouse.x > label.max.x || mouse.y < label.min.y || mouse.y > label.max.y)
				continue;
			const std::string& bssid = label.ap->network->_bssid;
			selectedBssid = (selectedBssid == bssid) ? std::string() : bssid;
			break;
		}
	}

	if (revealFocused && focused != nullptr)
		axis.ScrollIntoView(focused->span);

	// Reserve the content extent so the horizontal scroll range is right. Never taller than the visible area, or a
	// vertical scrollbar would appear.
	ImGui::Dummy(ImVec2(axis.plotWidth, std::min(plotHeight, ImGui::GetContentRegionAvail().y)));
}

std::vector<SpectrumView::PlacedLabel> SpectrumView::PlaceLabels(
	const ChannelAxis& axis, std::span<const BandAp> aps, const ChartGeometry& geometry)
{
	float scale = DpiScale();
	float height = kLabelHeight * scale;
	float marginX = kLabelMarginX * scale;
	float marginY = kLabelMarginY * scale;

	std::vector<PlacedLabel> placed;
	placed.reserve(aps.size());
	// aps is strongest first; walk it backwards so weaker labels settle first and stronger ones climb over them.
	for (auto apIt = aps.rbegin(); apIt != aps.rend(); ++apIt)
	{
		const BandAp& ap = *apIt;
		float left = axis.ClippedLeft(ap.span);
		float right = axis.ClippedRight(ap.span);
		if (right <= left)
			continue;

		ApLabel text(*ap.network);
		float width = text.BoxWidth();
		float anchorX =
			std::clamp((left + right) * 0.5f, kAnchorEdgeInset * scale, axis.plotWidth - kAnchorEdgeInset * scale);
		float maxLeft = std::max(kLabelEdgeInset * scale, axis.plotWidth - width - kLabelEdgeInset * scale);
		float boxLeft = std::clamp(anchorX - width * 0.5f, kLabelEdgeInset * scale, maxLeft);
		float peakY = geometry.YOf(ap.network->_signalDbm);
		float bottom = peakY - kLabelGapAbovePeak * scale;

		// Each step climbs strictly above one placed label, so this ends within placed.size() steps.
		for (size_t step = 0; step <= placed.size(); ++step)
		{
			auto hit = std::find_if(placed.begin(), placed.end(),
				[&](const PlacedLabel& other)
				{
					return boxLeft < other.max.x + marginX && boxLeft + width > other.min.x - marginX &&
						bottom - height < other.max.y + marginY && bottom > other.min.y - marginY;
				});
			if (hit == placed.end())
				break;
			bottom = hit->min.y - kLabelStackGap * scale;
			// Out of room at the top: stay pinned there, even if that overlaps.
			if (bottom - height < 0.0f)
				break;
		}
		bottom = std::max(bottom, height);

		placed.push_back(
			PlacedLabel{&ap, text, ImVec2(boxLeft, bottom - height), ImVec2(boxLeft + width, bottom), anchorX, peakY});
	}
	return placed;
}

void SpectrumView::DrawGrid(ImDrawList* draw, ImVec2 origin, float plotWidth, const ChartGeometry& geometry)
{
	for (int dbm = kAxisTopDbm; dbm >= kAxisBottomDbm; dbm -= kGridStepDbm)
	{
		float y = origin.y + geometry.YOf(dbm);
		UiColor color = dbm == kAxisBottomDbm ? UiColor::Border : UiColor::GridLine;
		draw->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + plotWidth, y), theme::Color(color), 1.0f);
	}
}

void SpectrumView::DrawHump(ImDrawList* draw, ImVec2 origin, const ChannelAxis& axis, const BandAp& ap,
	const ChartGeometry& geometry, bool isFocused, float alpha)
{
	float scale = DpiScale();
	float x0 = origin.x + axis.SlotX(ap.span.lo);
	float x1 = origin.x + axis.SlotX(ap.span.hi);
	float top = origin.y + geometry.YOf(ap.network->_signalDbm);
	float base = origin.y + geometry.baseline;
	// Slope width: at most 90% of a slot, and never more than a quarter of the hump so the plateau survives
	float slope = std::min(0.9f * axis.slotWidth, 0.25f * (x1 - x0));
	ImU32 color = theme::SignalColor(ap.network->_signalDbm);

	// Baseline up an S-curve, across the plateau, down an S-curve. The fill closes along the baseline.
	auto buildOutline = [&]
	{
		draw->PathLineTo(ImVec2(x0, base));
		draw->PathBezierCubicCurveTo(
			ImVec2(x0 + slope * 0.5f, base), ImVec2(x0 + slope * 0.5f, top), ImVec2(x0 + slope, top));
		draw->PathLineToMergeDuplicate(ImVec2(x1 - slope, top));
		draw->PathBezierCubicCurveTo(ImVec2(x1 - slope * 0.5f, top), ImVec2(x1 - slope * 0.5f, base), ImVec2(x1, base));
	};

	// The S-curves make the shape non-convex, so it needs the concave fill. A flat hump (at -100 dBm) has no area.
	// Anti-aliasing is off for the fill: ImGui's AA concave fill insets the vertices of its thin triangles, they
	// overlap, and the translucent fill shows dotted seams. The anti-aliased stroke covers the edge anyway.
	if (base - top >= 1.0f)
	{
		ImDrawListFlags savedFlags = draw->Flags;
		draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
		buildOutline();
		draw->PathFillConcave(theme::WithAlpha(color, (isFocused ? kFocusedHumpFillAlpha : kHumpFillAlpha) * alpha));
		draw->Flags = savedFlags;
	}
	buildOutline();
	draw->PathStroke(
		theme::WithAlpha(color, alpha), ImDrawFlags_None, (isFocused ? kFocusedHumpStroke : kHumpStroke) * scale);
}

void SpectrumView::DrawLeader(ImDrawList* draw, ImVec2 origin, const PlacedLabel& label, float alpha)
{
	float scale = DpiScale();
	if (label.peakY - label.max.y <= kLeaderMinLength * scale)
		return;

	ImU32 color = theme::SignalColor(label.ap->network->_signalDbm);
	float x = origin.x + label.anchorX;
	draw->AddLine(ImVec2(x, origin.y + label.max.y), ImVec2(x, origin.y + label.peakY),
		theme::WithAlpha(color, kLeaderAlpha * alpha), scale);
}

void SpectrumView::DrawLabelBox(ImDrawList* draw, ImVec2 origin, const PlacedLabel& label, float alpha)
{
	float scale = DpiScale();
	ImU32 color = theme::SignalColor(label.ap->network->_signalDbm);
	ImVec2 boxMin(origin.x + label.min.x, origin.y + label.min.y);
	ImVec2 boxMax(origin.x + label.max.x, origin.y + label.max.y);

	float rounding = kLabelRounding * scale;
	draw->AddRectFilled(boxMin, boxMax,
		theme::WithAlpha(theme::Color(UiColor::AppBackground), kLabelBackgroundAlpha * alpha), rounding);
	draw->AddRect(boxMin, boxMax, theme::WithAlpha(color, kLabelBorderAlpha * alpha), rounding, 0, scale);

	float textY = boxMin.y + (boxMax.y - boxMin.y - fonts::kSmallSize * scale) * 0.5f;
	label.text.Draw(draw, ImVec2(boxMin.x + (ApLabel::kPadding + 1.0f) * scale, textY), alpha);
}

} // namespace gui
