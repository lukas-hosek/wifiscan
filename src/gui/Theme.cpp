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
		return IM_COL32(11, 13, 18, 255);
	case UiColor::Border:
		return IM_COL32(38, 44, 58, 255);
	case UiColor::Accent:
		return IM_COL32(59, 130, 246, 255);
	case UiColor::ColumnHeader:
		return IM_COL32(110, 122, 140, 255);
	case UiColor::DataValue:
		return IM_COL32(200, 208, 222, 255);
	case UiColor::StatusText:
		return IM_COL32(155, 168, 185, 255);
	case UiColor::NetworkRow:
		return IM_COL32(210, 216, 228, 255);
	case UiColor::ConnectedNetwork:
		return IM_COL32(74, 222, 128, 255);
	case UiColor::SignalStrong:
		return IM_COL32(52, 211, 96, 255);
	case UiColor::SignalGood:
		return IM_COL32(163, 230, 53, 255);
	case UiColor::SignalMedium:
		return IM_COL32(245, 158, 11, 255);
	case UiColor::SignalWeak:
		return IM_COL32(220, 68, 50, 255);
	case UiColor::Muted:
		return IM_COL32(75, 85, 102, 255);
	case UiColor::ShortcutHint:
		return IM_COL32(100, 88, 60, 255);
	case UiColor::Surface:
		return IM_COL32(36, 41, 56, 255);
	case UiColor::SurfaceHover:
		return IM_COL32(51, 59, 82, 255);
	case UiColor::SurfaceActive:
		return IM_COL32(64, 71, 102, 255);
	case UiColor::RowBg:
		return IM_COL32(0, 0, 0, 0);
	case UiColor::SurfaceAlt:
		return IM_COL32(255, 255, 255, 5);
	case UiColor::BorderStrong:
		return IM_COL32(51, 56, 71, 255);
	case UiColor::SelectionBg:
		return IM_COL32(46, 56, 87, 255);
	case UiColor::SelectionBgHover:
		return IM_COL32(56, 69, 102, 255);
	case UiColor::SelectionBgActive:
		return IM_COL32(66, 82, 122, 255);
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
