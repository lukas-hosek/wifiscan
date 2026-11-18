// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <imgui.h>
#include <span>

namespace gui
{

// Custom ImGui widgets shared across the GUI panels.

// The desktop content scale (style.FontScaleDpi, never 0). Multiply the design's 1x px constants by it.
float DpiScale();

// A horizontal segmented "pill" toggle: a single rounded-bordered container split into
// N segments. The selected segment is filled with the Accent color; the rest render as
// muted text. The caller owns the selection state (single source of truth); selectedIndex
// is read and updated in place. Returns true if the user changed the selection this frame.
// id must be unique within the window.
bool SegmentedControl(const char* id, std::span<const char* const> labels, int& selectedIndex);

// Width SegmentedControl will take for these labels, so a caller can right-align it.
float SegmentedControlWidth(std::span<const char* const> labels);

// Draws a small rounded label with its top-left corner at topLeft, its text at fontSize (unscaled px). Draw-list
// only: it is not an ImGui item and doesn't move the cursor. Pass a transparent fill or border to leave that part out.
void DrawPill(
	ImDrawList* draw, ImVec2 topLeft, const char* text, float fontSize, ImU32 fill, ImU32 border, ImU32 textColor);

// Size DrawPill takes for this text at fontSize
ImVec2 PillSize(const char* text, float fontSize);

} // namespace gui
