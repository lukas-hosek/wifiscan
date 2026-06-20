// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NetworkTablePanel.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace gui
{

namespace
{

// Mirrors tui::NetworkTablePanel::ColumnType (src/tui/NetworkTablePanel.cpp).
enum class ColumnType
{
	SSID,
	BSSID,
	Channel,
	Band,
	Width,
	Standard,
	Security,
	Rate,
	Signal,
	Quality
};

struct ColumnDescriptor
{
	ColumnType type;
	const char* header;
	// Content width in terminal-character units. Kept identical to the FTXUI
	// table so the drop algorithm below makes the same decisions; converted to
	// pixels at render time via the live font glyph advance.
	int width;
};

// Display order + widths mirror tui::NetworkTablePanel::kColumns, except QUAL is
// widened 2x (10 -> 20) for the GUI's progress bar.
constexpr std::array<ColumnDescriptor, 10> kColumns = {{
	{ColumnType::SSID, "SSID", 28},
	{ColumnType::BSSID, "BSSID", 17},
	{ColumnType::Channel, "CH", 4},
	{ColumnType::Band, "BAND", 4},
	{ColumnType::Width, "W", 3},
	{ColumnType::Standard, "STD", 3},
	{ColumnType::Security, "SEC", 7},
	{ColumnType::Rate, "RATE", 7},
	{ColumnType::Signal, "SIGNAL", 8},
	{ColumnType::Quality, "QUAL", 20},
}};

// Drop order mirrors tui::NetworkTablePanel::kDropOrder: first entry vanishes
// first as the panel narrows. Types not listed are permanent.
constexpr std::array<ColumnType, 4> kDropOrder = {
	ColumnType::Rate,
	ColumnType::Quality,
	ColumnType::BSSID,
	ColumnType::Security,
};

using VisibilityMap = std::array<bool, kColumns.size()>;

int FindColumnIndex(ColumnType type)
{
	for (size_t i = 0; i < kColumns.size(); ++i)
	{
		if (kColumns[i].type == type)
			return static_cast<int>(i);
	}
	return -1;
}

// Mirrors tui::NetworkTablePanel::ComputeVisibility. availColumns is the panel
// width expressed in terminal-character units. The " | " separator between
// visible columns counts as 3 units, exactly as in the FTXUI implementation.
VisibilityMap ComputeVisibility(int availColumns)
{
	VisibilityMap visible;
	visible.fill(true);

	auto requiredWidth = [&]
	{
		int w = 0;
		bool first = true;
		for (size_t i = 0; i < kColumns.size(); ++i)
		{
			if (!visible[i])
				continue;
			if (!first)
				w += 3; // " | "
			w += kColumns[i].width;
			first = false;
		}
		return w;
	};

	for (ColumnType type : kDropOrder)
	{
		if (requiredWidth() <= availColumns)
			break;
		int idx = FindColumnIndex(type);
		if (idx >= 0)
			visible[static_cast<size_t>(idx)] = false;
	}
	return visible;
}

bool CompareNetworks(const wifi::Network& a, const wifi::Network& b, ColumnType type, bool ascending)
{
	auto order = [ascending](auto lhs, auto rhs)
	{
		return ascending ? lhs < rhs : lhs > rhs;
	};

	switch (type)
	{
	case ColumnType::SSID:
		return order(a._ssid, b._ssid);
	case ColumnType::BSSID:
		return order(a._bssid, b._bssid);
	case ColumnType::Channel:
		if (a._band != b._band)
			return order(a._band, b._band);
		return order(a._channel, b._channel);
	case ColumnType::Band:
		return order(a._band, b._band);
	case ColumnType::Width:
		return order(a._widthMhz, b._widthMhz);
	case ColumnType::Standard:
		return order(a._standard, b._standard);
	case ColumnType::Security:
		return order(a._security, b._security);
	case ColumnType::Rate:
		return order(a._maxRateMbps, b._maxRateMbps);
	case ColumnType::Quality:
		return order(a.SignalQuality(), b.SignalQuality());
	case ColumnType::Signal:
		break;
	}
	// Default / Signal: by signal, ties broken by SSID ascending.
	if (a._signalDbm != b._signalDbm)
		return order(a._signalDbm, b._signalDbm);
	return a._ssid < b._ssid;
}

ImVec4 ToVec(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

void RenderCell(ColumnType type, const wifi::Network& net)
{
	switch (type)
	{
	case ColumnType::BSSID:
		ImGui::TextColored(ToVec(theme::Color(theme::UiColor::Muted)), "%s", net._bssid.c_str());
		break;
	case ColumnType::Channel:
		ImGui::TextColored(ToVec(theme::Color(theme::UiColor::Accent)), "%d", net._channel);
		break;
	case ColumnType::Band:
		ImGui::TextColored(ToVec(theme::Color(theme::UiColor::DataValue)), "%s", wifi::BandLabel(net._band).c_str());
		break;
	case ColumnType::Width:
		if (net._widthMhz > 0)
			ImGui::TextColored(ToVec(theme::Color(theme::UiColor::DataValue)), "%u", net._widthMhz);
		else
			ImGui::TextColored(ToVec(theme::Color(theme::UiColor::DataValue)), "-");
		break;
	case ColumnType::Standard:
		ImGui::TextColored(
			ToVec(theme::Color(theme::UiColor::DataValue)), "%s", wifi::StandardLabel(net._standard).c_str());
		break;
	case ColumnType::Security:
		ImGui::TextColored(
			ToVec(theme::Color(theme::UiColor::DataValue)), "%s", wifi::SecurityLabel(net._security).c_str());
		break;
	case ColumnType::Rate:
		if (net._maxRateMbps > 0)
			ImGui::TextColored(ToVec(theme::Color(theme::UiColor::DataValue)), "%uM", net._maxRateMbps);
		else
			ImGui::TextColored(ToVec(theme::Color(theme::UiColor::DataValue)), "-");
		break;
	case ColumnType::Signal:
		ImGui::TextColored(ToVec(theme::SignalColor(net._signalDbm)), "%d dBm", net._signalDbm);
		break;
	case ColumnType::Quality:
	{
		int quality = net.SignalQuality();
		char label[16];
		std::snprintf(label, sizeof(label), "%d%%", quality);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ToVec(theme::SignalColor(net._signalDbm)));
		ImGui::ProgressBar(static_cast<float>(quality) / 100.0f, ImVec2(-FLT_MIN, 0.0f), label);
		ImGui::PopStyleColor();
		break;
	}
	case ColumnType::SSID:
		break; // handled inline (selectable)
	}
}

} // namespace

void NetworkTablePanel::Render(const std::vector<wifi::Network>& networks)
{
	int rowCount = static_cast<int>(networks.size());

	// Up/Down navigate the selection (global shortcut).
	bool selectionChanged = false;
	if (rowCount > 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
		{
			_selectedRow++;
			selectionChanged = true;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
		{
			_selectedRow--;
			selectionChanged = true;
		}
	}
	_selectedRow = std::clamp(_selectedRow, 0, std::max(0, rowCount - 1));

	if (networks.empty())
	{
		ImGui::TextColored(ToVec(theme::Color(theme::UiColor::Muted)), "  (no networks - try: sudo wifiscan)");
		return;
	}

	float charW = ImGui::CalcTextSize("0").x;
	int availColumns = static_cast<int>(ImGui::GetContentRegionAvail().x / std::max(1.0f, charW));
	VisibilityMap visible = ComputeVisibility(availColumns);

	std::vector<const ColumnDescriptor*> visCols;
	for (size_t i = 0; i < kColumns.size(); ++i)
	{
		if (visible[i])
			visCols.push_back(&kColumns[i]);
	}

	constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Sortable;

	if (!ImGui::BeginTable("networks", static_cast<int>(visCols.size()), kFlags))
		return;

	const ImGuiStyle& style = ImGui::GetStyle();
	float padX = style.CellPadding.x * 2.0f;
	float arrowW = ImGui::GetFontSize() + style.ItemInnerSpacing.x;

	// Sortable columns mirror tui::NetworkTablePanel::kSortableColumns. Only
	// these get a sort arrow, so the narrow columns keep room for their header
	// text.
	auto isSortable = [](ColumnType type)
	{
		return type == ColumnType::SSID || type == ColumnType::Channel || type == ColumnType::Signal;
	};

	for (const ColumnDescriptor* col : visCols)
	{
		bool sortable = isSortable(col->type);
		ImGuiTableColumnFlags colFlags = ImGuiTableColumnFlags_WidthFixed;
		if (!sortable)
			colFlags |= ImGuiTableColumnFlags_NoSort;
		if (col->type == ColumnType::Signal)
			colFlags |= ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending;

		// Width must fit the header text (proportional font) and cell padding,
		// plus the sort arrow on sortable columns, otherwise ImGui clips the
		// header to "...".
		float contentW = std::max(col->width * charW, ImGui::CalcTextSize(col->header).x);
		float colW = contentW + padX + (sortable ? arrowW : 0.0f);

		ImGui::TableSetupColumn(col->header, colFlags, colW, static_cast<ImGuiID>(col->type));
	}
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	std::vector<wifi::Network> sorted = networks;
	if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs())
	{
		if (specs->SpecsCount > 0)
		{
			const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
			ColumnType type = static_cast<ColumnType>(spec.ColumnUserID);
			bool ascending = spec.SortDirection == ImGuiSortDirection_Ascending;
			std::stable_sort(sorted.begin(), sorted.end(),
				[&](const wifi::Network& a, const wifi::Network& b)
				{
					return CompareNetworks(a, b, type, ascending);
				});
		}
		specs->SpecsDirty = false;
	}

	for (int row = 0; row < rowCount; ++row)
	{
		const wifi::Network& net = sorted[static_cast<size_t>(row)];
		bool isSelected = row == _selectedRow;

		ImGui::PushID(row);
		ImGui::TableNextRow();

		for (size_t c = 0; c < visCols.size(); ++c)
		{
			ImGui::TableSetColumnIndex(static_cast<int>(c));
			const ColumnType type = visCols[c]->type;

			if (type == ColumnType::SSID)
			{
				std::string ssid = net._ssid.empty() ? "???" : net._ssid;
				std::string label = (net._connected ? "* " : "  ") + ssid;
				ImU32 textColor = net._connected ? theme::Color(theme::UiColor::ConnectedNetwork)
												 : theme::Color(theme::UiColor::NetworkRow);
				ImGui::PushStyleColor(ImGuiCol_Text, ToVec(textColor));
				if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
					_selectedRow = row;
				ImGui::PopStyleColor();
				if (isSelected && selectionChanged)
					ImGui::SetScrollHereY();
			}
			else
			{
				RenderCell(type, net);
			}
		}
		ImGui::PopID();
	}

	ImGui::EndTable();
}

} // namespace gui
