// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <imgui.h>

namespace gui::theme
{

// The GUI's dark-navy palette. It started as RGB equivalents of tui::theme::UiColor (src/tui/Theme.hpp) and has
// since been tuned for the GUI, so the two are no longer kept identical.
enum class UiColor
{
	AppBackground,
	// Interactive highlights: active band and mode, primary channel labels, selection
	Accent,
	// Text, strongest to weakest
	PrimaryText,
	DataValue,
	StatusText,
	Muted,
	ShortcutHint,
	ConnectedNetwork,
	// Warning text, e.g. an Open network
	Warning,
	// Signal strokes and fills
	SignalStrong,
	SignalGood,
	SignalMedium,
	SignalWeak,
	// SignalWeak lightened so small text stays readable on the dark background
	SignalWeakText,
	// ImGui chrome: surfaces, borders, and selection states
	Surface,
	SurfaceHover,
	SurfaceActive,
	// Transparent base table row (inherits window bg) and 2% white alt stripe
	RowBg,
	SurfaceAlt,
	Border,
	BorderStrong,
	SelectionBg,
	SelectionBgHover,
	SelectionBgActive,
	// Charts and meters
	MeterTrack,
	GridLine,
	SlotDivider,
	EmptySlot,
};

ImU32 Color(UiColor element);

// Returns a color scaled by signal strength, using the same thresholds as tui::theme::SignalColor: >= -60 dBm
// strong, >= -70 good, >= -80 medium, else weak.
ImU32 SignalColor(int signalDbm);

// SignalColor for text: identical except that weak uses the lighter SignalWeakText.
ImU32 SignalTextColor(int signalDbm);

// The color with its alpha multiplied by alphaScale (0..1)
ImU32 WithAlpha(ImU32 color, float alphaScale);

} // namespace gui::theme
