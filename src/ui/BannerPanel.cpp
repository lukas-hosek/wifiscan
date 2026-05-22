#include "BannerPanel.hpp"
#include "Theme.hpp"
#include <ftxui/dom/elements.hpp>

namespace ui
{

BannerPanel::BannerPanel(std::string interfaceName)
	: _interfaceName(std::move(interfaceName))
{
}

ftxui::Element BannerPanel::Render(const std::vector<wifi::Network>& /*networks*/)
{
	using namespace ftxui;

	return hbox({
		text(" WIFISCAN") | bold | color(theme::Color(theme::UiColor::BannerText)),
		text("  |  interface: " + _interfaceName) | color(theme::Color(theme::UiColor::StatusText)) | dim,
	});
}

} // namespace ui
