// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "Network.hpp"
#include <array>
#include <optional>
#include <span>
#include <vector>

namespace wifi
{

// Bands that have a channel axis, in the order the band selectors show them.
inline constexpr std::array<Band, 3> kSupportedBands{Band::GHz2_4, Band::GHz5, Band::GHz6};

// An AP's extent on a band's channel axis in slot units: slot i covers [i, i + 1). Values can be fractional
// (2.4 GHz) and can reach past the first or last slot.
struct SlotSpan
{
	// The whole occupied bandwidth
	float lo;
	float hi;
	// The primary 20 MHz channel inside it
	float primaryLo;
	float primaryHi;
};

// The channel axis of one band: which 20 MHz channels it lists and where an AP sits on it.
class ChannelPlan
{
public:
	explicit ChannelPlan(Band band);

	[[nodiscard]] std::span<const int> Channels() const noexcept { return _channels; }

	// Index of the channel's slot on this axis, or -1 when the band doesn't list it.
	[[nodiscard]] int SlotOf(int channel) const noexcept;

	// Where the AP sits on this axis; nullopt when it is on another band or off the axis. On 2.4 GHz the span is
	// the real radio width: channels are only 5 MHz apart, so a 20 MHz AP covers its channel +-2. 40 MHz assumes
	// HT40+ because Network doesn't carry the secondary-channel direction.
	[[nodiscard]] std::optional<SlotSpan> SpanOf(const Network& net) const;

	// Returns the 20 MHz channel slots an AP occupies based on its primary channel and reported operating width.
	// Returns {primary} when the width is unknown or the centre-frequency hint needed to span is missing.
	[[nodiscard]] static std::vector<int> CoveredChannels(const Network& net);

private:
	Band _band;
	std::span<const int> _channels;
};

} // namespace wifi
