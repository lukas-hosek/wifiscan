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
};

ImU32 Color(UiColor element);

// Returns a color scaled by signal strength, using the same thresholds as
// ui::theme::SignalColor: >= -60 dBm strong, >= -70 good, >= -80 medium, else weak.
ImU32 SignalColor(int signalDbm);

} // namespace gui::theme
