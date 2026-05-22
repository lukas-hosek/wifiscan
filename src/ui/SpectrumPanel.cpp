#include "SpectrumPanel.hpp"
#include "Theme.hpp"
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <array>
#include <map>
#include <span>
#include <algorithm>

namespace ui
{

static constexpr int kBarWidth = 3;
// Display width of each channel column in chars (set by the 6-char SSID label)
static constexpr int kColWidth = 6;
// Rows consumed by SSID labels (2) + channel number (1) inside each column
static constexpr int kColumnOverhead = 3;

static constexpr std::array<int, 14> kChannels24{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
static constexpr std::array<int, 25> kChannels5{
	36,  40,  44,  48,
	52,  56,  60,  64,
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165
};
static constexpr std::array<int, 59> kChannels6{
	  1,   5,   9,  13,  17,  21,  25,  29,  33,  37,
	 41,  45,  49,  53,  57,  61,  65,  69,  73,  77,
	 81,  85,  89,  93,  97, 101, 105, 109, 113, 117,
	121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
	161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
	201, 205, 209, 213, 217, 221, 225, 229, 233
};

static std::span<const int> BandChannels(int bandIndex)
{
	switch (bandIndex)
	{
		case 0:  return kChannels24;
		case 1:  return kChannels5;
		default: return kChannels6;
	}
}

ftxui::Element SpectrumPanel::RenderBand(
    const std::vector<wifi::Network>& networks,
    std::span<const int> channels,
    wifi::Band band,
    const std::string& label,
    int maxBarHeight) const
{
	using namespace ftxui;

	std::map<int, std::vector<const wifi::Network*>> channelNetworks;
	for (const auto& network : networks)
	{
		if (network._band == band && network._channel > 0)
			channelNetworks[network._channel].push_back(&network);
	}

	std::vector<Element> columns;

	for (int channel : channels)
	{
		std::vector<Element> columnElements;
		auto it = channelNetworks.find(channel);

		if (it != channelNetworks.end())
		{
			const auto& channelNets = it->second;

			const wifi::Network* strongest = *std::max_element(
			    channelNets.begin(), channelNets.end(),
			    [](const wifi::Network* lhs, const wifi::Network* rhs)
			    {
			        return lhs->_signalDbm < rhs->_signalDbm;
			    });

			int barHeight = std::max(1, strongest->SignalQuality() * maxBarHeight / 100);

			int labelsShown = 0;
			for (const auto* net : channelNets)
			{
				if (labelsShown >= 2) break;
				std::string ssidLabel = net->_ssid.empty() ? "???" : net->_ssid;
				if (ssidLabel.size() > (size_t)kColWidth)
					ssidLabel = ssidLabel.substr(0, kColWidth);
				columnElements.push_back(
				    text(ssidLabel) | color(theme::SignalColor(net->_signalDbm)) | hcenter
				);
				labelsShown++;
			}
			for (; labelsShown < 2; ++labelsShown)
				columnElements.push_back(text(std::string(kColWidth, ' ')));

			// Bar blocks, bottom-aligned
			for (int row = 0; row < maxBarHeight - barHeight; row++)
				columnElements.push_back(text(std::string(kBarWidth, ' ')));
			for (int row = 0; row < barHeight; row++)
			{
				columnElements.push_back(
				    text(std::string(kBarWidth, '|')) |
				    color(theme::SignalColor(strongest->_signalDbm)) | hcenter
				);
			}

			columnElements.push_back(
			    text(std::to_string(channel)) |
			    color(theme::Color(theme::UiColor::DataValue)) | hcenter
			);
		}
		else
		{
			// Empty channel — no networks detected
			columnElements.push_back(text(std::string(kColWidth, ' ')));
			columnElements.push_back(text(std::string(kColWidth, ' ')));
			for (int row = 0; row < maxBarHeight; row++)
				columnElements.push_back(text(std::string(kBarWidth, ' ')));
			columnElements.push_back(
			    text(std::to_string(channel)) |
			    color(theme::Color(theme::UiColor::Muted)) | dim | hcenter
			);
		}

		columns.push_back(vbox(columnElements) | hcenter);
		columns.push_back(separator() | color(theme::Color(theme::UiColor::Muted)) | dim);
	}

	if (!columns.empty())
		columns.pop_back();

	return window(
	    text(" " + label + " ") | color(theme::Color(theme::UiColor::Border)) | bold,
	    hbox(columns)
	) | color(theme::Color(theme::UiColor::Border));
}

ftxui::Element SpectrumPanel::Render(const std::vector<wifi::Network>& networks)
{
	static constexpr std::array<wifi::Band, 3> kBands{
	    wifi::Band::GHz2_4, wifi::Band::GHz5, wifi::Band::GHz6
	};
	static constexpr std::array<const char*, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};

	// Fixed rows in the overall layout: banner + 3 separators + status = 5
	// Spectrum gets 1/3 of the remaining height; subtract window border (2) and overhead
	int available    = ftxui::Terminal::Size().dimy - 5;
	int spectrumRows = std::max(kColumnOverhead + 1, available / 3);
	int maxBarHeight = std::max(1, spectrumRows - kColumnOverhead - 2); // -2 for window border

	std::span<const int> allChannels = BandChannels(_activeBandIndex);
	int totalChannels = (int)allChannels.size();

	// Each column occupies kColWidth chars + 1 separator; the last column has no separator.
	// window() border adds 1 char on each side, so total = 2 + N*(kColWidth+1) - 1 = 7N+1.
	// Solving for N: N = (termWidth - 1) / (kColWidth + 1).
	int termWidth    = ftxui::Terminal::Size().dimx;
	int visibleCount = std::max(1, (termWidth - 1) / (kColWidth + 1));

	_scrollOffset = std::clamp(_scrollOffset, 0, std::max(0, totalChannels - visibleCount));
	int visibleEnd = std::min(_scrollOffset + visibleCount, totalChannels);

	bool canScrollLeft  = _scrollOffset > 0;
	bool canScrollRight = visibleEnd < totalChannels;

	std::string bandLabel = kBandLabels[_activeBandIndex];
	if (canScrollLeft)  bandLabel = "< " + bandLabel;
	if (canScrollRight) bandLabel += " >";

	return RenderBand(
	    networks,
	    allChannels.subspan((size_t)_scrollOffset, (size_t)(visibleEnd - _scrollOffset)),
	    kBands[_activeBandIndex],
	    bandLabel,
	    maxBarHeight
	);
}

bool SpectrumPanel::HandleEvent(ftxui::Event event)
{
	if (event == ftxui::Event::Tab)
	{
		_activeBandIndex = (_activeBandIndex + 1) % 3;
		_scrollOffset = 0;
		return true;
	}
	if (event == ftxui::Event::TabReverse)
	{
		_activeBandIndex = (_activeBandIndex + 2) % 3;
		_scrollOffset = 0;
		return true;
	}
	if (event == ftxui::Event::ArrowLeft)
	{
		if (_scrollOffset > 0) _scrollOffset--;
		return true;
	}
	if (event == ftxui::Event::ArrowRight)
	{
		_scrollOffset++;  // clamped in Render()
		return true;
	}
	return false;
}

} // namespace ui
