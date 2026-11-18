// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/Network.hpp"
#include <imgui.h>
#include <string>
#include <string_view>

namespace gui
{

// The "SSID dBm" text both chart modes attach to an AP: the name in the primary text color, the signal in its
// signal text color, in the small chart font.
class ApLabel
{
public:
	explicit ApLabel(const wifi::Network& net);

	[[nodiscard]] float TextWidth() const { return _textWidth; }

	// Width of the boxed label: text plus horizontal padding and a 1 px border on both sides
	[[nodiscard]] float BoxWidth() const;

	// Draws the text with its top-left at pos; alpha fades both colors (1 = fully opaque).
	void Draw(ImDrawList* draw, ImVec2 pos, float alpha) const;

	// Horizontal padding between the box border and the text, in px at 1x scale
	static constexpr float kPadding = 6.0f;

private:
	std::string_view _ssid;
	std::string _dbm;
	ImU32 _dbmColor;
	float _ssidWidth{0.0f};
	float _spaceWidth{0.0f};
	float _textWidth{0.0f};
};

} // namespace gui
