#include "Network.hpp"
#include <algorithm>
#include <cstdint>

namespace wifi
{

int Network::SignalQuality() const noexcept
{
	constexpr int kMinDbm = -100;
	constexpr int kMaxDbm = -30;
	int clamped = std::clamp(static_cast<int>(_signalDbm), kMinDbm, kMaxDbm);
	return (clamped - kMinDbm) * 100 / (kMaxDbm - kMinDbm);
}

int Network::SignalBars() const noexcept
{
	int quality = SignalQuality();
	if (quality >= 80) return 5;
	if (quality >= 60) return 4;
	if (quality >= 40) return 3;
	if (quality >= 20) return 2;
	if (quality >= 5)  return 1;
	return 0;
}

int FreqToChannel(uint32_t frequencyMhz) noexcept
{
	if (frequencyMhz == 2484)
		return 14;
	if (frequencyMhz >= 2412 && frequencyMhz <= 2472)
		return static_cast<int>((frequencyMhz - 2407) / 5);
	if (frequencyMhz >= 5180 && frequencyMhz <= 5885)
		return static_cast<int>((frequencyMhz - 5000) / 5);
	if (frequencyMhz >= 5955 && frequencyMhz <= 7115)
		return static_cast<int>((frequencyMhz - 5950) / 5);
	return 0;
}

Band FreqToBand(uint32_t frequencyMhz) noexcept
{
	if (frequencyMhz >= 2400 && frequencyMhz < 2500)
		return Band::GHz2_4;
	if (frequencyMhz >= 5000 && frequencyMhz < 5950)
		return Band::GHz5;
	if (frequencyMhz >= 5950 && frequencyMhz <= 7125)
		return Band::GHz6;
	return Band::Unknown;
}

std::string BandLabel(Band band)
{
	switch (band)
	{
		case Band::GHz2_4: return "2.4G";
		case Band::GHz5:   return "5G";
		case Band::GHz6:   return "6G";
		default:           return "???";
	}
}

} // namespace wifi
