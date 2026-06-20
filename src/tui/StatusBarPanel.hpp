// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "IPanel.hpp"
#include "wifi/IScanner.hpp"
#include <string>

namespace ui
{

class StatusBarPanel : public IPanel
{
public:
	// scanner must outlive this panel
	explicit StatusBarPanel(wifi::IScanner& scanner);

	// Called by App::Render() (render thread only) before Render() to supply
	// the latest scan status string
	void SetStatus(const std::string& msg);

	[[nodiscard]] ftxui::Element
	Render(const std::vector<wifi::Network>& networks, int allocatedHeight) override;

	[[nodiscard]] std::string GetTitle() const override { return "StatusBar"; }

private:
	wifi::IScanner& _scanner;
	std::string _statusMsg;
};

} // namespace ui
