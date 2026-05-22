#include "NL80211Scanner.hpp"
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/attr.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <poll.h>
#include <cerrno>
#include <filesystem>
#include <format>
#include <chrono>
#include <cstring>

namespace wifi
{

std::vector<std::string> FindWirelessInterfaces()
{
	std::vector<std::string> interfaces;
	for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net"))
	{
		if (std::filesystem::exists(entry.path() / "wireless"))
			interfaces.push_back(entry.path().filename().string());
	}
	return interfaces;
}

NL80211Scanner::NL80211Scanner()
{
	auto interfaces = FindWirelessInterfaces();
	if (interfaces.empty())
		throw ScanError("No wireless interfaces found");
	_iface = interfaces.front();
	InitNl();
}

NL80211Scanner::NL80211Scanner(std::string ifaceName)
	: _iface(std::move(ifaceName))
{
	InitNl();
}

NL80211Scanner::~NL80211Scanner()
{
	CleanupNl();
}

void NL80211Scanner::InitNl()
{
	_sock = nl_socket_alloc();
	if (!_sock)
		throw ScanError("Failed to allocate netlink socket");

	if (genl_connect(_sock) < 0)
	{
		nl_socket_free(_sock);
		_sock = nullptr;
		throw ScanError("Failed to connect generic netlink socket");
	}

	_nl80211Id = genl_ctrl_resolve(_sock, "nl80211");
	if (_nl80211Id < 0)
	{
		CleanupNl();
		throw ScanError("nl80211 not found — is cfg80211 loaded?");
	}

	_ifindex = static_cast<int>(if_nametoindex(_iface.c_str()));
	if (_ifindex == 0)
	{
		CleanupNl();
		throw ScanError(std::format("Interface '{}' not found", _iface));
	}
}

void NL80211Scanner::CleanupNl() noexcept
{
	if (_sock)
	{
		nl_socket_free(_sock);
		_sock = nullptr;
	}
}

bool NL80211Scanner::TriggerScan()
{
	// Subscribe to the nl80211 "scan" multicast group on a dedicated socket so
	// we can receive the NL80211_CMD_NEW_SCAN_RESULTS event without interfering
	// with the command socket used for NL80211_CMD_TRIGGER_SCAN / GET_SCAN.
	nl_sock* mcSock = nl_socket_alloc();
	if (!mcSock)
		return false;

	nl_socket_disable_seq_check(mcSock);
	if (genl_connect(mcSock) < 0)
	{
		nl_socket_free(mcSock);
		return false;
	}

	int mcGroup = genl_ctrl_resolve_grp(mcSock, "nl80211", "scan");
	if (mcGroup < 0)
	{
		nl_socket_free(mcSock);
		return false;
	}

	nl_socket_add_membership(mcSock, mcGroup);
	nl_socket_set_nonblocking(mcSock);

	// Build the trigger-scan command. NLM_F_ACK ensures the kernel always sends
	// a reply (ACK on success, NLMSG_ERROR on failure) so nl_recvmsgs terminates.
	nl_msg* triggerMsg = nlmsg_alloc();
	if (!triggerMsg)
	{
		nl_socket_free(mcSock);
		return false;
	}

	genlmsg_put(triggerMsg, NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Id, 0,
	            NLM_F_ACK, NL80211_CMD_TRIGGER_SCAN, 0);
	nla_put_u32(triggerMsg, NL80211_ATTR_IFINDEX, static_cast<uint32_t>(_ifindex));

	// One empty (wildcard) SSID → kernel sends probe requests on every channel
	// (active scan). Without this the kernel only does a passive scan.
	nlattr* ssidNest = nla_nest_start(triggerMsg, NL80211_ATTR_SCAN_SSIDS);
	nla_put(triggerMsg, 1, 0, "");
	nla_nest_end(triggerMsg, ssidNest);

	if (nl_send_auto(_sock, triggerMsg) < 0)
	{
		nlmsg_free(triggerMsg);
		nl_socket_free(mcSock);
		return false;
	}
	nlmsg_free(triggerMsg);

	// Wait for the ACK (or NLMSG_ERROR) on the command socket.
	_triggerAcked = false;
	_triggerErrno = 0;
	nl_cb* ackCb = nl_cb_alloc(NL_CB_DEFAULT);
	if (ackCb)
	{
		nl_cb_set(ackCb, NL_CB_ACK,   NL_CB_CUSTOM, NlTriggerAckCallback,   this);
		nl_cb_err(ackCb, NL_CB_CUSTOM,               NlTriggerErrorCallback, this);
		nl_recvmsgs(_sock, ackCb);
		nl_cb_put(ackCb);
	}

	// Hard error (not EBUSY): give up — GET_SCAN will also fail and show the error.
	if (_triggerErrno != 0 && _triggerErrno != EBUSY)
	{
		nl_socket_free(mcSock);
		return false;
	}

	// Poll the multicast socket until the kernel signals scan completion.
	_scanDone    = false;
	_scanAborted = false;
	nl_cb* mcCb = nl_cb_alloc(NL_CB_DEFAULT);
	if (mcCb)
	{
		nl_cb_set(mcCb, NL_CB_VALID, NL_CB_CUSTOM, NlScanEventCallback, this);

		int fd = nl_socket_get_fd(mcSock);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);

		while (!_scanDone && !_scanAborted)
		{
			auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
			    deadline - std::chrono::steady_clock::now()).count();
			if (remaining <= 0)
				break;

			pollfd pfd{fd, POLLIN, 0};
			if (poll(&pfd, 1, static_cast<int>(std::min(remaining, 500L))) > 0)
				nl_recvmsgs(mcSock, mcCb);
		}

		nl_cb_put(mcCb);
	}

	nl_socket_free(mcSock);
	return _scanDone;
}

std::vector<Network> NL80211Scanner::GetNetworks()
{
	_pendingScanResults.clear();
	_lastError.clear();

	if (_coldStart)
		_coldStart = false;
	else
		TriggerScan();

	nl_msg* message = nlmsg_alloc();
	if (!message)
	{
		_lastError = "Failed to allocate netlink message";
		return {};
	}

	genlmsg_put(message, NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Id, 0,
	            NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);
	nla_put_u32(message, NL80211_ATTR_IFINDEX, static_cast<uint32_t>(_ifindex));

	nl_cb* callback = nl_cb_alloc(NL_CB_DEFAULT);
	if (!callback)
	{
		nlmsg_free(message);
		_lastError = "Failed to allocate netlink callback";
		return {};
	}

	nl_cb_set(callback, NL_CB_VALID,  NL_CB_CUSTOM, NlBssMessageCallback,   this);
	nl_cb_set(callback, NL_CB_FINISH, NL_CB_CUSTOM, NlDumpFinishedCallback, this);
	nl_cb_err(callback, NL_CB_CUSTOM, NlErrorCallback, this);

	nl_send_auto(_sock, message);
	nl_recvmsgs(_sock, callback);

	nlmsg_free(message);
	nl_cb_put(callback);

	return std::move(_pendingScanResults);
}

int NL80211Scanner::ProcessBssMessage(nl_msg* message)
{
	struct nlmsghdr* header = nlmsg_hdr(message);
	struct genlmsghdr* genlHeader = static_cast<genlmsghdr*>(nlmsg_data(header));

	struct nlattr* topLevelAttrs[NL80211_ATTR_MAX + 1] = {};
	nla_parse(topLevelAttrs, NL80211_ATTR_MAX,
	          genlmsg_attrdata(genlHeader, 0), genlmsg_attrlen(genlHeader, 0), nullptr);

	if (!topLevelAttrs[NL80211_ATTR_BSS])
		return NL_SKIP;

	struct nlattr* bssAttrs[NL80211_BSS_MAX + 1] = {};
	nla_parse_nested(bssAttrs, NL80211_BSS_MAX, topLevelAttrs[NL80211_ATTR_BSS], nullptr);

	Network network{};

	if (bssAttrs[NL80211_BSS_SIGNAL_MBM])
		network._signalDbm = nla_get_s32(bssAttrs[NL80211_BSS_SIGNAL_MBM]) / 100;

	if (bssAttrs[NL80211_BSS_FREQUENCY])
		network._frequency = nla_get_u32(bssAttrs[NL80211_BSS_FREQUENCY]);

	if (bssAttrs[NL80211_BSS_BSSID])
	{
		auto* macBytes = static_cast<uint8_t*>(nla_data(bssAttrs[NL80211_BSS_BSSID]));
		network._bssid = std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
		    macBytes[0], macBytes[1], macBytes[2],
		    macBytes[3], macBytes[4], macBytes[5]);
	}

	// SSID lives in the Information Elements blob, not as a direct BSS attribute.
	// IE format: repeated [elementType:u8][length:u8][data:length bytes].
	// Element type 0 is the SSID; length 0 means hidden network.
	if (bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS])
	{
		auto* ieData = static_cast<uint8_t*>(nla_data(bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS]));
		int ieLength = nla_len(bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS]);
		int position = 0;
		while (position + 2 <= ieLength)
		{
			uint8_t elementType = ieData[position];
			uint8_t elementLength = ieData[position + 1];
			if (elementType == 0)
			{
				if (elementLength == 0)
					network._ssid = "<hidden>";
				else
					network._ssid.assign(reinterpret_cast<char*>(&ieData[position + 2]), elementLength);
				break;
			}
			position += 2 + elementLength;
		}
	}

	if (bssAttrs[NL80211_BSS_STATUS])
	{
		uint32_t status = nla_get_u32(bssAttrs[NL80211_BSS_STATUS]);
		network._connected = (status == NL80211_BSS_STATUS_ASSOCIATED);
	}

	network._channel = FreqToChannel(network._frequency);
	network._band = FreqToBand(network._frequency);

	_pendingScanResults.push_back(network);
	return NL_OK;
}

void NL80211Scanner::StoreNlError(nlmsgerr* error)
{
	_lastError = std::format("Netlink error: {} ({})",
	    strerror(-error->error), -error->error);
}

int NL80211Scanner::NlBssMessageCallback(nl_msg* message, void* scannerInstance)
{
	return static_cast<NL80211Scanner*>(scannerInstance)->ProcessBssMessage(message);
}

int NL80211Scanner::NlDumpFinishedCallback(nl_msg* /*message*/, void* /*scannerInstance*/)
{
	return NL_STOP;
}

int NL80211Scanner::NlErrorCallback(sockaddr_nl* /*sourceAddress*/, nlmsgerr* error, void* scannerInstance)
{
	static_cast<NL80211Scanner*>(scannerInstance)->StoreNlError(error);
	return NL_STOP;
}

int NL80211Scanner::ProcessScanEvent(nl_msg* message)
{
	auto* genlHeader = static_cast<genlmsghdr*>(nlmsg_data(nlmsg_hdr(message)));
	if (genlHeader->cmd == NL80211_CMD_NEW_SCAN_RESULTS)
	{
		_scanDone = true;
		return NL_STOP;
	}
	if (genlHeader->cmd == NL80211_CMD_SCAN_ABORTED)
	{
		_scanAborted = true;
		return NL_STOP;
	}
	return NL_SKIP;
}

int NL80211Scanner::NlTriggerAckCallback(nl_msg* /*message*/, void* scannerInstance)
{
	static_cast<NL80211Scanner*>(scannerInstance)->_triggerAcked = true;
	return NL_STOP;
}

int NL80211Scanner::NlTriggerErrorCallback(sockaddr_nl* /*sourceAddress*/, nlmsgerr* error, void* scannerInstance)
{
	static_cast<NL80211Scanner*>(scannerInstance)->_triggerErrno = -error->error;
	return NL_STOP;
}

int NL80211Scanner::NlScanEventCallback(nl_msg* message, void* scannerInstance)
{
	return static_cast<NL80211Scanner*>(scannerInstance)->ProcessScanEvent(message);
}

} // namespace wifi
