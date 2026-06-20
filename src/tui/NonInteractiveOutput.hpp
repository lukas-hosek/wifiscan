// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/IScanner.hpp"

namespace ui
{

// Performs one fresh scan via the given scanner, writes the resulting network
// list to stdout as a plain-text table (no ANSI, no FTXUI), then returns
// EXIT_SUCCESS. Mirrors NetworkTablePanel columns, omitting QUAL. Networks
// are sorted by signal descending, SSID ascending tiebreak. Exceptions
// (wifi::ScanError, std::exception) propagate to the caller.
int RunNonInteractive(wifi::IScanner& scanner);

} // namespace ui
