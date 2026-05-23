// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <ftxui/screen/color.hpp>

namespace ui::theme
{

enum class UiColor
{
	AppBackground,
	Border,
	BannerText,
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

ftxui::Color Color(UiColor element);

// Returns a color scaled by signal strength:
// >= -60 dBm → SignalStrong, >= -70 → SignalGood, >= -80 → SignalMedium, else
// SignalWeak
ftxui::Color SignalColor(int signalDbm);

} // namespace ui::theme
