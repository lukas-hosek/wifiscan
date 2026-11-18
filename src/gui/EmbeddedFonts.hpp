// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <span>

namespace gui::fonts
{

// Raw TTF bytes compiled into the binary. The definitions are generated at build time by cmake/EmbedFonts.cmake
// from third_party/fonts/JetBrainsMono.
std::span<const unsigned char> JetBrainsMonoRegularTtf();
std::span<const unsigned char> JetBrainsMonoSemiBoldTtf();
std::span<const unsigned char> JetBrainsMonoItalicTtf();

} // namespace gui::fonts
