// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "Theme.hpp"

namespace gui::theme
{

ImU32 Color(UiColor element)
{
	switch (element)
	{
	case UiColor::AppBackground:
		return IM_COL32(12, 12, 12, 255);
	case UiColor::Border:
		return IM_COL32(0, 180, 0, 255);
	case UiColor::ColumnHeader:
		return IM_COL32(128, 255, 255, 255);
	case UiColor::DataValue:
		return IM_COL32(0, 220, 220, 255);
	case UiColor::StatusText:
		return IM_COL32(0, 200, 0, 255);
	case UiColor::NetworkRow:
		return IM_COL32(140, 255, 140, 255);
	case UiColor::ConnectedNetwork:
		return IM_COL32(0, 255, 128, 255);
	case UiColor::SignalStrong:
		return IM_COL32(140, 255, 140, 255);
	case UiColor::SignalGood:
		return IM_COL32(0, 200, 0, 255);
	case UiColor::SignalMedium:
		return IM_COL32(235, 235, 0, 255);
	case UiColor::SignalWeak:
		return IM_COL32(235, 70, 70, 255);
	case UiColor::Muted:
		return IM_COL32(120, 120, 120, 255);
	case UiColor::ShortcutHint:
		return IM_COL32(180, 160, 90, 255);
	}
	return IM_COL32_WHITE;
}

ImU32 SignalColor(int signalDbm)
{
	if (signalDbm >= -60)
		return Color(UiColor::SignalStrong);
	if (signalDbm >= -70)
		return Color(UiColor::SignalGood);
	if (signalDbm >= -80)
		return Color(UiColor::SignalMedium);
	return Color(UiColor::SignalWeak);
}

} // namespace gui::theme
