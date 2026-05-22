#pragma once
#include "IPanel.hpp"
#include <string>

namespace ui
{

class BannerPanel : public IPanel
{
public:
	// Interface name displayed as a subtitle below the ASCII art header
	explicit BannerPanel(std::string interfaceName);

	[[nodiscard]] ftxui::Element Render(const std::vector<wifi::Network>& networks) override;
	[[nodiscard]] std::string GetTitle() const override { return "Banner"; }

private:
	std::string _interfaceName;
};

} // namespace ui
