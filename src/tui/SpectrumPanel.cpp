// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "SpectrumPanel.hpp"
#include "Theme.hpp"
#include "utils/TextUtil.hpp"
#include "wifi/ChannelPlan.hpp"
#include <algorithm>
#include <array>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <map>
#include <span>

namespace tui
{

using namespace utils;

namespace
{

constexpr int kBarWidth = 3;
// Display width of each channel column in chars (set by the 6-char SSID label)
constexpr int kColWidth = 6;
// Max SSID labels shown per channel column, sorted strongest → weakest
constexpr int kMaxLabels = 5;
// Rows consumed by SSID labels + channel number inside each column
constexpr int kColumnOverhead = kMaxLabels + 1;

constexpr std::array<const char*, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};

struct ChannelOccupancy
{
	// Networks whose PRIMARY channel is this column — labels appear only here.
	std::vector<const wifi::Network*> primaryNets;
	// Networks that extend into this column, 2.4 GHz neighbours included — bar
	// uses the strongest.
	std::vector<const wifi::Network*> coveringNets;
};

// Buckets the visible networks per channel for a single band. Each network is
// recorded once on its primary channel (for label placement) and once on every
// channel it extends into (for bar height/colour).
std::map<int, ChannelOccupancy> GroupByChannel(const std::vector<wifi::Network>& networks, wifi::Band band)
{
	std::span<const int> channels = wifi::ChannelPlan(band).Channels();
	std::map<int, ChannelOccupancy> byChannel;
	for (const auto& net : networks)
	{
		if (net._band != band || net._channel <= 0)
			continue;
		byChannel[net._channel].primaryNets.push_back(&net);
		// The band's channel list, not every integer: 5/6 GHz channel numbers go in steps of 4.
		for (int ch : channels)
		{
			if (ch >= net._channelLow && ch <= net._channelHigh)
				byChannel[ch].coveringNets.push_back(&net);
		}
	}
	for (auto& [ch, occ] : byChannel)
	{
		std::sort(occ.coveringNets.begin(), occ.coveringNets.end(),
			[](const wifi::Network* a, const wifi::Network* b)
			{
				return a->_signalDbm > b->_signalDbm;
			});
		std::sort(occ.primaryNets.begin(), occ.primaryNets.end(),
			[](const wifi::Network* a, const wifi::Network* b)
			{
				return a->_signalDbm > b->_signalDbm;
			});
	}
	return byChannel;
}

// Top-of-column SSID label stack, sorted strongest → weakest, padded to
// kMaxLabels rows.
ftxui::Element BuildLabelsBlock(const std::vector<const wifi::Network*>& sortedNets)
{
	using namespace ftxui;

	std::vector<Element> rows;
	int shown = 0;
	for (const auto* net : sortedNets)
	{
		if (shown >= kMaxLabels)
			break;
		std::string label = net->_ssid.empty() ? "???" : net->_ssid;
		if (label.empty())
			label = "???";
		if (label.size() > static_cast<size_t>(kColWidth))
			label.resize(kColWidth);
		rows.push_back(text(label) | color(theme::SignalColor(net->_signalDbm)) | hcenter);
		++shown;
	}
	for (; shown < kMaxLabels; ++shown)
		rows.push_back(text(std::string(kColWidth, ' ')));
	return vbox(rows);
}

// Bottom-aligned vertical bar; body rows use █ (full block), and the top row
// uses ▁..▇ to draw the fractional remainder for sub-row precision.
// barEighths is the bar height measured in 1/8-row units.
ftxui::Element BuildBarBlock(int barEighths, int topSignalDbm, int maxBarHeight)
{
	using namespace ftxui;

	// U+2581..U+2588 — ▁ ▂ ▃ ▄ ▅ ▆ ▇ █, growing from the cell bottom upward.
	static const std::array<std::string_view, 8> kLowerBlocks{"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
	static constexpr std::string_view kFull{"█"};

	int fullRows = barEighths / 8;
	int remainder = barEighths % 8;
	int capRows = remainder > 0 ? 1 : 0;

	auto repeat = [](std::string_view glyph, int count)
	{
		std::string out;
		out.reserve(glyph.size() * static_cast<size_t>(count));
		for (int i = 0; i < count; i++)
			out.append(glyph);
		return out;
	};

	auto barColor = color(theme::SignalColor(topSignalDbm));

	std::vector<Element> rows;
	for (int i = 0; i < maxBarHeight - fullRows - capRows; i++)
		rows.push_back(text(std::string(kBarWidth, ' ')));
	if (capRows > 0)
		rows.push_back(text(repeat(kLowerBlocks[remainder - 1], kBarWidth)) | barColor | hcenter);
	for (int i = 0; i < fullRows; i++)
		rows.push_back(text(repeat(kFull, kBarWidth)) | barColor | hcenter);
	return vbox(rows);
}

// Channel-number footer — dim for empty channels, highlighted when networks are
// present.
ftxui::Element BuildChannelLabel(int channel, bool hasNetworks)
{
	using namespace ftxui;
	if (hasNetworks)
		return text(std::to_string(channel)) | color(theme::Color(theme::UiColor::DataValue)) | hcenter;
	return text(std::to_string(channel)) | color(theme::Color(theme::UiColor::Muted)) | dim | hcenter;
}

// Empty label stack — used when a channel is only covered by another AP's
// wider spread (no primary AP here), so the bar is drawn but the SSID
// label slot stays blank to avoid repeating the same name across the spread.
ftxui::Element BuildEmptyLabelsBlock()
{
	using namespace ftxui;
	std::vector<Element> rows;
	for (int i = 0; i < kMaxLabels; i++)
		rows.push_back(text(std::string(kColWidth, ' ')));
	return vbox(rows);
}

ftxui::Element BuildPopulatedColumn(int channel, const ChannelOccupancy& occ, int maxBarHeight)
{
	using namespace ftxui;

	const wifi::Network* bar = occ.coveringNets.front();
	int barEighths = std::max(1, bar->SignalQuality() * maxBarHeight * 8 / 100);
	int topSignal = bar->_signalDbm;

	Element labels = occ.primaryNets.empty() ? BuildEmptyLabelsBlock() : BuildLabelsBlock(occ.primaryNets);

	return vbox({
			   labels,
			   BuildBarBlock(barEighths, topSignal, maxBarHeight),
			   BuildChannelLabel(channel, true),
		   }) |
		hcenter;
}

ftxui::Element BuildEmptyColumn(int channel, int maxBarHeight)
{
	using namespace ftxui;

	std::vector<Element> rows;
	for (int i = 0; i < kMaxLabels; i++)
		rows.push_back(text(std::string(kColWidth, ' ')));
	for (int i = 0; i < maxBarHeight; i++)
		rows.push_back(text(std::string(kBarWidth, ' ')));
	rows.push_back(BuildChannelLabel(channel, false));
	return vbox(rows) | hcenter;
}

ftxui::Element RenderBand(const std::vector<wifi::Network>& networks, std::span<const int> channels, wifi::Band band,
	const std::string& label, int maxBarHeight)
{
	using namespace ftxui;

	auto byChannel = GroupByChannel(networks, band);

	std::vector<Element> columns;
	for (int channel : channels)
	{
		auto it = byChannel.find(channel);
		if (it != byChannel.end() && !it->second.coveringNets.empty())
			columns.push_back(BuildPopulatedColumn(channel, it->second, maxBarHeight));
		else
			columns.push_back(BuildEmptyColumn(channel, maxBarHeight));
		columns.push_back(separator() | color(theme::Color(theme::UiColor::Muted)) | dim);
	}
	if (!columns.empty())
		columns.pop_back();

	return window(text(" " + label + " ") | color(theme::Color(theme::UiColor::Border)) | bold, hbox(columns)) |
		color(theme::Color(theme::UiColor::Border));
}

} // namespace

ftxui::Element SpectrumPanel::Render(const std::vector<wifi::Network>& networks, int allocatedHeight)
{
	// Subtract window border (2 rows) to get the drawable interior height,
	// then split into label rows (kColumnOverhead) and bar rows.
	int maxBarHeight = std::max(1, allocatedHeight - kColumnOverhead - 2);

	std::vector<wifi::Network> filtered;
	const std::vector<wifi::Network>* visible = &networks;
	if (_hideConnected)
	{
		filtered.reserve(networks.size());
		for (const auto& net : networks)
			if (!net._connected)
				filtered.push_back(net);
		visible = &filtered;
	}

	wifi::Band band = wifi::kSupportedBands[static_cast<size_t>(_activeBandIndex)];
	std::span<const int> allChannels = wifi::ChannelPlan(band).Channels();
	int totalChannels = static_cast<int>(allChannels.size());

	// Each column occupies kColWidth chars + 1 separator; the last column has
	// no separator. window() border adds 1 char on each side, so total = 2 +
	// N*(kColWidth+1) - 1 = 7N+1. Solving for N: N = (termWidth - 1) /
	// (kColWidth + 1).
	int termWidth = ftxui::Terminal::Size().dimx;
	int visibleCount = std::max(1, (termWidth - 1) / (kColWidth + 1));

	_scrollOffset = std::clamp(_scrollOffset, 0, std::max(0, totalChannels - visibleCount));
	int visibleEnd = std::min(_scrollOffset + visibleCount, totalChannels);

	bool canScrollLeft = _scrollOffset > 0;
	bool canScrollRight = visibleEnd < totalChannels;

	std::string bandLabel = kBandLabels[_activeBandIndex];
	if (canScrollLeft)
		bandLabel = "< " + bandLabel;
	if (canScrollRight)
		bandLabel += " >";
	if (_hideConnected)
		bandLabel += " (connected hidden)";

	return RenderBand(*visible,
		allChannels.subspan(static_cast<size_t>(_scrollOffset), static_cast<size_t>(visibleEnd - _scrollOffset)), band,
		bandLabel, maxBarHeight);
}

bool SpectrumPanel::HandleEvent(ftxui::Event event)
{
	int bandCount = static_cast<int>(wifi::kSupportedBands.size());
	if (event == ftxui::Event::Tab)
	{
		_activeBandIndex = (_activeBandIndex + 1) % bandCount;
		_scrollOffset = 0;
		return true;
	}
	if (event == ftxui::Event::TabReverse)
	{
		_activeBandIndex = (_activeBandIndex + bandCount - 1) % bandCount;
		_scrollOffset = 0;
		return true;
	}
	if (event == ftxui::Event::ArrowLeft)
	{
		if (_scrollOffset > 0)
			_scrollOffset--;
		return true;
	}
	if (event == ftxui::Event::ArrowRight)
	{
		_scrollOffset++; // clamped in Render()
		return true;
	}
	if (event == ftxui::Event::Character('e') || event == ftxui::Event::Character('E'))
	{
		_hideConnected = !_hideConnected;
		return true;
	}
	return false;
}

} // namespace tui
