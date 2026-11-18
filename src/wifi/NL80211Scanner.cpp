// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NL80211Scanner.hpp"
#include "ChannelPlan.hpp"
#include "IE.hpp"
#include "utils/TextUtil.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <netlink/attr.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <optional>
#include <poll.h>

namespace wifi
{

namespace
{

// IE (Information Element) payloads extracted from the raw NL80211_BSS_INFORMATION_ELEMENTS
// blob. Each IE in the blob has the form [type:u8][length:u8][payload…]; these pointers
// point directly into the netlink message buffer (non-owning) and are valid only for the
// lifetime of that buffer. A null pointer means the AP did not advertise that element.
struct ParsedIes
{
	// 802.11n HT Capabilities IE (id 45) — encodes MIMO streams, short GI, LDPC, etc.
	const uint8_t* ht{nullptr};
	uint8_t htLen{0};

	// 802.11n HT Operation IE (id 61) — primary channel + secondary-channel offset (HT40±).
	const uint8_t* htOp{nullptr};
	uint8_t htOpLen{0};

	// 802.11ac VHT Capabilities IE (id 191) — MCS map, max MPDU size, SU/MU-MIMO flags.
	const uint8_t* vht{nullptr};
	uint8_t vhtLen{0};

	// 802.11ac VHT Operation IE (id 192) — channel width (80/160 MHz) + centre frequencies.
	const uint8_t* vhtOp{nullptr};
	uint8_t vhtOpLen{0};

	// RSN (Robust Security Network) IE (id 48) — WPA2/WPA3 cipher suites + AKM list.
	const uint8_t* rsn{nullptr};
	uint16_t rsnLen{0};

	// BSS Load IE (id 11) — station count, channel utilisation (0–255), available admission capacity.
	const uint8_t* bssLoad{nullptr};
	uint8_t bssLoadLen{0};

	// WPA1 Vendor-Specific IE (id 221, Microsoft OUI DD-50-F2 type 01) — legacy WPA-TKIP signalling.
	const uint8_t* wpa1{nullptr};
	uint16_t wpa1Len{0};

	// 802.11ax HE Operation Extension IE (id 255, ext 36), payload after the ext ID — carries the 6 GHz operating
	// channel, since 6 GHz beacons have no HT/VHT Operation IEs.
	const uint8_t* heOp{nullptr};
	uint8_t heOpLen{0};

	// 802.11be EHT Operation Extension IE (id 255, ext 106), payload after the ext ID — can override the operating
	// channel, e.g. for 320 MHz.
	const uint8_t* ehtOp{nullptr};
	uint8_t ehtOpLen{0};

	// True if any 802.11ax HE Capabilities or HE Operation Extension IE was seen (id 255, ext 35/36).
	bool he{false};

	// True if any 802.11be EHT Capabilities or EHT Operation Extension IE was seen (id 255, ext 108/106).
	bool eht{false};
};

bool AttrHasLen(nlattr* attr, int minLength) { return attr && nla_len(attr) >= minLength; }

std::string LibnlFailure(const char* operation, int result) { return fmt::format("{} ({})", operation, result); }

// libnl applies its default sequence-number check against the socket's last
// outgoing sequence. Multicast events from the kernel carry unrelated
// sequence numbers, so the default check fails with NLE_SEQ_MISMATCH on the
// first event. nl_socket_disable_seq_check() only affects the socket's
// default callback; once we pass a custom nl_cb to nl_recvmsgs that override
// is bypassed, so the check must be disabled on the callback itself.
int NlSeqCheckPass(nl_msg* /*msg*/, void* /*arg*/) { return NL_OK; }

bool TryGetU32(nlattr* attr, uint32_t& out)
{
	if (!AttrHasLen(attr, static_cast<int>(sizeof(uint32_t))))
		return false;
	out = nla_get_u32(attr);
	return true;
}

bool TryGetS32(nlattr* attr, int32_t& out)
{
	if (!AttrHasLen(attr, static_cast<int>(sizeof(int32_t))))
		return false;
	out = nla_get_s32(attr);
	return true;
}

bool TryGetU16(nlattr* attr, uint16_t& out)
{
	if (!AttrHasLen(attr, static_cast<int>(sizeof(uint16_t))))
		return false;
	out = nla_get_u16(attr);
	return true;
}

ParsedIes ScanIes(const uint8_t* ieData, int ieLength, std::string& ssidOut)
{
	ParsedIes out;
	int position = 0;
	while (position + 2 <= ieLength)
	{
		uint8_t elementType = ieData[position];
		uint8_t elementLength = ieData[position + 1];
		if (position + 2 + elementLength > ieLength)
			break;
		const uint8_t* payload = &ieData[position + 2];

		switch (elementType)
		{
		case ie::SSID:
			if (elementLength == 0)
				ssidOut = "<hidden>";
			else
				ssidOut =
					utils::SanitizeForTerminal(std::string_view(reinterpret_cast<const char*>(payload), elementLength));
			break;

		case ie::BSS_LOAD:
			if (elementLength >= 5)
			{
				out.bssLoad = payload;
				out.bssLoadLen = elementLength;
			}
			break;

		case ie::HT_CAPABILITY:
			if (elementLength >= 26)
			{
				out.ht = payload;
				out.htLen = elementLength;
			}
			break;

		case ie::HT_OPERATION:
			if (elementLength >= 22)
			{
				out.htOp = payload;
				out.htOpLen = elementLength;
			}
			break;

		case ie::RSN:
			out.rsn = payload;
			out.rsnLen = elementLength;
			break;

		case ie::VHT_CAPABILITY:
			if (elementLength >= 12)
			{
				out.vht = payload;
				out.vhtLen = elementLength;
			}
			break;

		case ie::VHT_OPERATION:
			if (elementLength >= 5)
			{
				out.vhtOp = payload;
				out.vhtOpLen = elementLength;
			}
			break;

		case ie::VENDOR_SPECIFIC:
			// WPA1 vendor IE: Microsoft OUI + type 1 + version.
			if (elementLength >= 4 && std::memcmp(payload, ie::OUI_MICROSOFT, 3) == 0 && payload[3] == 0x01)
			{
				out.wpa1 = payload;
				out.wpa1Len = elementLength;
			}
			break;

		case ie::EXTENSION:
			if (elementLength >= 1)
			{
				uint8_t extId = payload[0];
				if (extId == ie::EXT_HE_CAPABILITY || extId == ie::EXT_HE_OPERATION)
					out.he = true;
				else if (extId == ie::EXT_EHT_CAPABILITY || extId == ie::EXT_EHT_OPERATION)
					out.eht = true;

				if (extId == ie::EXT_HE_OPERATION)
				{
					out.heOp = payload + 1;
					out.heOpLen = static_cast<uint8_t>(elementLength - 1);
				}
				else if (extId == ie::EXT_EHT_OPERATION)
				{
					out.ehtOp = payload + 1;
					out.ehtOpLen = static_cast<uint8_t>(elementLength - 1);
				}
			}
			break;

		default:
			break;
		}

		position += 2 + elementLength;
	}
	return out;
}

// A BSS's operating channel: its total bandwidth and the edges of the spectrum it occupies
struct OperatingChannel
{
	uint16_t widthMhz;
	uint32_t lowMhz;
	uint32_t highMhz;
};

OperatingChannel CentredOn(uint32_t centreMhz, uint16_t widthMhz)
{
	return OperatingChannel{widthMhz, centreMhz - widthMhz / 2U, centreMhz + widthMhz / 2U};
}

// From VHT onwards a channel is given as up to two centre frequency segments (CCFS, channel numbers). Up to 80 MHz,
// CCFS0 is the centre. For a wider channel CCFS0 is the centre of the primary half and CCFS1 the centre of the
// whole channel, except in two older encodings: VHT's deprecated 160 MHz code puts the centre in CCFS0 (CCFS1 = 0),
// and 80+80 puts the second, separate segment in CCFS1.
OperatingChannel FromSegments(uint16_t widthMhz, uint8_t ccfs0, uint8_t ccfs1, Band band)
{
	uint32_t ccfs0Mhz = ChannelToFreq(ccfs0, band);
	if (widthMhz <= 80 || ccfs1 == 0)
		return CentredOn(ccfs0Mhz, widthMhz);

	// The primary half's centre sits a quarter of the width (in 5 MHz channel numbers: width / 20) from the centre
	int halfOffset = widthMhz / 20;
	if (std::abs(static_cast<int>(ccfs1) - static_cast<int>(ccfs0)) == halfOffset)
		return CentredOn(ChannelToFreq(ccfs1, band), widthMhz);

	// 80+80: a range can't describe two separate segments, so cover only the one with the primary channel
	OperatingChannel primarySegment = CentredOn(ccfs0Mhz, 80);
	primarySegment.widthMhz = widthMhz;
	return primarySegment;
}

// EHT Operation: params(1), basic EHT-MCS/NSS set(4), then — when params bit 0 is set — the EHT Operation
// Information: control (bits 0-2 width), CCFS0, CCFS1.
std::optional<OperatingChannel> FromEhtOperation(const ParsedIes& parsed, Band band)
{
	constexpr size_t kInfoOffset = 5;
	if (parsed.ehtOp == nullptr || parsed.ehtOpLen < kInfoOffset + 3 || (parsed.ehtOp[0] & 0x01) == 0)
		return std::nullopt;

	constexpr std::array<uint16_t, 5> kWidths{20, 40, 80, 160, 320};
	const uint8_t* info = parsed.ehtOp + kInfoOffset;
	size_t widthCode = info[0] & 0x07;
	if (widthCode >= kWidths.size())
		return std::nullopt;
	return FromSegments(kWidths[widthCode], info[1], info[2], band);
}

// HE Operation: params(3, little-endian), BSS colour(1), basic HE-MCS/NSS set(2), then optional fields flagged in
// params: VHT Operation Information(3, bit 14), Max Co-Hosted BSSID Indicator(1, bit 15) and the 6 GHz Operation
// Information(5, bit 17): primary channel, control (bits 0-1 width), CCFS0, CCFS1, minimum rate.
std::optional<OperatingChannel> FromHe6GhzOperation(const ParsedIes& parsed)
{
	constexpr size_t kFixedLength = 6;
	if (parsed.heOp == nullptr || parsed.heOpLen < kFixedLength)
		return std::nullopt;

	uint32_t params = parsed.heOp[0] | (parsed.heOp[1] << 8) | (parsed.heOp[2] << 16);
	if ((params & (1U << 17)) == 0)
		return std::nullopt;

	size_t offset = kFixedLength;
	if (params & (1U << 14))
		offset += 3;
	if (params & (1U << 15))
		offset += 1;
	if (parsed.heOpLen < offset + 5)
		return std::nullopt;

	constexpr std::array<uint16_t, 4> kWidths{20, 40, 80, 160};
	const uint8_t* info = parsed.heOp + offset;
	return FromSegments(kWidths[info[1] & 0x03], info[2], info[3], Band::GHz6);
}

// VHT Operation: channel width code, CCFS0, CCFS1. Code 0 means 20/40 MHz, which the HT Operation IE describes.
std::optional<OperatingChannel> FromVhtOperation(const ParsedIes& parsed)
{
	if (parsed.vhtOp == nullptr || parsed.vhtOpLen < 3)
		return std::nullopt;

	uint8_t widthCode = parsed.vhtOp[0];
	uint8_t ccfs0 = parsed.vhtOp[1];
	uint8_t ccfs1 = parsed.vhtOp[2];
	switch (widthCode)
	{
	case 1:
		return FromSegments(ccfs1 == 0 ? 80 : 160, ccfs0, ccfs1, Band::GHz5);
	case 2: // deprecated 160 MHz encoding
	case 3: // deprecated 80+80 MHz encoding
		return FromSegments(160, ccfs0, ccfs1, Band::GHz5);
	default:
		return std::nullopt;
	}
}

// HT Operation: primary channel, then info byte 1 with the secondary channel offset (bits 0-1: 1 above, 3 below)
// and the STA channel width (bit 2: 40 MHz allowed). 40 MHz needs both.
std::optional<OperatingChannel> FromHtOperation(const ParsedIes& parsed, uint32_t primaryMhz)
{
	if (parsed.htOp == nullptr || parsed.htOpLen < 2)
		return std::nullopt;

	uint8_t info = parsed.htOp[1];
	if ((info & 0x04) == 0)
		return std::nullopt;
	switch (info & 0x03)
	{
	case 1:
		return CentredOn(primaryMhz + 10U, 40);
	case 3:
		return CentredOn(primaryMhz - 10U, 40);
	default:
		return std::nullopt;
	}
}

// The operating channel from the most specific Operation IE the BSS advertises; 20 MHz on the primary channel when
// there is none. EHT goes first because it is only included when it describes more than the older IEs do.
OperatingChannel DeriveOperatingChannel(const ParsedIes& parsed, Band band, uint32_t primaryMhz)
{
	std::optional<OperatingChannel> channel = FromEhtOperation(parsed, band);
	if (!channel && band == Band::GHz6)
		channel = FromHe6GhzOperation(parsed);
	if (!channel && band == Band::GHz5)
		channel = FromVhtOperation(parsed);
	if (!channel && (band == Band::GHz2_4 || band == Band::GHz5))
		channel = FromHtOperation(parsed, primaryMhz);

	// A malformed IE can describe a channel that doesn't even contain the primary one
	if (!channel || primaryMhz < channel->lowMhz || primaryMhz > channel->highMhz)
		return CentredOn(primaryMhz, 20);
	return *channel;
}

WifiStandard DeriveStandard(const ParsedIes& parsed, Band band)
{
	if (parsed.eht)
		return WifiStandard::BE;
	if (parsed.he)
		return WifiStandard::AX;
	if (parsed.vht)
		return WifiStandard::AC;
	if (parsed.ht || parsed.htOp)
		return WifiStandard::N;
	if (band == Band::GHz2_4)
		return WifiStandard::G;
	if (band == Band::GHz5 || band == Band::GHz6)
		return WifiStandard::A;
	return WifiStandard::Unknown;
}

Security DeriveSecurity(const ParsedIes& parsed, uint16_t capabilityBits)
{
	bool privacy = (capabilityBits & ie::CAP_PRIVACY) != 0;

	if (!parsed.rsn && !parsed.wpa1)
		return privacy ? Security::WEP : Security::Open;
	if (parsed.wpa1 && !parsed.rsn)
		return Security::WPA;

	// RSN IE layout (variable):
	//   u16 version
	//   u32 groupCipherSuite
	//   u16 pairwiseCipherCount, then count * u32
	//   u16 akmCount, then count * u32 (OUI[3] + type[1])
	const uint8_t* p = parsed.rsn;
	uint16_t len = parsed.rsnLen;
	if (len < 2 + 4 + 2)
		return Security::Unknown;

	size_t offset = 2 + 4;
	uint16_t pairwiseCount = static_cast<uint16_t>(p[offset] | (p[offset + 1] << 8));
	offset += 2;
	size_t pairwiseBytes = static_cast<size_t>(pairwiseCount) * 4U;
	if (offset + pairwiseBytes + 2 > len)
		return Security::Unknown;
	offset += pairwiseBytes;

	uint16_t akmCount = static_cast<uint16_t>(p[offset] | (p[offset + 1] << 8));
	offset += 2;
	size_t akmBytes = static_cast<size_t>(akmCount) * 4U;
	if (offset + akmBytes > len)
		return Security::Unknown;

	bool sae = false, psk = false, eap = false, owe = false;
	for (uint16_t i = 0; i < akmCount; i++)
	{
		const uint8_t* suite = p + offset + static_cast<size_t>(i) * 4U;
		if (std::memcmp(suite, ie::OUI_RSN, 3) != 0)
			continue;
		switch (suite[3])
		{
		case 1:
			eap = true;
			break;
		case 2:
			psk = true;
			break;
		case 8:
			sae = true;
			break;
		case 18:
			owe = true;
			break;
		default:
			break;
		}
	}

	if (sae && psk)
		return Security::WPA2_WPA3_Transition;
	if (sae)
		return Security::WPA3_SAE;
	if (owe)
		return Security::OWE;
	if (eap)
		return Security::WPA2_Enterprise;
	return Security::WPA2_Personal;
}

uint8_t DeriveSpatialStreams(const ParsedIes& parsed)
{
	uint8_t streams = 0;

	// VHT RX MCS Map: 2-bit field per stream × 8 streams; 0b11 (3) =
	// unsupported.
	if (parsed.vht && parsed.vhtLen >= 6)
	{
		uint16_t rxMcsMap = static_cast<uint16_t>(parsed.vht[4] | (parsed.vht[5] << 8));
		for (uint8_t i = 0; i < 8; i++)
		{
			uint8_t field = static_cast<uint8_t>((rxMcsMap >> (i * 2)) & 0x3);
			if (field != 3)
				streams = static_cast<uint8_t>(i + 1);
		}
		if (streams > 0)
			return streams;
	}

	// HT Supported MCS Set: 16 bytes starting at HT Cap offset 3.
	// Bytes 0..3 hold MCS 0-31 (one byte = one stream of 8 MCS).
	if (parsed.ht && parsed.htLen >= 3 + 16)
	{
		for (uint8_t i = 0; i < 4; i++)
			if (parsed.ht[3 + i] != 0)
				streams = static_cast<uint8_t>(i + 1);
	}

	return streams;
}

uint32_t DeriveMaxRate(WifiStandard standard, uint16_t widthMhz, uint8_t streams)
{
	if (streams == 0 || widthMhz == 0)
		return 0;

	// Rate per 1 spatial stream in Mbps, indexed by (standard, width).
	// 0 = invalid combination.
	auto perStream = [&]() -> uint32_t
	{
		switch (standard)
		{
		case WifiStandard::N:
			if (widthMhz == 20)
				return 72;
			if (widthMhz == 40)
				return 150;
			return 0;
		case WifiStandard::AC:
			if (widthMhz == 20)
				return 87;
			if (widthMhz == 40)
				return 200;
			if (widthMhz == 80)
				return 433;
			if (widthMhz == 160)
				return 867;
			return 0;
		case WifiStandard::AX:
			if (widthMhz == 20)
				return 143;
			if (widthMhz == 40)
				return 287;
			if (widthMhz == 80)
				return 600;
			if (widthMhz == 160)
				return 1201;
			return 0;
		case WifiStandard::BE:
			if (widthMhz == 20)
				return 172;
			if (widthMhz == 40)
				return 344;
			if (widthMhz == 80)
				return 721;
			if (widthMhz == 160)
				return 1441;
			if (widthMhz == 320)
				return 2882;
			return 0;
		default:
			return 0;
		}
	}();

	return perStream * streams;
}

} // namespace

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

NL80211Scanner::NL80211Scanner(std::string ifaceName) : _iface(std::move(ifaceName)) { InitNl(); }

NL80211Scanner::~NL80211Scanner() { CleanupNl(); }

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
		throw ScanError(fmt::format("Interface '{}' not found", _iface));
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

bool NL80211Scanner::TriggerScan(std::stop_token stopToken)
{
	_lastError.clear();
	bool triggerAccepted = false;

	// Subscribe to the nl80211 "scan" multicast group on a dedicated socket so
	// we can receive the NL80211_CMD_NEW_SCAN_RESULTS event without interfering
	// with the command socket used for NL80211_CMD_TRIGGER_SCAN / GET_SCAN.
	nl_sock* mcSock = nl_socket_alloc();
	if (!mcSock)
	{
		_lastError = "Failed to allocate multicast netlink socket";
		return false;
	}

	nl_socket_disable_seq_check(mcSock);
	if (genl_connect(mcSock) < 0)
	{
		_lastError = "Failed to connect multicast netlink socket";
		nl_socket_free(mcSock);
		return false;
	}

	int mcGroup = genl_ctrl_resolve_grp(mcSock, "nl80211", "scan");
	if (mcGroup < 0)
	{
		_lastError = LibnlFailure("Failed to resolve nl80211 scan multicast group", mcGroup);
		nl_socket_free(mcSock);
		return false;
	}

	int membershipResult = nl_socket_add_membership(mcSock, mcGroup);
	if (membershipResult < 0)
	{
		_lastError = LibnlFailure("Failed to subscribe to nl80211 scan events", membershipResult);
		nl_socket_free(mcSock);
		return false;
	}
	nl_socket_set_nonblocking(mcSock);

	// Build the trigger-scan command. NLM_F_ACK ensures the kernel always sends
	// a reply (ACK on success, NLMSG_ERROR on failure) so nl_recvmsgs
	// terminates.
	nl_msg* triggerMsg = nlmsg_alloc();
	if (!triggerMsg)
	{
		_lastError = "Failed to allocate trigger-scan netlink message";
		nl_socket_free(mcSock);
		return false;
	}

	if (!genlmsg_put(triggerMsg, NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Id, 0, NLM_F_ACK, NL80211_CMD_TRIGGER_SCAN, 0))
	{
		_lastError = "Failed to initialize trigger-scan message";
		nlmsg_free(triggerMsg);
		nl_socket_free(mcSock);
		return false;
	}

	int ifindexResult = nla_put_u32(triggerMsg, NL80211_ATTR_IFINDEX, static_cast<uint32_t>(_ifindex));
	if (ifindexResult < 0)
	{
		_lastError = LibnlFailure("Failed to encode trigger-scan interface index", ifindexResult);
		nlmsg_free(triggerMsg);
		nl_socket_free(mcSock);
		return false;
	}

	// One empty (wildcard) SSID → kernel sends probe requests on every channel
	// (active scan). Without this the kernel only does a passive scan.
	nlattr* ssidNest = nla_nest_start(triggerMsg, NL80211_ATTR_SCAN_SSIDS);
	if (!ssidNest)
	{
		_lastError = "Failed to start trigger-scan SSID list";
		nlmsg_free(triggerMsg);
		nl_socket_free(mcSock);
		return false;
	}

	int ssidResult = nla_put(triggerMsg, 1, 0, "");
	if (ssidResult < 0)
	{
		nla_nest_cancel(triggerMsg, ssidNest);
		_lastError = LibnlFailure("Failed to encode wildcard scan SSID", ssidResult);
		nlmsg_free(triggerMsg);
		nl_socket_free(mcSock);
		return false;
	}
	nla_nest_end(triggerMsg, ssidNest);

	int sendResult = nl_send_auto(_sock, triggerMsg);
	if (sendResult < 0)
	{
		_lastError = LibnlFailure("Failed to send trigger-scan command", sendResult);
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
		nl_cb_set(ackCb, NL_CB_ACK, NL_CB_CUSTOM, NlTriggerAckCallback, this);
		nl_cb_err(ackCb, NL_CB_CUSTOM, NlTriggerErrorCallback, this);
		int ackResult = nl_recvmsgs(_sock, ackCb);
		if (ackResult < 0 && _triggerErrno == 0)
			_lastError = LibnlFailure("Failed while waiting for trigger-scan acknowledgement", ackResult);
		nl_cb_put(ackCb);
	}
	else
	{
		_lastError = "Failed to allocate trigger-scan callback";
		nl_socket_free(mcSock);
		return false;
	}

	// Hard error (not EBUSY): give up — GET_SCAN will also fail and show the
	// error.
	if (_triggerErrno != 0 && _triggerErrno != EBUSY)
	{
		_lastError = fmt::format("Trigger scan failed ({})", _triggerErrno);
		nl_socket_free(mcSock);
		return false;
	}

	triggerAccepted = _triggerAcked || _triggerErrno == EBUSY;

	// Poll the multicast socket until the kernel signals scan completion.
	_scanDone = false;
	_scanAborted = false;
	nl_cb* mcCb = nl_cb_alloc(NL_CB_DEFAULT);
	if (mcCb)
	{
		nl_cb_set(mcCb, NL_CB_VALID, NL_CB_CUSTOM, NlScanEventCallback, this);
		nl_cb_set(mcCb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, NlSeqCheckPass, nullptr);

		int fd = nl_socket_get_fd(mcSock);
		auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);

		while (!_scanDone && !_scanAborted && !stopToken.stop_requested())
		{
			auto remaining =
				std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
					.count();
			if (remaining <= 0)
				break;

			pollfd pfd{fd, POLLIN, 0};
			int pollResult = poll(&pfd, 1, static_cast<int>(std::min(remaining, 500L)));
			if (pollResult < 0)
			{
				_lastError = fmt::format("Failed while waiting for scan events ({})", errno);
				break;
			}
			if (pollResult > 0)
			{
				int eventResult = nl_recvmsgs(mcSock, mcCb);
				if (eventResult == -NLE_AGAIN)
					continue;
				if (eventResult < 0)
				{
					_lastError = LibnlFailure("Failed while receiving scan events", eventResult);
					break;
				}
			}
		}

		nl_cb_put(mcCb);
	}
	else
	{
		_lastError = "Failed to allocate scan-event callback";
		nl_socket_free(mcSock);
		return false;
	}

	nl_socket_free(mcSock);
	if (_scanAborted && _lastError.empty())
		_lastError = "Kernel reported scan aborted";
	if (_scanAborted)
		return false;
	if (_scanDone)
		return true;
	if (!_lastError.empty())
		return false;

	// Some drivers update the cached BSS table without delivering an observable
	// multicast completion event to this socket. Treat an accepted trigger as a
	// successful refresh attempt instead of surfacing a false cached-result
	// error.
	return triggerAccepted;
}

std::vector<Network> NL80211Scanner::GetNetworks()
{
	_pendingScanResults.clear();
	_lastError.clear();

	nl_msg* message = nlmsg_alloc();
	if (!message)
	{
		_lastError = "Failed to allocate netlink message";
		return {};
	}

	// NL80211_CMD_GET_SCAN requires CAP_NET_ADMIN — running unprivileged
	// surfaces as EPERM from nl_recvmsgs below and is reported via _lastError.
	if (!genlmsg_put(message, NL_AUTO_PORT, NL_AUTO_SEQ, _nl80211Id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0))
	{
		nlmsg_free(message);
		_lastError = "Failed to initialize GET_SCAN message";
		return {};
	}

	int ifindexResult = nla_put_u32(message, NL80211_ATTR_IFINDEX, static_cast<uint32_t>(_ifindex));
	if (ifindexResult < 0)
	{
		nlmsg_free(message);
		_lastError = LibnlFailure("Failed to encode GET_SCAN interface index", ifindexResult);
		return {};
	}

	nl_cb* callback = nl_cb_alloc(NL_CB_DEFAULT);
	if (!callback)
	{
		nlmsg_free(message);
		_lastError = "Failed to allocate netlink callback";
		return {};
	}

	nl_cb_set(callback, NL_CB_VALID, NL_CB_CUSTOM, NlBssMessageCallback, this);
	nl_cb_set(callback, NL_CB_FINISH, NL_CB_CUSTOM, NlDumpFinishedCallback, this);
	nl_cb_err(callback, NL_CB_CUSTOM, NlErrorCallback, this);

	int sendResult = nl_send_auto(_sock, message);
	if (sendResult < 0)
	{
		nlmsg_free(message);
		nl_cb_put(callback);
		_lastError = LibnlFailure("Failed to send GET_SCAN request", sendResult);
		return {};
	}

	int recvResult = nl_recvmsgs(_sock, callback);
	if (recvResult < 0)
	{
		nlmsg_free(message);
		nl_cb_put(callback);
		if (_lastError.empty())
			_lastError = LibnlFailure("Failed to receive GET_SCAN results", recvResult);
		return {};
	}

	nlmsg_free(message);
	nl_cb_put(callback);

	return std::move(_pendingScanResults);
}

int NL80211Scanner::ProcessBssMessage(nl_msg* message)
{
	struct nlmsghdr* header = nlmsg_hdr(message);
	struct genlmsghdr* genlHeader = static_cast<genlmsghdr*>(nlmsg_data(header));

	struct nlattr* topLevelAttrs[NL80211_ATTR_MAX + 1] = {};
	nla_parse(
		topLevelAttrs, NL80211_ATTR_MAX, genlmsg_attrdata(genlHeader, 0), genlmsg_attrlen(genlHeader, 0), nullptr);

	if (!topLevelAttrs[NL80211_ATTR_BSS])
		return NL_SKIP;

	struct nlattr* bssAttrs[NL80211_BSS_MAX + 1] = {};
	nla_parse_nested(bssAttrs, NL80211_BSS_MAX, topLevelAttrs[NL80211_ATTR_BSS], nullptr);

	Network network{};

	// NL80211_BSS_SIGNAL_MBM is reported in mBm (1/100 dBm), not dBm — divide
	// by 100 to get the conventional dBm value.
	int32_t signalMbm = 0;
	if (TryGetS32(bssAttrs[NL80211_BSS_SIGNAL_MBM], signalMbm))
		network._signalDbm = signalMbm / 100;

	TryGetU32(bssAttrs[NL80211_BSS_FREQUENCY], network._frequency);

	uint16_t capabilityBits = 0;
	TryGetU16(bssAttrs[NL80211_BSS_CAPABILITY], capabilityBits);

	if (AttrHasLen(bssAttrs[NL80211_BSS_BSSID], 6))
	{
		auto* macBytes = static_cast<uint8_t*>(nla_data(bssAttrs[NL80211_BSS_BSSID]));
		network._bssid = fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", macBytes[0], macBytes[1], macBytes[2],
			macBytes[3], macBytes[4], macBytes[5]);
	}

	// SSID is NOT a direct BSS attribute — there is no NL80211_BSS_SSID. It
	// lives inside the NL80211_BSS_INFORMATION_ELEMENTS blob as IE element
	// type 0 (see ie::SSID in ScanIes). Length 0 → hidden network.
	//
	// IE blob layout: repeated [elementType:u8][length:u8][data:length bytes].
	// ScanIes walks every IE (no early-break) and pulls SSID, BSS Load and the
	// capability/operation IEs we use for width/standard/security/rate
	// derivation.
	ParsedIes parsed{};
	if (bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS] && nla_len(bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS]) > 0)
	{
		auto* ieData = static_cast<uint8_t*>(nla_data(bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS]));
		int ieLength = nla_len(bssAttrs[NL80211_BSS_INFORMATION_ELEMENTS]);
		parsed = ScanIes(ieData, ieLength, network._ssid);
	}

	uint32_t status = 0;
	if (TryGetU32(bssAttrs[NL80211_BSS_STATUS], status))
	{
		network._connected = (status == NL80211_BSS_STATUS_ASSOCIATED);
	}

	network._channel = FreqToChannel(network._frequency);
	network._band = FreqToBand(network._frequency);

	if (parsed.bssLoad && parsed.bssLoadLen >= 5)
	{
		network._stationCount = static_cast<int16_t>(parsed.bssLoad[0] | (parsed.bssLoad[1] << 8));
		network._channelUtilization = static_cast<int16_t>(parsed.bssLoad[2]);
	}

	if (network._frequency != 0)
	{
		OperatingChannel channel = DeriveOperatingChannel(parsed, network._band, network._frequency);
		network._widthMhz = channel.widthMhz;
		network._frequencyLow = channel.lowMhz;
		network._frequencyHigh = channel.highMhz;
		ChannelRange range = ChannelPlan(network._band)
								 .ChannelRangeWithin(channel.lowMhz, channel.highMhz)
								 .value_or(ChannelRange{network._channel, network._channel});
		network._channelLow = range.low;
		network._channelHigh = range.high;
	}
	network._standard = DeriveStandard(parsed, network._band);
	network._security = DeriveSecurity(parsed, capabilityBits);
	network._spatialStreams = DeriveSpatialStreams(parsed);
	network._maxRateMbps = DeriveMaxRate(network._standard, network._widthMhz, network._spatialStreams);

	_pendingScanResults.push_back(network);
	return NL_OK;
}

void NL80211Scanner::StoreNlError(nlmsgerr* error)
{
	_lastError = fmt::format("Netlink error: {} ({})", strerror(-error->error), -error->error);
}

int NL80211Scanner::NlBssMessageCallback(nl_msg* message, void* scannerInstance)
{
	return static_cast<NL80211Scanner*>(scannerInstance)->ProcessBssMessage(message);
}

int NL80211Scanner::NlDumpFinishedCallback(nl_msg* /*message*/, void* /*scannerInstance*/) { return NL_STOP; }

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
