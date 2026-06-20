// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <span>

namespace gui
{

// Custom ImGui widgets shared across the GUI panels.

// A horizontal segmented "pill" toggle: a single rounded-bordered container split into
// N segments. The selected segment is filled with the Accent color; the rest render as
// muted text. The caller owns the selection state (single source of truth); selectedIndex
// is read and updated in place. Returns true if the user changed the selection this frame.
// id must be unique within the window.
bool SegmentedControl(const char* id, std::span<const char* const> labels, int& selectedIndex);

} // namespace gui
