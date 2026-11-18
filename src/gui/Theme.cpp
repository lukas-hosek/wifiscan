// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "Theme.hpp"
#include <algorithm>

namespace gui::theme
{

ImU32 Color(UiColor element)
{
	switch (element)
	{
	case UiColor::AppBackground:
		return IM_COL32(11, 13, 18, 255);
	case UiColor::Accent:
		return IM_COL32(59, 130, 246, 255);
	case UiColor::PrimaryText:
		return IM_COL32(230, 234, 242, 255);
	case UiColor::DataValue:
		return IM_COL32(200, 208, 222, 255);
	case UiColor::StatusText:
		return IM_COL32(155, 168, 185, 255);
	case UiColor::Muted:
		return IM_COL32(155, 168, 185, 255);
	case UiColor::ShortcutHint:
		return IM_COL32(100, 88, 60, 255);
	case UiColor::ConnectedNetwork:
		return IM_COL32(74, 222, 128, 255);
	case UiColor::Warning:
		return IM_COL32(245, 158, 11, 255);
	case UiColor::SignalStrong:
		return IM_COL32(52, 211, 96, 255);
	case UiColor::SignalGood:
		return IM_COL32(163, 230, 53, 255);
	case UiColor::SignalMedium:
		return IM_COL32(245, 158, 11, 255);
	case UiColor::SignalWeak:
		return IM_COL32(220, 68, 50, 255);
	case UiColor::SignalWeakText:
		return IM_COL32(242, 107, 87, 255);
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
	case UiColor::Border:
		return IM_COL32(38, 44, 58, 255);
	case UiColor::BorderStrong:
		return IM_COL32(51, 56, 71, 255);
	case UiColor::SelectionBg:
		return IM_COL32(59, 130, 246, 36);
	case UiColor::SelectionBgHover:
		return IM_COL32(255, 255, 255, 10);
	case UiColor::SelectionBgActive:
		return IM_COL32(59, 130, 246, 56);
	case UiColor::MeterTrack:
		return IM_COL32(29, 34, 48, 255);
	case UiColor::GridLine:
		return IM_COL32(60, 60, 60, 200);
	case UiColor::SlotDivider:
		return IM_COL32(255, 255, 255, 13);
	case UiColor::EmptySlot:
		return IM_COL32(255, 255, 255, 8);
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

ImU32 SignalTextColor(int signalDbm)
{
	if (signalDbm < -80)
		return Color(UiColor::SignalWeakText);
	return SignalColor(signalDbm);
}

ImU32 WithAlpha(ImU32 color, float alphaScale)
{
	ImU32 alpha = (color >> IM_COL32_A_SHIFT) & 0xFF;
	ImU32 scaled = static_cast<ImU32>(static_cast<float>(alpha) * std::clamp(alphaScale, 0.0f, 1.0f) + 0.5f);
	return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

} // namespace gui::theme
