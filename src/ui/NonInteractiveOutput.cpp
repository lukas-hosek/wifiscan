// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NonInteractiveOutput.hpp"
#include "utils/TextUtil.hpp"
#include "wifi/Network.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ranges>
#include <stop_token>
#include <string>

namespace ui
{

namespace
{

struct Column
{
	const char* header;
	int width;
};

// Display order mirrors NetworkTablePanel::kColumns minus QUAL.
constexpr std::array<Column, 9> kColumns = {{
	{"  SSID", 28},
	{"BSSID", 17},
	{"CH", 4},
	{"BAND", 4},
	{"W", 3},
	{"STD", 3},
	{"SEC", 7},
	{"RATE", 7},
	{"SIGNAL", 8},
}};

constexpr const char* kSeparator = " | ";

std::string FormatSsidCell(const wifi::Network& net)
{
	std::string ssid = net._ssid.empty() ? "???" : net._ssid;
	std::string prefix = net._connected ? "* " : "  ";
	return prefix + utils::PadRight(ssid, 26);
}

std::string FormatRow(const wifi::Network& net)
{
	std::array<std::string, 9> cells;
	cells[0] = FormatSsidCell(net);
	cells[1] = utils::PadRight(net._bssid, 17);
	cells[2] = utils::PadRight(std::to_string(net._channel), 4);
	cells[3] = utils::PadRight(wifi::BandLabel(net._band), 4);
	cells[4] = utils::PadRight(
		net._widthMhz > 0 ? std::to_string(net._widthMhz) : "-", 3
	);
	cells[5] = utils::PadRight(wifi::StandardLabel(net._standard), 3);
	cells[6] = utils::PadRight(wifi::SecurityLabel(net._security), 7);
	cells[7] = utils::PadRight(
		net._maxRateMbps > 0 ? std::to_string(net._maxRateMbps) + "M" : "-", 7
	);
	cells[8] =
		utils::PadRight(std::to_string(net._signalDbm) + " dBm", 8);

	std::string row;
	for (size_t i = 0; i < cells.size(); ++i)
	{
		if (i > 0)
			row += kSeparator;
		row += cells[i];
	}
	return row;
}

std::string FormatHeader()
{
	std::string row;
	for (size_t i = 0; i < kColumns.size(); ++i)
	{
		if (i > 0)
			row += kSeparator;
		row += utils::PadRight(kColumns[i].header, kColumns[i].width);
	}
	return row;
}

} // namespace

int RunNonInteractive(wifi::IScanner& scanner)
{
	// Default stop_source: never fires. TriggerScan blocks for its full
	// timeout — fine here, there is no UI from which to cancel.
	std::stop_source stopSource;
	if (!scanner.TriggerScan(stopSource.get_token()))
	{
		std::fprintf(stderr, "Scan trigger failed: %s\n",
					 scanner.GetLastError().c_str());
		return EXIT_FAILURE;
	}

	auto networks = scanner.GetNetworks();

	std::ranges::sort(
		networks,
		[](const wifi::Network& a, const wifi::Network& b)
		{
			if (a._signalDbm != b._signalDbm)
				return a._signalDbm > b._signalDbm;
			return a._ssid < b._ssid;
		}
	);

	std::puts(FormatHeader().c_str());
	for (const auto& net : networks)
		std::puts(FormatRow(net).c_str());

	return EXIT_SUCCESS;
}

} // namespace ui
