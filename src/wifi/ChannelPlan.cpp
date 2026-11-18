// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ChannelPlan.hpp"
#include <algorithm>
#include <initializer_list>

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

std::optional<SlotSpan> ChannelPlan::SpanOf(const Network& net) const
{
	if (net._band != _band)
		return std::nullopt;

	int primarySlot = SlotOf(net._channel);
	if (primarySlot < 0)
		return std::nullopt;
	float primary = static_cast<float>(primarySlot);

	if (_band == Band::GHz2_4)
	{
		// One slot is 5 MHz here, so 20 MHz reaches 2 slots either side of the channel centre (slot + 0.5).
		SlotSpan span{primary - 1.5f, primary + 2.5f, primary - 1.5f, primary + 2.5f};
		if (net._widthMhz >= 40)
			span.hi = primary + 6.5f; // HT40+: the secondary 20 MHz sits 4 channels above
		return span;
	}

	int minSlot = static_cast<int>(_channels.size());
	int maxSlot = -1;
	for (int channel : CoveredChannels(net))
	{
		int slot = SlotOf(channel);
		if (slot < 0)
			continue;
		minSlot = std::min(minSlot, slot);
		maxSlot = std::max(maxSlot, slot);
	}
	if (maxSlot < 0)
		return std::nullopt;

	return SlotSpan{static_cast<float>(minSlot), static_cast<float>(maxSlot + 1), primary, primary + 1.0f};
}

std::vector<int> ChannelPlan::CoveredChannels(const Network& net)
{
	int primary = net._channel;
	if (primary <= 0 || net._widthMhz <= 20)
		return {primary};

	// 2.4 GHz HT40 needs the HT-Op secondary-channel offset to know which way the pair extends. We don't surface
	// that on Network, so fall back to primary-only (HT40 on 2.4 GHz is rare in modern deployments).
	if (net._band == Band::GHz2_4)
		return {primary};

	if (net._centerFreq1Mhz == 0)
		return {primary};

	uint32_t baseMhz = (net._band == Band::GHz6) ? 5950U : 5000U;
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
		return build({-30, -26, -22, -18, -14, -10, -6, -2, +2, +6, +10, +14, +18, +22, +26, +30});
	default:
		return {primary};
	}
}

} // namespace wifi
