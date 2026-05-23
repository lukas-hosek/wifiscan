// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "StatusBarPanel.hpp"
#include "Theme.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

namespace ui
{

StatusBarPanel::StatusBarPanel(wifi::IScanner& scanner) : _scanner(scanner) {}

void StatusBarPanel::SetStatus(const std::string& msg)
{
	_statusMsg = msg;
}

ftxui::Element StatusBarPanel::Render(
	[[maybe_unused]] const std::vector<wifi::Network>& networks,
	[[maybe_unused]] int allocatedHeight)
{
	using namespace ftxui;

	static constexpr std::string_view kHintsFull =
		" [q] quit  [↑↓] scroll  [←→] spectrum  [Tab] band  [s] sort  [e] "
		"hide-connected ";
	static constexpr std::string_view kHintsCompact = " [q] [↑↓] [←→] [Tab] [s] [e] ";
	// Arrow glyphs are 3 UTF-8 bytes but 1 display column each; there are 4 of
	// them in each hint string.
	static constexpr int kHintColsFull =
		static_cast<int>(kHintsFull.size()) - 4 * 2;

	std::string iface = "iface: " + _scanner.GetInterface();
	int ifaceCols = static_cast<int>(iface.size());
	int termWidth = Terminal::Size().dimx;
	bool showFull = termWidth >= ifaceCols + kHintColsFull;

	std::string_view hints = showFull ? kHintsFull : kHintsCompact;

	return hbox({
		text(_statusMsg + " ") | color(theme::Color(theme::UiColor::StatusText)),
		text(iface) | color(theme::Color(theme::UiColor::DataValue)),
		filler(),
		text(std::string(hints)) | color(theme::Color(theme::UiColor::ShortcutHint)),
	});
}

} // namespace ui
