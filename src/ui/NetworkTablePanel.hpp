#pragma once
#include "IPanel.hpp"

namespace ui
{

class NetworkTablePanel : public IPanel
{
public:
	[[nodiscard]] ftxui::Element Render(const std::vector<wifi::Network>& networks) override;
	[[nodiscard]] std::string GetTitle() const override { return "Networks"; }
	bool HandleEvent(ftxui::Event event) override;

private:
	// Index of the currently highlighted row in the network table
	int _selectedRow{0};
};

} // namespace ui
