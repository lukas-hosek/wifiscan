// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "NetworkTablePanel.hpp"
#include "Fonts.hpp"
#include "ImGuiExtras.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

namespace gui
{

namespace
{

using ColumnType = NetworkTablePanel::ColumnType;
using theme::UiColor;

struct ColumnDescriptor
{
	ColumnType type;
	const char* header;
	// Content width in px at 1x scale; 0 marks the SSID column, which stretches
	float width;
	bool sortable;
	bool rightAligned;
};

constexpr std::array<ColumnDescriptor, 9> kColumns = {{
	{ColumnType::SSID, "SSID", 0.0f, true, false},
	{ColumnType::BSSID, "BSSID", 150.0f, false, false},
	{ColumnType::Channel, "CH", 40.0f, true, false},
	{ColumnType::Width, "WIDTH", 64.0f, false, false},
	{ColumnType::Band, "BAND", 52.0f, false, false},
	{ColumnType::Standard, "STD", 44.0f, false, false},
	{ColumnType::Security, "SECURITY", 64.0f, false, false},
	{ColumnType::Rate, "RATE", 76.0f, false, true},
	{ColumnType::Signal, "SIGNAL", 176.0f, true, false},
}};

// Layout in px at 1x scale
// The SSID column stretches, but below this width other columns start to drop
constexpr float kSsidMinWidth = 160.0f;
// Cells are padded by half of this on each side, which leaves this gap between columns
constexpr float kColumnGap = 12.0f;
constexpr float kRowHeight = 26.0f;
// The "connected" dot before the SSID, and the gap after it. The slot is reserved on every row so names align.
constexpr float kConnectedDotSize = 8.0f;
constexpr float kConnectedDotGap = 8.0f;
// Signal column: meter thickness, and the gap between the meter and the dBm text
constexpr float kMeterHeight = 6.0f;
constexpr float kMeterGap = 8.0f;
// Sort arrow triangle
constexpr float kSortArrowWidth = 7.0f;
constexpr float kSortArrowHeight = 4.0f;

// First entry vanishes first as the panel narrows. Types not listed are permanent.
constexpr std::array<ColumnType, 3> kDropOrder = {
	ColumnType::Rate,
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

// availWidth is in screen px; scale converts the 1x design widths to match.
VisibilityMap ComputeVisibility(float availWidth, float scale)
{
	VisibilityMap visible;
	visible.fill(true);

	auto requiredWidth = [&]
	{
		float width = 0.0f;
		for (size_t i = 0; i < kColumns.size(); ++i)
		{
			if (visible[i])
				width += (kColumns[i].width > 0.0f ? kColumns[i].width : kSsidMinWidth) + kColumnGap;
		}
		return width * scale;
	};

	for (ColumnType type : kDropOrder)
	{
		if (requiredWidth() <= availWidth)
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
	case ColumnType::Signal:
		break;
	}
	// Default / Signal: by signal, ties broken by SSID ascending.
	if (a._signalDbm != b._signalDbm)
		return order(a._signalDbm, b._signalDbm);
	return a._ssid < b._ssid;
}

void DrawText(ImDrawList* draw, ImVec2 pos, UiColor color, std::string_view text)
{
	draw->AddText(pos, theme::Color(color), text.data(), text.data() + text.size());
}

// The header and body tables must set up identical columns, or their cells won't line up.
void SetupColumns(const VisibilityMap& visible, float scale)
{
	for (size_t i = 0; i < kColumns.size(); ++i)
	{
		if (!visible[i])
			continue;
		const ColumnDescriptor& column = kColumns[i];
		bool stretches = column.width <= 0.0f;
		ImGui::TableSetupColumn(column.header,
			stretches ? ImGuiTableColumnFlags_WidthStretch : ImGuiTableColumnFlags_WidthFixed,
			stretches ? 0.0f : column.width * scale, static_cast<ImGuiID>(column.type));
	}
}

} // namespace

std::vector<wifi::Network> NetworkTablePanel::SortedNetworks(const std::vector<wifi::Network>& networks) const
{
	std::vector<wifi::Network> sorted = networks;
	std::stable_sort(sorted.begin(), sorted.end(),
		[this](const wifi::Network& a, const wifi::Network& b)
		{
			return CompareNetworks(a, b, _sortColumn, _sortAscending);
		});
	return sorted;
}

bool NetworkTablePanel::HandleKeyboard(const std::vector<wifi::Network>& sorted, std::string& selectedBssid)
{
	int step = 0;
	if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true))
		step = 1;
	if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))
		step = -1;
	if (step == 0 || sorted.empty())
		return false;

	auto current = std::find_if(sorted.begin(), sorted.end(),
		[&](const wifi::Network& net)
		{
			return net._bssid == selectedBssid;
		});
	int lastRow = static_cast<int>(sorted.size()) - 1;
	// With nothing selected (or the selected AP gone from the scan) either key starts at the top row.
	int row = current == sorted.end() ? 0 : std::clamp(static_cast<int>(current - sorted.begin()) + step, 0, lastRow);

	selectedBssid = sorted[static_cast<size_t>(row)]._bssid;
	return true;
}

void NetworkTablePanel::Render(const std::vector<wifi::Network>& networks, std::string& selectedBssid)
{
	std::vector<wifi::Network> sorted = SortedNetworks(networks);

	bool keyboardMoved = HandleKeyboard(sorted, selectedBssid);
	bool revealSelection = keyboardMoved || selectedBssid != _lastSeenSelection;

	if (sorted.empty())
	{
		ImGui::TextColored(
			ImGui::ColorConvertU32ToFloat4(theme::Color(UiColor::Muted)), "  (no networks - try: sudo wifiscan)");
		_lastSeenSelection = selectedBssid;
		return;
	}

	float scale = DpiScale();
	float rowHeight = kRowHeight * scale;
	VisibilityMap visible =
		ComputeVisibility(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize, scale);
	int visibleCount = static_cast<int>(std::count(visible.begin(), visible.end(), true));

	// No vertical cell padding: rows take their height from rowHeight and every cell centres its own content,
	// which lets the row Selectable cover the full row.
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(kColumnGap * 0.5f * scale, 0.0f));

	constexpr ImGuiTableFlags kHeaderFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
	constexpr ImGuiTableFlags kBodyFlags = kHeaderFlags | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg;

	// Returns the current cell's box; call it before submitting anything into the cell.
	auto currentCell = [rowHeight]
	{
		ImVec2 min = ImGui::GetCursorScreenPos();
		return CellBox{min, ImVec2(min.x + ImGui::GetContentRegionAvail().x, min.y + rowHeight)};
	};

	// The header is a table of its own above the scrolling body, so the body's scrollbar starts below the header
	// instead of running alongside it. The body is laid out first, one row down: the header must match the body's
	// inner width, which depends on whether the body shows a scrollbar.
	ImVec2 tableStart = ImGui::GetCursorPos();
	ImVec2 tableScreenStart = ImGui::GetCursorScreenPos();
	float tableWidth = ImGui::GetContentRegionAvail().x;
	float bodyInnerWidth = tableWidth;

	ImGui::SetCursorPos(ImVec2(tableStart.x, tableStart.y + rowHeight));
	if (ImGui::BeginTable("networks", visibleCount, kBodyFlags))
	{
		SetupColumns(visible, scale);
		// The current window is now the body's own scrolling child.
		if (ImGui::GetScrollMaxY() > 0.0f)
			bodyInnerWidth -= ImGui::GetStyle().ScrollbarSize;

		for (size_t row = 0; row < sorted.size(); ++row)
		{
			const wifi::Network& net = sorted[row];
			bool isSelected = !selectedBssid.empty() && net._bssid == selectedBssid;

			ImGui::PushID(static_cast<int>(row));
			ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
			int columnIndex = 0;
			for (size_t i = 0; i < kColumns.size(); ++i)
			{
				if (!visible[i])
					continue;
				ImGui::TableSetColumnIndex(columnIndex);
				CellBox cell = currentCell();
				if (columnIndex == 0)
					RenderRowSelectable(net, isSelected, revealSelection, rowHeight, selectedBssid);
				RenderCell(kColumns[i].type, net, cell);
				++columnIndex;
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	ImGui::SetCursorPos(tableStart);
	if (ImGui::BeginTable("header", visibleCount, kHeaderFlags, ImVec2(bodyInnerWidth, 0.0f)))
	{
		SetupColumns(visible, scale);
		ImGui::TableNextRow(ImGuiTableRowFlags_Headers, rowHeight);
		int columnIndex = 0;
		for (size_t i = 0; i < kColumns.size(); ++i)
		{
			if (!visible[i])
				continue;
			ImGui::TableSetColumnIndex(columnIndex);
			const ColumnDescriptor& column = kColumns[i];
			RenderHeaderCell(column.type, column.header, column.sortable, column.rightAligned, currentCell());
			++columnIndex;
		}
		ImGui::EndTable();
	}
	DrawHeaderRule(tableScreenStart.x, tableScreenStart.x + tableWidth, tableScreenStart.y + rowHeight);

	ImGui::PopStyleVar();

	_lastSeenSelection = selectedBssid;
}

void NetworkTablePanel::RenderHeaderCell(
	ColumnType type, const char* header, bool sortable, bool rightAligned, const CellBox& cell)
{
	fonts::ScopedFont caption(fonts::FontStyle::Regular, fonts::kCaptionSize);

	bool isActive = type == _sortColumn;
	bool hovered = false;
	if (sortable)
	{
		ImGui::PushID(header);
		// InvisibleButton asserts on a zero size, which a squeezed column can reach.
		ImVec2 buttonSize(std::max(1.0f, cell.max.x - cell.min.x), std::max(1.0f, cell.max.y - cell.min.y));
		if (ImGui::InvisibleButton("##sort", buttonSize))
		{
			if (isActive)
				_sortAscending = !_sortAscending;
			else
			{
				_sortColumn = type;
				// Strongest signal first is the useful default; names and channels read top-down.
				_sortAscending = type != ColumnType::Signal;
			}
		}
		hovered = ImGui::IsItemHovered();
		ImGui::PopID();
	}

	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 textSize = ImGui::CalcTextSize(header);
	float textX = rightAligned ? cell.max.x - textSize.x : cell.min.x;
	DrawText(draw, ImVec2(textX, cell.CenteredY(textSize.y)),
		(isActive || hovered) ? UiColor::PrimaryText : UiColor::Muted, header);

	if (!isActive)
		return;

	// Sort direction arrow at the cell's right edge: down = descending, up = ascending
	float scale = DpiScale();
	float arrowW = kSortArrowWidth * scale;
	float arrowH = kSortArrowHeight * scale;
	float left = cell.max.x - arrowW;
	float top = cell.CenteredY(arrowH);
	ImU32 accent = theme::Color(UiColor::Accent);
	if (_sortAscending)
		draw->AddTriangleFilled(
			ImVec2(left + arrowW * 0.5f, top), ImVec2(left + arrowW, top + arrowH), ImVec2(left, top + arrowH), accent);
	else
		draw->AddTriangleFilled(
			ImVec2(left, top), ImVec2(left + arrowW, top), ImVec2(left + arrowW * 0.5f, top + arrowH), accent);
}

void NetworkTablePanel::DrawHeaderRule(float left, float right, float bottom)
{
	float thickness = DpiScale();
	float y = bottom - thickness * 0.5f;
	ImGui::GetWindowDrawList()->AddLine(ImVec2(left, y), ImVec2(right, y), theme::Color(UiColor::Border), thickness);
}

void NetworkTablePanel::RenderRowSelectable(
	const wifi::Network& net, bool isSelected, bool revealSelection, float rowHeight, std::string& selectedBssid)
{
	// A selected row keeps its accent fill while hovered instead of switching to the plain hover tint.
	if (isSelected)
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::Color(UiColor::SelectionBg));
	// Selectable pads its box by half the vertical item spacing; without spacing it covers exactly the row.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
	bool clicked = ImGui::Selectable("##row", isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, rowHeight));
	ImGui::PopStyleVar();
	if (isSelected)
		ImGui::PopStyleColor();

	if (clicked)
		selectedBssid = isSelected ? std::string() : net._bssid;

	if (!isSelected)
		return;

	if (revealSelection)
		ImGui::SetScrollHereY();

	// 1 px accent outline around the whole row. The current clip rect is this column's, so widen it to the row;
	// its vertical range is the body's, so a row scrolled up past the header stays hidden.
	ImDrawList* draw = ImGui::GetWindowDrawList();
	ImVec2 rowMin = ImGui::GetItemRectMin();
	ImVec2 rowMax = ImGui::GetItemRectMax();
	ImGui::PushClipRect(ImVec2(rowMin.x, draw->GetClipRectMin().y), ImVec2(rowMax.x, draw->GetClipRectMax().y), false);
	draw->AddRect(rowMin, rowMax, theme::Color(UiColor::Accent), 0.0f, 0, DpiScale());
	ImGui::PopClipRect();
}

void NetworkTablePanel::RenderCell(ColumnType type, const wifi::Network& net, const CellBox& cell)
{
	ImDrawList* draw = ImGui::GetWindowDrawList();
	float scale = DpiScale();
	float textY = cell.CenteredY(ImGui::GetFontSize());
	char buffer[32];

	switch (type)
	{
	case ColumnType::SSID:
	{
		float dotSize = kConnectedDotSize * scale;
		if (net._connected)
		{
			ImVec2 dotCenter(cell.min.x + dotSize * 0.5f, cell.min.y + (cell.max.y - cell.min.y) * 0.5f);
			draw->AddCircleFilled(dotCenter, dotSize * 0.5f, theme::Color(UiColor::ConnectedNetwork));
		}
		ImVec2 textPos(cell.min.x + dotSize + kConnectedDotGap * scale, textY);
		if (net._ssid.empty())
		{
			fonts::ScopedFont italic(fonts::FontStyle::Italic, fonts::kBodySize);
			DrawText(draw, textPos, UiColor::Muted, net.DisplaySsid());
		}
		else
		{
			DrawText(draw, textPos, net._connected ? UiColor::ConnectedNetwork : UiColor::PrimaryText, net._ssid);
		}
		break;
	}
	case ColumnType::BSSID:
		DrawText(draw, ImVec2(cell.min.x, textY), UiColor::Muted, net._bssid);
		break;
	case ColumnType::Channel:
	{
		fonts::ScopedFont semiBold(fonts::FontStyle::SemiBold, fonts::kBodySize);
		std::snprintf(buffer, sizeof(buffer), "%d", net._channel);
		DrawText(draw, ImVec2(cell.min.x, textY), UiColor::Accent, buffer);
		break;
	}
	case ColumnType::Width:
		if (net._widthMhz > 0)
		{
			std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(net._widthMhz));
			DrawText(draw, ImVec2(cell.min.x, textY), UiColor::DataValue, buffer);
			float unitX = cell.min.x + ImGui::CalcTextSize(buffer).x;
			DrawText(draw, ImVec2(unitX, textY), UiColor::Muted, " MHz");
		}
		else
		{
			DrawText(draw, ImVec2(cell.min.x, textY), UiColor::Muted, "-");
		}
		break;
	case ColumnType::Band:
	{
		std::string label = wifi::BandLabel(net._band);
		ImVec2 pillSize = PillSize(label.c_str());
		DrawPill(draw, ImVec2(cell.min.x, cell.CenteredY(pillSize.y)), label.c_str(), theme::Color(UiColor::Surface),
			IM_COL32_BLACK_TRANS, theme::Color(UiColor::DataValue));
		break;
	}
	case ColumnType::Standard:
		DrawText(draw, ImVec2(cell.min.x, textY), UiColor::DataValue, wifi::StandardLabel(net._standard));
		break;
	case ColumnType::Security:
	{
		// An Open network is the one worth noticing, so it turns amber.
		bool isOpen = net._security == wifi::Security::Open;
		ImU32 border =
			isOpen ? theme::WithAlpha(theme::Color(UiColor::Warning), 0.55f) : theme::Color(UiColor::BorderStrong);
		ImU32 textColor = theme::Color(isOpen ? UiColor::Warning : UiColor::DataValue);
		std::string label = wifi::SecurityLabel(net._security);
		ImVec2 pillSize = PillSize(label.c_str());
		DrawPill(draw, ImVec2(cell.min.x, cell.CenteredY(pillSize.y)), label.c_str(), IM_COL32_BLACK_TRANS, border,
			textColor);
		break;
	}
	case ColumnType::Rate:
	{
		if (net._maxRateMbps > 0)
			std::snprintf(buffer, sizeof(buffer), "%u M", static_cast<unsigned>(net._maxRateMbps));
		else
			std::snprintf(buffer, sizeof(buffer), "-");
		float textW = ImGui::CalcTextSize(buffer).x;
		DrawText(draw, ImVec2(cell.max.x - textW, textY), UiColor::DataValue, buffer);
		break;
	}
	case ColumnType::Signal:
	{
		// Quality meter on the left, dBm right-aligned in a slot wide enough for "-100 dBm"
		float dbmSlotW = ImGui::CalcTextSize("-100 dBm").x;
		float meterH = kMeterHeight * scale;
		float meterLeft = cell.min.x;
		float meterRight = std::max(meterLeft, cell.max.x - dbmSlotW - kMeterGap * scale);
		float meterTop = cell.CenteredY(meterH);
		float rounding = meterH * 0.5f;
		draw->AddRectFilled(ImVec2(meterLeft, meterTop), ImVec2(meterRight, meterTop + meterH),
			theme::Color(UiColor::MeterTrack), rounding);
		float fillRight = meterLeft + (meterRight - meterLeft) * static_cast<float>(net.SignalQuality()) / 100.0f;
		if (fillRight > meterLeft)
			draw->AddRectFilled(ImVec2(meterLeft, meterTop), ImVec2(fillRight, meterTop + meterH),
				theme::SignalColor(net._signalDbm), rounding);

		std::snprintf(buffer, sizeof(buffer), "%d dBm", static_cast<int>(net._signalDbm));
		float textW = ImGui::CalcTextSize(buffer).x;
		draw->AddText(ImVec2(cell.max.x - textW, textY), theme::SignalTextColor(net._signalDbm), buffer);
		break;
	}
	}
}

} // namespace gui
