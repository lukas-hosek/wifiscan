// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "StatusBarPanel.hpp"
#include "Theme.hpp"
#include <imgui.h>

namespace gui
{

StatusBarPanel::StatusBarPanel(wifi::IScanner& scanner) : _scanner(scanner) {}

void StatusBarPanel::SetStatus(const std::string& msg) { _statusMsg = msg; }

void StatusBarPanel::Render()
{
	static constexpr const char* kHints = "[q] quit   [Tab] band   [Up/Down] table   [Left/Right] spectrum   "
										  "[click header] sort";

	auto toVec = [](ImU32 c)
	{
		return ImGui::ColorConvertU32ToFloat4(c);
	};

	ImGui::PushStyleColor(ImGuiCol_Text, toVec(theme::Color(theme::UiColor::StatusText)));
	ImGui::TextUnformatted(_statusMsg.c_str());
	ImGui::PopStyleColor();

	ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, toVec(theme::Color(theme::UiColor::DataValue)));
	ImGui::Text("  iface: %s", _scanner.GetInterface().c_str());
	ImGui::PopStyleColor();

	// Right-align the key hints.
	float hintWidth = ImGui::CalcTextSize(kHints).x;
	float avail = ImGui::GetContentRegionAvail().x;
	if (avail > hintWidth)
	{
		ImGui::SameLine(ImGui::GetCursorPosX() + avail - hintWidth);
		ImGui::PushStyleColor(ImGuiCol_Text, toVec(theme::Color(theme::UiColor::ShortcutHint)));
		ImGui::TextUnformatted(kHints);
		ImGui::PopStyleColor();
	}
}

} // namespace gui
