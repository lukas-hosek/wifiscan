// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#pragma once
#include "Network.hpp"
#include <stop_token>
#include <string>
#include <vector>

namespace wifi
{

class IScanner
{
public:
	virtual ~IScanner() = default;

	// Returns the most recently cached scan results from the kernel BSS table
	[[nodiscard]] virtual std::vector<Network> GetNetworks() = 0;

	// Asks the kernel to perform a fresh active scan and blocks until it
	// completes, is aborted, times out, or the stop token is cancelled.
	// Returns true if the kernel signalled a successful scan.
	virtual bool TriggerScan(std::stop_token stopToken) = 0;

	// Returns the wireless interface name this scanner is bound to (e.g.
	// "wlp3s0")
	[[nodiscard]] virtual const std::string& GetInterface() const noexcept = 0;

	// Returns a human-readable description of the last error, or empty string
	// if none
	[[nodiscard]] virtual const std::string& GetLastError() const noexcept = 0;
};

} // namespace wifi
