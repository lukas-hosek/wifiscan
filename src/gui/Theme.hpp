// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <imgui.h>

namespace gui::theme
{

// Mirrors ui::theme::UiColor (src/ui/Theme.hpp) as RGB equivalents of the named
// FTXUI colors used by the terminal UI.
enum class UiColor
{
	AppBackground,
	Border,
	// interactive highlights: active band button, primary channel labels
	Accent,
	ColumnHeader,
	DataValue,
	StatusText,
	NetworkRow,
	ConnectedNetwork,
	SignalStrong,
	SignalGood,
	SignalMedium,
	SignalWeak,
	Muted,
	ShortcutHint,
	// ImGui chrome: surfaces, borders, and selection states
	Surface,
	SurfaceHover,
	SurfaceActive,
	// transparent base table row (inherits window bg) and 2% white alt stripe
	RowBg,
	SurfaceAlt,
	BorderStrong,
	SelectionBg,
	SelectionBgHover,
	SelectionBgActive,
};

ImU32 Color(UiColor element);

// Returns a color scaled by signal strength, using the same thresholds as
// ui::theme::SignalColor: >= -60 dBm strong, >= -70 good, >= -80 medium, else weak.
ImU32 SignalColor(int signalDbm);

} // namespace gui::theme
