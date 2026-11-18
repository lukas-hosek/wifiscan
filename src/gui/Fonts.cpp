// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "Fonts.hpp"
#include "EmbeddedFonts.hpp"
#include <array>
#include <span>

namespace gui::fonts
{

namespace
{

std::array<ImFont*, 3> loadedFonts{};

ImFont* AddEmbeddedFont(std::span<const unsigned char> ttf)
{
	ImFontConfig config;
	// The bytes are static and read-only; ImGui must neither free nor take ownership of them.
	config.FontDataOwnedByAtlas = false;
	// ImGui only reads the buffer, but its API takes a mutable pointer.
	void* data = const_cast<unsigned char*>(ttf.data());
	return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, static_cast<int>(ttf.size()), kBodySize, &config);
}

} // namespace

void Load()
{
	// Regular is added first, which makes it ImGui's default font.
	loadedFonts[static_cast<size_t>(FontStyle::Regular)] = AddEmbeddedFont(JetBrainsMonoRegularTtf());
	loadedFonts[static_cast<size_t>(FontStyle::SemiBold)] = AddEmbeddedFont(JetBrainsMonoSemiBoldTtf());
	loadedFonts[static_cast<size_t>(FontStyle::Italic)] = AddEmbeddedFont(JetBrainsMonoItalicTtf());
}

ImFont* Get(FontStyle style) { return loadedFonts[static_cast<size_t>(style)]; }

ScopedFont::ScopedFont(FontStyle style, float size) { ImGui::PushFont(Get(style), size); }

ScopedFont::~ScopedFont() { ImGui::PopFont(); }

} // namespace gui::fonts
