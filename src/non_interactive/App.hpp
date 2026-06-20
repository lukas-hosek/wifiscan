// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "wifi/IScanner.hpp"

namespace non_interactive
{

class App
{
public:
	explicit App(wifi::IScanner& scanner);
	int Run();
private:
	wifi::IScanner& _scanner;
};

} // namespace non_interactive
