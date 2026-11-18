// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ChannelPlan.hpp"
#include <algorithm>

namespace wifi
{

namespace
{

constexpr std::array<int, 14> kChannels24{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
constexpr std::array<int, 25> kChannels5{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132,
	136, 140, 144, 149, 153, 157, 161, 165};
constexpr std::array<int, 59> kChannels6{1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77,
	81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173,
	177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233};

std::span<const int> ChannelsOf(Band band)
{
	switch (band)
	{
	case Band::GHz2_4:
		return kChannels24;
	case Band::GHz5:
		return kChannels5;
	case Band::GHz6:
		return kChannels6;
	default:
		return {};
	}
}

} // namespace

ChannelPlan::ChannelPlan(Band band) : _band(band), _channels(ChannelsOf(band)) {}

int ChannelPlan::SlotOf(int channel) const noexcept
{
	auto it = std::find(_channels.begin(), _channels.end(), channel);
	return it == _channels.end() ? -1 : static_cast<int>(it - _channels.begin());
}

std::optional<ChannelRange> ChannelPlan::ChannelRangeWithin(uint32_t lowMhz, uint32_t highMhz) const
{
	std::optional<ChannelRange> range;
	for (int channel : _channels)
	{
		uint32_t centreMhz = ChannelToFreq(channel, _band);
		if (centreMhz < lowMhz || centreMhz > highMhz)
			continue;
		if (range)
			range->high = channel;
		else
			range = ChannelRange{channel, channel};
	}
	return range;
}

std::optional<float> ChannelPlan::SlotPositionOf(uint32_t frequencyMhz, int anchorChannel) const
{
	int anchorSlot = SlotOf(anchorChannel);
	if (anchorSlot < 0)
		return std::nullopt;

	float slotMhz = _band == Band::GHz2_4 ? 5.0f : 20.0f;
	float offsetMhz = static_cast<float>(frequencyMhz) - static_cast<float>(ChannelToFreq(anchorChannel, _band));
	return static_cast<float>(anchorSlot) + 0.5f + offsetMhz / slotMhz;
}

} // namespace wifi
