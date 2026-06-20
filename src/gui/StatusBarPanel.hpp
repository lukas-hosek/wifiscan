// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/IScanner.hpp"
#include <string>

namespace gui
{

// Bottom status line: scan status message + interface + key hints.
// Mirrors ui::StatusBarPanel (src/tui/StatusBarPanel.hpp).
class StatusBarPanel
{
public:
	explicit StatusBarPanel(wifi::IScanner& scanner);

	void SetStatus(const std::string& msg);

	void Render();

private:
	wifi::IScanner& _scanner;

	// Human-readable status line updated after each scan
	std::string _statusMsg{"Scanning..."};
};

} // namespace gui
