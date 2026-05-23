#include "SpectrumPanel.hpp"
#include "TextUtil.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <array>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <map>
#include <span>

namespace ui
{

namespace
{

constexpr int kBarWidth = 3;
// Display width of each channel column in chars (set by the 6-char SSID label)
constexpr int kColWidth = 6;
// Max SSID labels shown per channel column, sorted strongest → weakest
constexpr int kMaxLabels = 5;
// Rows consumed by SSID labels + channel number inside each column
constexpr int kColumnOverhead = kMaxLabels + 1;

constexpr std::array<int, 14> kChannels24{1, 2, 3,	4,	5,	6,	7,
										  8, 9, 10, 11, 12, 13, 14};
constexpr std::array<int, 25> kChannels5{
	36,	 40,  44,  48,	52,	 56,  60,  64,	100, 104, 108, 112, 116,
	120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165};
constexpr std::array<int, 59> kChannels6{
	1,	 5,	  9,   13,	17,	 21,  25,  29,	33,	 37,  41,  45,	49,	 53,  57,
	61,	 65,  69,  73,	77,	 81,  85,  89,	93,	 97,  101, 105, 109, 113, 117,
	121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173, 177,
	181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233};

constexpr std::array<wifi::Band, 3> kBands{wifi::Band::GHz2_4, wifi::Band::GHz5,
										   wifi::Band::GHz6};
constexpr std::array<const char*, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};

std::span<const int> BandChannels(int bandIndex)
{
	switch (bandIndex)
	{
	case 0:
		return kChannels24;
	case 1:
		return kChannels5;
	default:
		return kChannels6;
	}
}

// Returns the list of 20-MHz channel slots an AP occupies based on its
// primary channel + reported operating width. Returns {primary} when the
// width is unknown or we lack the centre-frequency hint needed to span.
std::vector<int> CoveredChannels(const wifi::Network& net)
{
	int primary = net._channel;
	if (primary <= 0 || net._widthMhz <= 20)
		return {primary};

	// 2.4 GHz HT40 needs the HT-Op secondary-channel offset to know which
	// way the pair extends. We don't surface that on Network, so fall back
	// to primary-only (HT40 on 2.4 GHz is rare in modern deployments).
	if (net._band == wifi::Band::GHz2_4)
		return {primary};

	if (net._centerFreq1Mhz == 0)
		return {primary};

	uint32_t baseMhz = (net._band == wifi::Band::GHz6) ? 5950U : 5000U;
	int centerCh = static_cast<int>((net._centerFreq1Mhz - baseMhz) / 5);

	auto build = [&](std::initializer_list<int> offsets)
	{
		std::vector<int> out;
		out.reserve(offsets.size());
		for (int o : offsets)
			out.push_back(centerCh + o);
		return out;
	};

	switch (net._widthMhz)
	{
	case 40:
		return build({-2, +2});
	case 80:
		return build({-6, -2, +2, +6});
	case 160:
		return build({-14, -10, -6, -2, +2, +6, +10, +14});
	case 320:
		return build({-30, -26, -22, -18, -14, -10, -6, -2, +2, +6, +10, +14,
					  +18, +22, +26, +30});
	default:
		return {primary};
	}
}

struct ChannelOccupancy
{
	// Networks whose PRIMARY channel is this column — labels appear only here.
	std::vector<const wifi::Network*> primaryNets;
	// Networks whose operating spread touches this column — bar uses the
	// strongest.
	std::vector<const wifi::Network*> coveringNets;
};

// Buckets the visible networks per channel for a single band. Each network is
// recorded once on its primary channel (for label placement) and once on every
// channel its operating bandwidth spans (for bar height/colour).
std::map<int, ChannelOccupancy>
GroupByChannel(const std::vector<wifi::Network>& networks, wifi::Band band)
{
	std::map<int, ChannelOccupancy> byChannel;
	for (const auto& net : networks)
	{
		if (net._band != band || net._channel <= 0)
			continue;
		byChannel[net._channel].primaryNets.push_back(&net);
		for (int ch : CoveredChannels(net))
		{
			if (ch > 0)
				byChannel[ch].coveringNets.push_back(&net);
		}
	}
	for (auto& [ch, occ] : byChannel)
	{
		std::sort(occ.coveringNets.begin(), occ.coveringNets.end(),
				  [](const wifi::Network* a, const wifi::Network* b)
				  { return a->_signalDbm > b->_signalDbm; });
		std::sort(occ.primaryNets.begin(), occ.primaryNets.end(),
				  [](const wifi::Network* a, const wifi::Network* b)
				  { return a->_signalDbm > b->_signalDbm; });
	}
	return byChannel;
}

// Top-of-column SSID label stack, sorted strongest → weakest, padded to
// kMaxLabels rows.
ftxui::Element
BuildLabelsBlock(const std::vector<const wifi::Network*>& sortedNets)
{
	using namespace ftxui;

	std::vector<Element> rows;
	int shown = 0;
	for (const auto* net : sortedNets)
	{
		if (shown >= kMaxLabels)
			break;
		std::string label =
			net->_ssid.empty() ? "???" : SanitizeForTerminal(net->_ssid);
		if (label.empty())
			label = "???";
		if (label.size() > static_cast<size_t>(kColWidth))
			label.resize(kColWidth);
		rows.push_back(text(label) |
					   color(theme::SignalColor(net->_signalDbm)) | hcenter);
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
	static const std::array<std::string_view, 8> kLowerBlocks{
		"▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};
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
		rows.push_back(text(repeat(kLowerBlocks[remainder - 1], kBarWidth)) |
					   barColor | hcenter);
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
		return text(std::to_string(channel)) |
			   color(theme::Color(theme::UiColor::DataValue)) | hcenter;
	return text(std::to_string(channel)) |
		   color(theme::Color(theme::UiColor::Muted)) | dim | hcenter;
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

ftxui::Element BuildPopulatedColumn(int channel, const ChannelOccupancy& occ,
									int maxBarHeight)
{
	using namespace ftxui;

	const wifi::Network* bar = occ.coveringNets.front();
	int barEighths = std::max(1, bar->SignalQuality() * maxBarHeight * 8 / 100);
	int topSignal = bar->_signalDbm;

	Element labels = occ.primaryNets.empty()
						 ? BuildEmptyLabelsBlock()
						 : BuildLabelsBlock(occ.primaryNets);

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

ftxui::Element RenderBand(const std::vector<wifi::Network>& networks,
						  std::span<const int> channels, wifi::Band band,
						  const std::string& label, int maxBarHeight)
{
	using namespace ftxui;

	auto byChannel = GroupByChannel(networks, band);

	std::vector<Element> columns;
	for (int channel : channels)
	{
		auto it = byChannel.find(channel);
		if (it != byChannel.end() && !it->second.coveringNets.empty())
			columns.push_back(
				BuildPopulatedColumn(channel, it->second, maxBarHeight));
		else
			columns.push_back(BuildEmptyColumn(channel, maxBarHeight));
		columns.push_back(separator() |
						  color(theme::Color(theme::UiColor::Muted)) | dim);
	}
	if (!columns.empty())
		columns.pop_back();

	return window(text(" " + label + " ") |
					  color(theme::Color(theme::UiColor::Border)) | bold,
				  hbox(columns)) |
		   color(theme::Color(theme::UiColor::Border));
}

} // namespace

ftxui::Element SpectrumPanel::Render(const std::vector<wifi::Network>& networks)
{
	// Fixed rows in the overall layout: 2 separators + status = 3
	// Spectrum gets 2/5 of the remaining height; subtract window border (2) and
	// overhead
	int available = ftxui::Terminal::Size().dimy - 3;
	int spectrumRows = std::max(kColumnOverhead + 1, available * 2 / 5);
	int maxBarHeight = std::max(1, spectrumRows - kColumnOverhead - 2);

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

	std::span<const int> allChannels = BandChannels(_activeBandIndex);
	int totalChannels = static_cast<int>(allChannels.size());

	// Each column occupies kColWidth chars + 1 separator; the last column has
	// no separator. window() border adds 1 char on each side, so total = 2 +
	// N*(kColWidth+1) - 1 = 7N+1. Solving for N: N = (termWidth - 1) /
	// (kColWidth + 1).
	int termWidth = ftxui::Terminal::Size().dimx;
	int visibleCount = std::max(1, (termWidth - 1) / (kColWidth + 1));

	_scrollOffset =
		std::clamp(_scrollOffset, 0, std::max(0, totalChannels - visibleCount));
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

	return RenderBand(
		*visible,
		allChannels.subspan(static_cast<size_t>(_scrollOffset),
							static_cast<size_t>(visibleEnd - _scrollOffset)),
		kBands[_activeBandIndex], bandLabel, maxBarHeight);
}

bool SpectrumPanel::HandleEvent(ftxui::Event event)
{
	if (event == ftxui::Event::Tab)
	{
		_activeBandIndex =
			(_activeBandIndex + 1) % static_cast<int>(kBands.size());
		_scrollOffset = 0;
		return true;
	}
	if (event == ftxui::Event::TabReverse)
	{
		_activeBandIndex =
			(_activeBandIndex + static_cast<int>(kBands.size()) - 1) %
			static_cast<int>(kBands.size());
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
	if (event == ftxui::Event::Character('e') ||
		event == ftxui::Event::Character('E'))
	{
		_hideConnected = !_hideConnected;
		return true;
	}
	return false;
}

} // namespace ui
