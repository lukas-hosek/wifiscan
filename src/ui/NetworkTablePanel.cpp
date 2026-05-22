#include "NetworkTablePanel.hpp"
#include "Theme.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <format>
#include <algorithm>
#include <ranges>

namespace ui
{

static uint32_t Utf8Decode(std::string_view str, size_t& pos)
{
	unsigned char byte = static_cast<unsigned char>(str[pos]);
	uint32_t cp;
	int len;
	if      (byte < 0x80)           { cp = byte;        len = 1; }
	else if ((byte & 0xE0) == 0xC0) { cp = byte & 0x1F; len = 2; }
	else if ((byte & 0xF0) == 0xE0) { cp = byte & 0x0F; len = 3; }
	else if ((byte & 0xF8) == 0xF0) { cp = byte & 0x07; len = 4; }
	else                            { ++pos; return 0xFFFD; }
	for (int j = 1; j < len && pos + j < str.size(); ++j)
		cp = (cp << 6) | (static_cast<unsigned char>(str[pos + j]) & 0x3F);
	pos += static_cast<size_t>(len);
	return cp;
}

static bool IsWide(uint32_t cp)
{
	return (cp >= 0x1100 && cp <= 0x115F) ||
	       (cp == 0x2329 || cp == 0x232A) ||
	       (cp >= 0x2E80 && cp <= 0x303E) ||
	       (cp >= 0x3040 && cp <= 0x33FF) ||
	       (cp >= 0x3400 && cp <= 0x4DBF) ||
	       (cp >= 0x4E00 && cp <= 0xA4CF) ||
	       (cp >= 0xAC00 && cp <= 0xD7AF) ||
	       (cp >= 0xF900 && cp <= 0xFAFF) ||
	       (cp >= 0xFE10 && cp <= 0xFE19) ||
	       (cp >= 0xFE30 && cp <= 0xFE4F) ||
	       (cp >= 0xFF00 && cp <= 0xFF60) ||
	       (cp >= 0xFFE0 && cp <= 0xFFE6) ||
	       (cp >= 0x1F300 && cp <= 0x1F9FF) ||
	       (cp >= 0x20000 && cp <= 0x2FFFD) ||
	       (cp >= 0x30000 && cp <= 0x3FFFD);
}

static int Utf8DisplayWidth(std::string_view str)
{
	int width = 0;
	size_t pos = 0;
	while (pos < str.size())
		width += IsWide(Utf8Decode(str, pos)) ? 2 : 1;
	return width;
}

static std::string Utf8TruncateToCols(std::string_view str, int cols)
{
	int width = 0;
	size_t pos = 0;
	while (pos < str.size())
	{
		size_t charStart = pos;
		int charWidth = IsWide(Utf8Decode(str, pos)) ? 2 : 1;
		if (width + charWidth > cols)
			return std::string(str.substr(0, charStart));
		width += charWidth;
	}
	return std::string(str);
}

static std::string PadRight(const std::string& str, int width)
{
	int displayWidth = Utf8DisplayWidth(str);
	if (displayWidth >= width)
		return Utf8TruncateToCols(str, width);
	return str + std::string(static_cast<size_t>(width - displayWidth), ' ');
}

static std::string CenterText(const std::string& str, int width)
{
	int len = Utf8DisplayWidth(str);
	if (len >= width)
		return Utf8TruncateToCols(str, width);
	int leftPad = (width - len) / 2;
	int rightPad = width - len - leftPad;
	return std::string(static_cast<size_t>(leftPad), ' ') + str + std::string(static_cast<size_t>(rightPad), ' ');
}

bool NetworkTablePanel::HandleEvent(ftxui::Event event)
{
	if (event == ftxui::Event::ArrowUp)
	{
		if (_selectedRow > 0)
			_selectedRow--;
		return true;
	}
	if (event == ftxui::Event::ArrowDown)
	{
		_selectedRow++;
		return true;
	}
	if (event == ftxui::Event::Character('s'))
	{
		switch (_sortMode)
		{
			case SortMode::Signal:  _sortMode = SortMode::SSID;    break;
			case SortMode::SSID:    _sortMode = SortMode::Channel;  break;
			case SortMode::Channel: _sortMode = SortMode::Signal;   break;
		}
		_selectedRow = 0;
		return true;
	}
	return false;
}

ftxui::Element NetworkTablePanel::Render(const std::vector<wifi::Network>& networks)
{
	using namespace ftxui;

	// Sort a local copy according to the active sort mode
	std::vector<wifi::Network> sorted = networks;
	switch (_sortMode)
	{
		case SortMode::Signal:
			std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a._signalDbm > b._signalDbm; });
			break;
		case SortMode::SSID:
			std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a._ssid < b._ssid; });
			break;
		case SortMode::Channel:
			std::ranges::sort(sorted, [](const auto& a, const auto& b) { return a._channel < b._channel; });
			break;
	}

	// Clamp selection to valid range
	int rowCount = static_cast<int>(sorted.size());
	if (_selectedRow >= rowCount && rowCount > 0)
		_selectedRow = rowCount - 1;
	if (_selectedRow < 0)
		_selectedRow = 0;

	// Returns label with "*" appended when this column is the active sort key
	auto hdrText = [&](const std::string& label, int width, SortMode mode) -> std::string
	{
		return PadRight(_sortMode == mode ? label + "*" : label, width);
	};
	auto hdrColor = [&](SortMode mode) -> ftxui::Color
	{
		return _sortMode == mode ? theme::Color(theme::UiColor::DataValue) : theme::Color(theme::UiColor::ColumnHeader);
	};

	auto headerRow = hbox({
		text(" ") | color(theme::Color(theme::UiColor::Muted)),
		text(hdrText("SSID", 24, SortMode::SSID))     | color(hdrColor(SortMode::SSID))    | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("BSSID", 17))                    | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(hdrText("CH", 4, SortMode::Channel))      | color(hdrColor(SortMode::Channel)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("BAND", 5))                      | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("FREQ", 7))                      | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(hdrText("SIGNAL", 8, SortMode::Signal))   | color(hdrColor(SortMode::Signal))  | bold,
		text(" | ") | color(theme::Color(theme::UiColor::Muted)),
		text(PadRight("QUAL", 10)) | color(theme::Color(theme::UiColor::ColumnHeader)) | bold,
	});

	std::vector<Element> rows;
	rows.push_back(headerRow);
	rows.push_back(separator() | color(theme::Color(theme::UiColor::Border)));

	if (sorted.empty())
	{
		rows.push_back(
		    text("  (no networks — try: sudo wifiscan)") | color(theme::Color(theme::UiColor::Muted))
		);
	}

	constexpr int kQualBarWidth = 10;

	for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
	{
		const auto& network = sorted[static_cast<size_t>(rowIndex)];

		std::string prefix = network._connected ? "[*] " : " -  ";
		auto prefixColor = network._connected ? theme::Color(theme::UiColor::ConnectedNetwork) : theme::Color(theme::UiColor::Muted);

		std::string ssid = network._ssid.empty() ? "???" : network._ssid;
		auto rowColor = network._connected ? theme::Color(theme::UiColor::ConnectedNetwork) : theme::Color(theme::UiColor::NetworkRow);

		int quality = network.SignalQuality();
		int qualFilled = quality * kQualBarWidth / 100;
		std::string qualCentered = CenterText(std::to_string(quality) + "%", kQualBarWidth);
		auto qualBar = hbox({
			text(qualCentered.substr(0, static_cast<size_t>(qualFilled)))
				| color(ftxui::Color::Black)
				| bgcolor(theme::SignalColor(network._signalDbm)),
			text(qualCentered.substr(static_cast<size_t>(qualFilled)))
				| color(theme::SignalColor(network._signalDbm))
				| bgcolor(ftxui::Color::GrayDark),
		});

		auto dataRow = hbox({
			text(prefix) | color(prefixColor),
			text(PadRight(ssid, 24)) | color(rowColor),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(network._bssid, 17)) | color(theme::Color(theme::UiColor::Muted)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._channel), 4)) | color(theme::Color(theme::UiColor::DataValue)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(wifi::BandLabel(network._band), 5)) | color(theme::Color(theme::UiColor::DataValue)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._frequency) + "M", 7)) | color(theme::Color(theme::UiColor::Muted)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			text(PadRight(std::to_string(network._signalDbm) + " dBm", 8)) | color(theme::SignalColor(network._signalDbm)),
			text(" | ") | color(theme::Color(theme::UiColor::Muted)),
			qualBar,
		});

		if (rowIndex == _selectedRow)
			rows.push_back(dataRow | inverted | focus);
		else
			rows.push_back(dataRow);
	}

	return vbox(rows) | yframe;
}

} // namespace ui
