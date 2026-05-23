// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ui/App.hpp"
#include "wifi/NL80211Scanner.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char* argv[])
{
	try
	{
		auto scanner =
			(argc > 1)
				? std::make_unique<wifi::NL80211Scanner>(std::string(argv[1]))
				: std::make_unique<wifi::NL80211Scanner>();

		ui::App app(*scanner);
		app.Run();
		return EXIT_SUCCESS;
	}
	catch (const wifi::ScanError& error)
	{
		fprintf(stderr, "WiFi scan error: %s\n", error.what());
		fprintf(stderr, "Try: sudo ./build/wifiscan\n");
		return EXIT_FAILURE;
	}
	catch (const std::exception& error)
	{
		fprintf(stderr, "Fatal: %s\n", error.what());
		return EXIT_FAILURE;
	}
}
