// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "tui/App.hpp"
#include "non_interactive/App.hpp"
#include "wifi/NL80211Scanner.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#ifdef WIFISCAN_GUI
#include "gui/App.hpp"
#endif

// Which front-end to run. Resolved from command-line flags, falling back to
// auto-detection when none is given.
enum class UiType
{
	Undecided,
	Gui,
	Tui,
	NonInteractive,
};

int main(int argc, char* argv[])
{
	try
	{
		UiType uiType = UiType::Undecided;
		std::string ifaceName;

		for (int i = 1; i < argc; ++i)
		{
			std::string_view arg = argv[i];
			if (arg == "--gui")
				uiType = UiType::Gui;
			else if (arg == "--tui")
				uiType = UiType::Tui;
			else if (arg == "--non-interactive")
				uiType = UiType::NonInteractive;
			else if (ifaceName.empty() && !arg.empty() && arg[0] != '-')
				ifaceName = std::string(arg);
		}

		// No UI flag given: auto-pick the GUI if the desktop supports it,
		// otherwise the terminal UI.
		if (uiType == UiType::Undecided)
		{
#ifdef WIFISCAN_GUI
			uiType = gui::IsGuiSupported() ? UiType::Gui : UiType::Tui;
#else
			uiType = UiType::Tui;
#endif
		}

		auto scanner = ifaceName.empty() ? std::make_unique<wifi::NL80211Scanner>()
										 : std::make_unique<wifi::NL80211Scanner>(ifaceName);

		switch (uiType)
		{
		case UiType::NonInteractive:
		{
			non_interactive::App app(*scanner);
			return app.Run();
		}
		case UiType::Gui:
		{
#ifdef WIFISCAN_GUI
			gui::App app(*scanner);
			app.Run();
			return EXIT_SUCCESS;
#else
			fprintf(stderr,
				"--gui not built; reconfigure with "
				"-DWIFISCAN_ENABLE_GUI=ON\n");
			return EXIT_FAILURE;
#endif
		}
		case UiType::Tui:
		case UiType::Undecided:
		default:
		{
			tui::App app(*scanner);
			app.Run();
			return EXIT_SUCCESS;
		}
		}
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
