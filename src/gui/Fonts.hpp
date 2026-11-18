// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <imgui.h>

namespace gui::fonts
{

enum class FontStyle
{
	Regular,
	// Weight 600: channel numbers and primary-channel ticks
	SemiBold,
	// Placeholder text such as "(hidden)"
	Italic,
};

// Text sizes in px at 1x scale. They are passed to ImGui::PushFont unscaled; ImGui applies style.FontScaleDpi.
constexpr float kBodySize = 13.0f;
// Chart labels, channel ticks, dBm axis
constexpr float kSmallSize = 12.0f;
// Row captions
constexpr float kCaptionSize = 11.0f;
// All text in the network list (header and pills included) and the toolbar toggles above the chart. Larger than
// the rest: these are read the most, and their rows have the room.
constexpr float kLargeSize = 15.0f;

// Registers the embedded JetBrains Mono faces with the ImGui font atlas. Regular becomes the default font. Call
// once, right after ImGui::CreateContext().
void Load();

[[nodiscard]] ImFont* Get(FontStyle style);

// Pushes a font face and size for the lifetime of the object
class ScopedFont
{
public:
	ScopedFont(FontStyle style, float size);
	~ScopedFont();
	ScopedFont(const ScopedFont&) = delete;
	ScopedFont& operator=(const ScopedFont&) = delete;
};

} // namespace gui::fonts
