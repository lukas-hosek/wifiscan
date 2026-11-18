// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ImGuiExtras.hpp"
#include "Fonts.hpp"
#include "Theme.hpp"
#include <vector>

namespace gui
{

float DpiScale()
{
	float scale = ImGui::GetStyle().FontScaleDpi;
	return scale > 0.0f ? scale : 1.0f;
}

namespace
{

// Per-segment widths: label text plus horizontal padding on both sides.
std::vector<float> SegmentWidths(std::span<const char* const> labels)
{
	const float padX = 12.0f * DpiScale();
	std::vector<float> widths;
	widths.reserve(labels.size());
	for (const char* label : labels)
		widths.push_back(ImGui::CalcTextSize(label).x + 2.0f * padX);
	return widths;
}

} // namespace

float SegmentedControlWidth(std::span<const char* const> labels)
{
	float total = 0.0f;
	for (float w : SegmentWidths(labels))
		total += w;
	return total;
}

bool SegmentedControl(const char* id, std::span<const char* const> labels, int& selectedIndex)
{
	int count = static_cast<int>(labels.size());
	if (count == 0)
		return false;

	float scale = DpiScale();

	// Geometry: vertical padding around the font height. The pill rounding is half the height (a full capsule);
	// the inset segment fill rounds a touch tighter so it nests inside the border.
	const float padY = 6.0f * scale;
	const float inset = 2.0f * scale;
	const float height = ImGui::GetFontSize() + 2.0f * padY;
	const float rounding = height * 0.5f;
	const float innerRounding = rounding - inset;

	std::vector<float> segWidths = SegmentWidths(labels);
	float totalW = SegmentedControlWidth(labels);

	// Reserve layout space and capture interaction with a single invisible button spanning
	// the whole control; the cursor then advances exactly like a normal widget.
	ImVec2 p0 = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(id, ImVec2(totalW, height));
	bool hovered = ImGui::IsItemHovered();
	ImVec2 p1 = ImVec2(p0.x + totalW, p0.y + height);

	// Mouse-x to segment index, used for both the hover highlight and click selection.
	int hotSegment = -1;
	if (hovered)
	{
		float mouseX = ImGui::GetIO().MousePos.x;
		float x = p0.x;
		for (int i = 0; i < count; ++i)
		{
			float next = x + segWidths[static_cast<size_t>(i)];
			if (mouseX >= x && mouseX < next)
			{
				hotSegment = i;
				break;
			}
			x = next;
		}
	}

	bool changed = false;
	if (clicked && hotSegment >= 0 && hotSegment != selectedIndex)
	{
		selectedIndex = hotSegment;
		changed = true;
	}

	ImDrawList* draw = ImGui::GetWindowDrawList();

	// Outer pill: filled surface plus border outline.
	draw->AddRectFilled(p0, p1, theme::Color(theme::UiColor::Surface), rounding);
	draw->AddRect(p0, p1, theme::Color(theme::UiColor::Border), rounding, 0, 1.0f * scale);

	// Per-segment fills and centered labels.
	float x = p0.x;
	for (int i = 0; i < count; ++i)
	{
		float w = segWidths[static_cast<size_t>(i)];
		bool isSelected = i == selectedIndex;

		if (isSelected || i == hotSegment)
		{
			// Inset rounded fill: accent for the selection, a subtle surface for hover.
			ImVec2 fillMin(x + inset, p0.y + inset);
			ImVec2 fillMax(x + w - inset, p1.y - inset);
			ImU32 fill = isSelected ? theme::Color(theme::UiColor::Accent) : theme::Color(theme::UiColor::SurfaceHover);
			draw->AddRectFilled(fillMin, fillMax, fill, innerRounding);
		}

		const char* label = labels[static_cast<size_t>(i)];
		ImVec2 ts = ImGui::CalcTextSize(label);
		ImVec2 textPos(x + (w - ts.x) * 0.5f, p0.y + (height - ts.y) * 0.5f);
		ImU32 textColor;
		if (isSelected)
			textColor = IM_COL32_BLACK;
		else if (i == hotSegment)
			textColor = theme::Color(theme::UiColor::DataValue);
		else
			textColor = theme::Color(theme::UiColor::Muted);
		draw->AddText(textPos, textColor, label);

		x += w;
	}

	return changed;
}

ImVec2 PillSize(const char* text, float fontSize)
{
	fonts::ScopedFont font(fonts::FontStyle::Regular, fontSize);
	float scale = DpiScale();
	ImVec2 textSize = ImGui::CalcTextSize(text);
	return ImVec2(textSize.x + 2.0f * 7.0f * scale, textSize.y + 2.0f * 3.0f * scale);
}

void DrawPill(
	ImDrawList* draw, ImVec2 topLeft, const char* text, float fontSize, ImU32 fill, ImU32 border, ImU32 textColor)
{
	ImVec2 size = PillSize(text, fontSize);
	ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);
	float rounding = size.y * 0.5f;

	draw->AddRectFilled(topLeft, bottomRight, fill, rounding);
	draw->AddRect(topLeft, bottomRight, border, rounding, 0, DpiScale());

	fonts::ScopedFont font(fonts::FontStyle::Regular, fontSize);
	ImVec2 textSize = ImGui::CalcTextSize(text);
	draw->AddText(
		ImVec2(topLeft.x + (size.x - textSize.x) * 0.5f, topLeft.y + (size.y - textSize.y) * 0.5f), textColor, text);
}

} // namespace gui
