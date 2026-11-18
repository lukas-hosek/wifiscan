// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ApLabel.hpp"
#include "Fonts.hpp"
#include "ImGuiExtras.hpp"
#include "Theme.hpp"

namespace gui
{

ApLabel::ApLabel(const wifi::Network& net)
	: _ssid(net.DisplaySsid()), _dbm(std::to_string(net._signalDbm)), _dbmColor(theme::SignalTextColor(net._signalDbm))
{
	fonts::ScopedFont font(fonts::FontStyle::Regular, fonts::kSmallSize);
	_ssidWidth = ImGui::CalcTextSize(_ssid.data(), _ssid.data() + _ssid.size()).x;
	_spaceWidth = ImGui::CalcTextSize(" ").x;
	_textWidth = _ssidWidth + _spaceWidth + ImGui::CalcTextSize(_dbm.c_str()).x;
}

float ApLabel::BoxWidth() const { return _textWidth + 2.0f * (kPadding + 1.0f) * DpiScale(); }

void ApLabel::Draw(ImDrawList* draw, ImVec2 pos, float alpha) const
{
	fonts::ScopedFont font(fonts::FontStyle::Regular, fonts::kSmallSize);
	ImU32 ssidColor = theme::WithAlpha(theme::Color(theme::UiColor::PrimaryText), alpha);
	draw->AddText(pos, ssidColor, _ssid.data(), _ssid.data() + _ssid.size());
	draw->AddText(ImVec2(pos.x + _ssidWidth + _spaceWidth, pos.y), theme::WithAlpha(_dbmColor, alpha), _dbm.c_str());
}

} // namespace gui
