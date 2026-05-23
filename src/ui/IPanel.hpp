#pragma once
#include "wifi/Network.hpp"
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

namespace ui
{

class IPanel
{
public:
	virtual ~IPanel() = default;

	// Builds and returns the FTXUI element tree for this panel
	[[nodiscard]] virtual ftxui::Element
	Render(const std::vector<wifi::Network>& networks) = 0;

	// Short display name used for debugging and status messages
	[[nodiscard]] virtual std::string GetTitle() const = 0;

	// Called by App for every keyboard event before the default handler.
	// Returns true if the event was consumed and should not propagate further.
	virtual bool HandleEvent(ftxui::Event event)
	{
		(void)event;
		return false;
	}
};

} // namespace ui
