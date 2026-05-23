// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <string>
#include <string_view>

namespace utils
{

// Pads str on the right with spaces until it occupies the given number of
// terminal columns. If str is already wider, it is truncated to fit.
std::string PadRight(const std::string& str, int width);

// Removes terminal-unsafe control/formatting characters while preserving
// printable text for display in the TUI.
std::string SanitizeForTerminal(std::string_view str);

// Centers str within the given number of terminal columns by padding both
// sides with spaces. If str is already wider, it is truncated to fit.
std::string CenterText(const std::string& str, int width);

} // namespace utils
