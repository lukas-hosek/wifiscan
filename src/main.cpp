// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ui/App.hpp"
#include "ui/NonInteractiveOutput.hpp"
#include "wifi/NL80211Scanner.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

int main(int argc, char* argv[])
{
	try
	{
		bool nonInteractive = false;
		std::string ifaceName;

		for (int i = 1; i < argc; ++i)
		{
			std::string_view arg = argv[i];
			if (arg == "--non-interactive")
				nonInteractive = true;
			else if (ifaceName.empty() && !arg.empty() && arg[0] != '-')
				ifaceName = std::string(arg);
		}

		auto scanner = ifaceName.empty()
						   ? std::make_unique<wifi::NL80211Scanner>()
						   : std::make_unique<wifi::NL80211Scanner>(ifaceName);

		if (nonInteractive)
			return ui::RunNonInteractive(*scanner);

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
