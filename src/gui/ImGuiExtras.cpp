// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ImGuiExtras.hpp"
#include "Theme.hpp"
#include <imgui.h>
#include <vector>

namespace gui
{

bool SegmentedControl(const char* id, std::span<const char* const> labels, int& selectedIndex)
{
	int count = static_cast<int>(labels.size());
	if (count == 0)
		return false;

	float scale = ImGui::GetStyle().FontScaleDpi;
	if (scale <= 0.0f)
		scale = 1.0f;

	// Geometry: text width plus horizontal padding per segment, vertical padding around
	// the font height. The pill rounding is half the height (a full capsule); the inset
	// segment fill rounds a touch tighter so it nests inside the border.
	const float padX = 14.0f * scale;
	const float padY = 6.0f * scale;
	const float inset = 2.0f * scale;
	const float height = ImGui::GetFontSize() + 2.0f * padY;
	const float rounding = height * 0.5f;
	const float innerRounding = rounding - inset;

	// Per-segment widths, accumulated into the total control width.
	std::vector<float> segWidths(static_cast<size_t>(count));
	float totalW = 0.0f;
	for (int i = 0; i < count; ++i)
	{
		float w = ImGui::CalcTextSize(labels[static_cast<size_t>(i)]).x + 2.0f * padX;
		segWidths[static_cast<size_t>(i)] = w;
		totalW += w;
	}

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

} // namespace gui
