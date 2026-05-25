// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include <cstdint>
#include <string>

namespace wifi
{

enum class Band
{
	GHz2_4,
	GHz5,
	GHz6,
	Unknown
};

// Detected 802.11 PHY generation. Mapped from which capability IEs the AP
// advertises.
enum class WifiStandard
{
	Unknown,
	A,
	B,
	G,
	N,
	AC,
	AX,
	BE
};

// Detected link-layer security. Derived from the RSN IE, vendor WPA IE and the
// Privacy bit in the BSS Capability info field.
enum class Security
{
	Unknown,
	Open,
	WEP,
	WPA,				  // WPA1 (vendor IE, Microsoft OUI type 1)
	WPA2_Personal,		  // RSN with PSK AKM
	WPA2_Enterprise,	  // RSN with 802.1X AKM
	WPA3_SAE,			  // RSN with SAE AKM
	WPA2_WPA3_Transition, // RSN with both PSK and SAE
	OWE					  // Opportunistic Wireless Encryption
};

struct Network
{
	// Human-readable network name; empty string if the AP broadcasts a hidden
	// SSID
	std::string _ssid;

	// MAC address of the access point formatted as "AA:BB:CC:DD:EE:FF"
	std::string _bssid;

	// Center frequency in MHz (e.g. 2412 for 2.4 GHz channel 1)
	uint32_t _frequency;

	// Received signal strength in dBm; converted from kernel mBm value (divided
	// by 100)
	int32_t _signalDbm;

	// 802.11 channel number derived from frequency
	int _channel;

	Band _band;

	// True when the kernel reports NL80211_BSS_STATUS_ASSOCIATED for this BSS
	bool _connected;

	// Center frequency of the primary 80/160 MHz segment in MHz, derived from
	// the VHT/HE Operation IE (CCFS0). 0 when the AP is 20/40 MHz only or
	// unknown.
	uint32_t _centerFreq1Mhz{0};

	// Operating channel width in MHz: 20, 40, 80, 160, 320. 0 means unknown.
	uint16_t _widthMhz{0};

	// Detected 802.11 PHY generation
	WifiStandard _standard{WifiStandard::Unknown};

	// Detected link-layer security
	Security _security{Security::Unknown};

	// Theoretical max PHY rate in Mbps for a single link (peak MCS × streams ×
	// width). 0 when we lack enough information to estimate.
	uint32_t _maxRateMbps{0};

	// BSS Load IE — number of associated stations. -1 means the AP didn't
	// advertise it.
	int16_t _stationCount{-1};

	// BSS Load IE — channel utilisation byte (0-255, 255 = fully busy). -1 if
	// absent.
	int16_t _channelUtilization{-1};

	// Number of spatial streams (1-8) inferred from the HT/VHT MCS bitmaps. 0 =
	// unknown.
	uint8_t _spatialStreams{0};

	// Returns signal quality mapped linearly from [-100, -30] dBm to [0, 100]
	[[nodiscard]] int SignalQuality() const noexcept;
};

int FreqToChannel(uint32_t frequencyMhz) noexcept;
Band FreqToBand(uint32_t frequencyMhz) noexcept;
std::string BandLabel(Band band);

// Short label for the table column (e.g. "ax", "ac", "n", "-").
std::string StandardLabel(WifiStandard standard);

// Short label for the security column (e.g. "WPA3", "WPA2", "WPA2/3", "Open").
std::string SecurityLabel(Security security);

} // namespace wifi
