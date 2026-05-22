#pragma once
#include "IPanel.hpp"
#include <span>

namespace ui
{

class SpectrumPanel : public IPanel
{
public:
	[[nodiscard]] ftxui::Element Render(const std::vector<wifi::Network>& networks) override;
	[[nodiscard]] std::string GetTitle() const override { return "Spectrum"; }
	[[nodiscard]] bool HandleEvent(ftxui::Event event) override;

private:
	// Index into {GHz2_4, GHz5, GHz6}; cycled by Tab / Shift+Tab
	int _activeBandIndex{0};
	// First visible channel index within the current band's full channel list
	int _scrollOffset{0};

	// Renders the given slice of channels for one band
	[[nodiscard]] ftxui::Element RenderBand(
	    const std::vector<wifi::Network>& networks,
	    std::span<const int> channels,
	    wifi::Band band,
	    const std::string& label,
	    int maxBarHeight) const;
};

} // namespace ui
