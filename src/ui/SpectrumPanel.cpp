#include "SpectrumPanel.hpp"
#include "Theme.hpp"
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <array>
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

		constexpr std::array<int, 14> kChannels24{
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
		constexpr std::array<int, 25> kChannels5{
			36, 40, 44, 48,
			52, 56, 60, 64,
			100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
			149, 153, 157, 161, 165};
		constexpr std::array<int, 59> kChannels6{
			1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
			41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
			81, 85, 89, 93, 97, 101, 105, 109, 113, 117,
			121, 125, 129, 133, 137, 141, 145, 149, 153, 157,
			161, 165, 169, 173, 177, 181, 185, 189, 193, 197,
			201, 205, 209, 213, 217, 221, 225, 229, 233};

		constexpr std::array<wifi::Band, 3> kBands{
			wifi::Band::GHz2_4, wifi::Band::GHz5, wifi::Band::GHz6};
		constexpr std::array<const char *, 3> kBandLabels{"2.4 GHz", "5 GHz", "6 GHz"};

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

		// Buckets the visible networks by channel for a single band, dropping
		// entries with no channel info.
		std::map<int, std::vector<const wifi::Network *>> GroupByChannel(
			const std::vector<wifi::Network> &networks, wifi::Band band)
		{
			std::map<int, std::vector<const wifi::Network *>> byChannel;
			for (const auto &net : networks)
			{
				if (net._band == band && net._channel > 0)
					byChannel[net._channel].push_back(&net);
			}
			return byChannel;
		}

		// Top-of-column SSID label stack, sorted strongest → weakest, padded to kMaxLabels rows.
		ftxui::Element BuildLabelsBlock(const std::vector<const wifi::Network *> &sortedNets)
		{
			using namespace ftxui;

			std::vector<Element> rows;
			int shown = 0;
			for (const auto *net : sortedNets)
			{
				if (shown >= kMaxLabels)
					break;
				std::string label = net->_ssid.empty() ? "???" : net->_ssid;
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
			static const std::array<std::string_view, 8> kLowerBlocks{
				"▁", "▂", "▃", "▄",
				"▅", "▆", "▇", "█"};
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

		// Channel-number footer — dim for empty channels, highlighted when networks are present.
		ftxui::Element BuildChannelLabel(int channel, bool hasNetworks)
		{
			using namespace ftxui;
			if (hasNetworks)
				return text(std::to_string(channel)) | color(theme::Color(theme::UiColor::DataValue)) | hcenter;
			return text(std::to_string(channel)) | color(theme::Color(theme::UiColor::Muted)) | dim | hcenter;
		}

		ftxui::Element BuildPopulatedColumn(int channel, std::vector<const wifi::Network *> nets, int maxBarHeight)
		{
			using namespace ftxui;

			std::sort(nets.begin(), nets.end(),
					  [](const wifi::Network *lhs, const wifi::Network *rhs)
					  {
						  return lhs->_signalDbm > rhs->_signalDbm;
					  });

			int barEighths = std::max(1, nets.front()->SignalQuality() * maxBarHeight * 8 / 100);
			int topSignal = nets.front()->_signalDbm;

			return vbox({
					   BuildLabelsBlock(nets),
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

		ftxui::Element RenderBand(
			const std::vector<wifi::Network> &networks,
			std::span<const int> channels,
			wifi::Band band,
			const std::string &label,
			int maxBarHeight)
		{
			using namespace ftxui;

			auto byChannel = GroupByChannel(networks, band);

			std::vector<Element> columns;
			for (int channel : channels)
			{
				auto it = byChannel.find(channel);
				if (it != byChannel.end())
					columns.push_back(BuildPopulatedColumn(channel, it->second, maxBarHeight));
				else
					columns.push_back(BuildEmptyColumn(channel, maxBarHeight));
				columns.push_back(separator() | color(theme::Color(theme::UiColor::Muted)) | dim);
			}
			if (!columns.empty())
				columns.pop_back();

			return window(
					   text(" " + label + " ") | color(theme::Color(theme::UiColor::Border)) | bold,
					   hbox(columns)) |
				   color(theme::Color(theme::UiColor::Border));
		}

	} // namespace

	ftxui::Element SpectrumPanel::Render(const std::vector<wifi::Network> &networks)
	{
		// Fixed rows in the overall layout: 2 separators + status = 3
		// Spectrum gets 2/5 of the remaining height; subtract window border (2) and overhead
		int available = ftxui::Terminal::Size().dimy - 3;
		int spectrumRows = std::max(kColumnOverhead + 1, available * 2 / 5);
		int maxBarHeight = std::max(1, spectrumRows - kColumnOverhead - 2);

		std::span<const int> allChannels = BandChannels(_activeBandIndex);
		int totalChannels = static_cast<int>(allChannels.size());

		// Each column occupies kColWidth chars + 1 separator; the last column has no separator.
		// window() border adds 1 char on each side, so total = 2 + N*(kColWidth+1) - 1 = 7N+1.
		// Solving for N: N = (termWidth - 1) / (kColWidth + 1).
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

		return RenderBand(
			networks,
			allChannels.subspan(static_cast<size_t>(_scrollOffset), static_cast<size_t>(visibleEnd - _scrollOffset)),
			kBands[_activeBandIndex],
			bandLabel,
			maxBarHeight);
	}

	bool SpectrumPanel::HandleEvent(ftxui::Event event)
	{
		if (event == ftxui::Event::Tab)
		{
			_activeBandIndex = (_activeBandIndex + 1) % static_cast<int>(kBands.size());
			_scrollOffset = 0;
			return true;
		}
		if (event == ftxui::Event::TabReverse)
		{
			_activeBandIndex = (_activeBandIndex + static_cast<int>(kBands.size()) - 1) % static_cast<int>(kBands.size());
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
		return false;
	}

} // namespace ui
