#pragma once
#include "IScanner.hpp"
#include <stdexcept>

struct nl_sock;
struct nl_msg;
struct nl_cb;
struct sockaddr_nl;
struct nlmsgerr;

namespace wifi
{

class ScanError : public std::runtime_error
{
	using std::runtime_error::runtime_error;
};

class NL80211Scanner : public IScanner
{
public:
	// Auto-detects the first wireless interface via /sys/class/net/*/wireless
	NL80211Scanner();

	// Binds the scanner to a specific interface name (e.g. "wlp3s0")
	explicit NL80211Scanner(std::string ifaceName);

	~NL80211Scanner() override;

	NL80211Scanner(const NL80211Scanner&) = delete;
	NL80211Scanner& operator=(const NL80211Scanner&) = delete;

	[[nodiscard]] std::vector<Network> GetNetworks() override;
	[[nodiscard]] const std::string& GetInterface() const noexcept override
	{
		return _iface;
	}
	[[nodiscard]] const std::string& GetLastError() const noexcept override
	{
		return _lastError;
	}
	[[nodiscard]] ScanFetchState GetLastFetchState() const noexcept override
	{
		return _lastFetchState;
	}

private:
	void InitNl();
	void CleanupNl() noexcept;

	// Sends NL80211_CMD_TRIGGER_SCAN and blocks until the kernel reports scan
	// completion (or timeout / error). Returns true when a fresh scan is ready
	// to be read; returns false on hard errors (caller falls back to cached
	// data).
	bool TriggerScan();

	// Parses one BSS netlink message and appends the decoded Network to
	// _pendingScanResults. Called by the static trampoline
	// NlBssMessageCallback.
	int ProcessBssMessage(nl_msg* message);

	// Parses multicast scan-event messages; sets _scanDone or _scanAborted.
	// Called by the static trampoline NlScanEventCallback.
	int ProcessScanEvent(nl_msg* message);

	// Stores the error description from a failed netlink operation into
	// _lastError. Called by the static trampoline NlErrorCallback.
	void StoreNlError(nlmsgerr* error);

	// Static trampolines required by the libnl C API (void* is always this
	// instance). Each one casts to NL80211Scanner* and forwards to the typed
	// instance method above.
	static int NlBssMessageCallback(nl_msg* message, void* scannerInstance);
	static int NlDumpFinishedCallback(nl_msg* message, void* scannerInstance);
	static int NlErrorCallback(sockaddr_nl* sourceAddress, nlmsgerr* error,
							   void* scannerInstance);
	static int NlTriggerAckCallback(nl_msg* message, void* scannerInstance);
	static int NlTriggerErrorCallback(sockaddr_nl* sourceAddress,
									  nlmsgerr* error, void* scannerInstance);
	static int NlScanEventCallback(nl_msg* message, void* scannerInstance);

	// Wireless interface name (e.g. "wlp3s0")
	std::string _iface;

	// Kernel interface index corresponding to _iface
	int _ifindex{0};

	// Netlink socket used for all nl80211 communication
	nl_sock* _sock{nullptr};

	// Generic netlink family ID for nl80211, resolved once during InitNl()
	int _nl80211Id{-1};

	// Collects decoded Network entries during a single GetNetworks() call;
	// cleared at the start of each call and moved into the return value when
	// done
	std::vector<Network> _pendingScanResults;

	// Human-readable description of the most recent error, empty if none
	std::string _lastError;

	// Whether the latest results came from a fresh scan or cached kernel state.
	ScanFetchState _lastFetchState{ScanFetchState::Unknown};

	// Skip TriggerScan on the very first GetNetworks() call so the kernel's
	// cached BSS table is returned immediately without waiting for a full scan.
	bool _coldStart{true};

	// Transient flags used only during a single TriggerScan() call
	bool _scanDone{false};
	bool _scanAborted{false};
	bool _triggerAcked{false};
	int _triggerErrno{0};
};

// Enumerates wireless interface names by checking
// /sys/class/net/<name>/wireless
std::vector<std::string> FindWirelessInterfaces();

} // namespace wifi
