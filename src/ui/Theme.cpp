// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "Theme.hpp"

namespace ui::theme
{

ftxui::Color Color(UiColor element)
{
	switch (element)
	{
	case UiColor::AppBackground:
		return ftxui::Color::Black;
	case UiColor::Border:
	{
		static const ftxui::Color c = ftxui::Color::RGB(0, 180, 0);
		return c;
	}
	case UiColor::BannerText:
		return ftxui::Color::CyanLight;
	case UiColor::ColumnHeader:
		return ftxui::Color::CyanLight;
	case UiColor::DataValue:
		return ftxui::Color::Cyan;
	case UiColor::StatusText:
		return ftxui::Color::Green;
	case UiColor::NetworkRow:
		return ftxui::Color::GreenLight;
	case UiColor::ConnectedNetwork:
	{
		static const ftxui::Color c = ftxui::Color::RGB(0, 255, 128);
		return c;
	}
	case UiColor::SignalStrong:
		return ftxui::Color::GreenLight;
	case UiColor::SignalGood:
		return ftxui::Color::Green;
	case UiColor::SignalMedium:
		return ftxui::Color::Yellow;
	case UiColor::SignalWeak:
		return ftxui::Color::Red;
	case UiColor::Muted:
		return ftxui::Color::GrayDark;
	case UiColor::ShortcutHint:
	{
		static const ftxui::Color c = ftxui::Color::RGB(180, 160, 90);
		return c;
	}
	}
	return ftxui::Color::Default;
}

ftxui::Color SignalColor(int signalDbm)
{
	if (signalDbm >= -60)
		return Color(UiColor::SignalStrong);
	if (signalDbm >= -70)
		return Color(UiColor::SignalGood);
	if (signalDbm >= -80)
		return Color(UiColor::SignalMedium);
	return Color(UiColor::SignalWeak);
}

} // namespace ui::theme
