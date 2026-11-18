// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "Network.hpp"
#include <array>
#include <optional>
#include <span>

namespace wifi
{

// Bands that have a channel axis, in the order the band selectors show them.
inline constexpr std::array<Band, 3> kSupportedBands{Band::GHz2_4, Band::GHz5, Band::GHz6};

// A run of a band's channels, both ends included
struct ChannelRange
{
	int low;
	int high;
};

// The channel axis of one band: which 20 MHz channels it lists, one slot each, and where a frequency falls on it.
class ChannelPlan
{
public:
	explicit ChannelPlan(Band band);

	[[nodiscard]] Band GetBand() const noexcept { return _band; }
	[[nodiscard]] std::span<const int> Channels() const noexcept { return _channels; }

	// Index of the channel's slot on this axis, or -1 when the band doesn't list it.
	[[nodiscard]] int SlotOf(int channel) const noexcept;

	// The lowest and highest listed channel whose centre lies within [lowMhz, highMhz], edges included; nullopt
	// when there is none.
	[[nodiscard]] std::optional<ChannelRange> ChannelRangeWithin(uint32_t lowMhz, uint32_t highMhz) const;

	// Fractional slot position of a frequency: the anchor channel's centre sits mid-slot (slot + 0.5), and one slot
	// spans 5 MHz on 2.4 GHz, 20 MHz on 5/6 GHz. Measuring from a nearby channel instead of the band start keeps the
	// gaps between 5 GHz channel groups from taking room, and copes with 2.4 GHz channel 14, which is off the 5 MHz
	// grid. nullopt when the anchor isn't listed.
	[[nodiscard]] std::optional<float> SlotPositionOf(uint32_t frequencyMhz, int anchorChannel) const;

private:
	Band _band;
	std::span<const int> _channels;
};

} // namespace wifi
