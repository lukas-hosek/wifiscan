#pragma once
#include <cstdint>

// IEEE 802.11 Information Element identifiers and a few related constants.
// We define these locally because <linux/ieee80211.h> isn't installed on the
// target system; the values are stable across the standard.
namespace wifi::ie
{

constexpr uint8_t SSID            = 0;
constexpr uint8_t BSS_LOAD        = 11;
constexpr uint8_t HT_CAPABILITY   = 45;
constexpr uint8_t RSN             = 48;
constexpr uint8_t HT_OPERATION    = 61;
constexpr uint8_t VHT_CAPABILITY  = 191;
constexpr uint8_t VHT_OPERATION   = 192;
constexpr uint8_t VENDOR_SPECIFIC = 221;
constexpr uint8_t EXTENSION       = 255;

// Sub-IDs that appear as the first payload byte of an Extension (EID 255) IE.
constexpr uint8_t EXT_HE_CAPABILITY  = 35;
constexpr uint8_t EXT_HE_OPERATION   = 36;
constexpr uint8_t EXT_EHT_OPERATION  = 106;
constexpr uint8_t EXT_EHT_CAPABILITY = 108;

// Capability Information field, bit 4 — "Privacy" (any link-layer encryption).
constexpr uint16_t CAP_PRIVACY = 0x0010;

// OUIs used in cipher- and AKM-suite selectors.
constexpr uint8_t OUI_RSN[3]       = {0x00, 0x0F, 0xAC};
constexpr uint8_t OUI_MICROSOFT[3] = {0x00, 0x50, 0xF2};

} // namespace wifi::ie
