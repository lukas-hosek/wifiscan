// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "ChannelAxis.hpp"
#include "Fonts.hpp"
#include "Theme.hpp"
#include <algorithm>
#include <string>

namespace gui
{

float ChannelAxis::ClippedLeft(const wifi::SlotSpan& span) const { return std::max(0.0f, SlotX(span.lo)); }

float ChannelAxis::ClippedRight(const wifi::SlotSpan& span) const { return std::min(plotWidth, SlotX(span.hi)); }

void ChannelAxis::DrawTicks(ImDrawList* draw, ImVec2 origin, float top, float rowHeight) const
{
	for (size_t slot = 0; slot < channels.size(); ++slot)
	{
		int channel = channels[slot];
		bool isPrimary = primaryChannels.contains(channel);
		fonts::ScopedFont font(isPrimary ? fonts::FontStyle::SemiBold : fonts::FontStyle::Regular, fonts::kSmallSize);

		std::string label = std::to_string(channel);
		ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
		float centerX = origin.x + SlotX(static_cast<float>(slot) + 0.5f);
		ImU32 color = theme::Color(isPrimary ? theme::UiColor::Accent : theme::UiColor::Muted);
		draw->AddText(ImVec2(centerX - textSize.x * 0.5f, top + (rowHeight - textSize.y) * 0.5f), color, label.c_str());
	}
}

void ChannelAxis::ScrollIntoView(const wifi::SlotSpan& span) const
{
	float left = ClippedLeft(span);
	float right = ClippedRight(span);
	float visibleWidth =
		ImGui::GetWindowWidth() - (ImGui::GetScrollMaxY() > 0.0f ? ImGui::GetStyle().ScrollbarSize : 0.0f);
	float scrollX = ImGui::GetScrollX();
	if (left >= scrollX && right <= scrollX + visibleWidth)
		return;
	ImGui::SetScrollX((left + right) * 0.5f - visibleWidth * 0.5f);
}

} // namespace gui
