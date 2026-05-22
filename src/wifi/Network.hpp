#pragma once
#include <string>
#include <cstdint>

namespace wifi
{

enum class Band { GHz2_4, GHz5, GHz6, Unknown };

struct Network
{
	// Human-readable network name; empty string if the AP broadcasts a hidden SSID
	std::string _ssid;

	// MAC address of the access point formatted as "AA:BB:CC:DD:EE:FF"
	std::string _bssid;

	// Center frequency in MHz (e.g. 2412 for 2.4 GHz channel 1)
	uint32_t _frequency;

	// Received signal strength in dBm; converted from kernel mBm value (divided by 100)
	int32_t _signalDbm;

	// 802.11 channel number derived from frequency
	int _channel;

	Band _band;

	// True when the kernel reports NL80211_BSS_STATUS_ASSOCIATED for this BSS
	bool _connected;

	// Returns signal quality mapped linearly from [-100, -30] dBm to [0, 100]
	[[nodiscard]] int SignalQuality() const noexcept;

	// Returns a 0–5 bar count suitable for a signal strength indicator
	[[nodiscard]] int SignalBars() const noexcept;
};

int FreqToChannel(uint32_t frequencyMhz) noexcept;
Band FreqToBand(uint32_t frequencyMhz) noexcept;
std::string BandLabel(Band band);

} // namespace wifi
