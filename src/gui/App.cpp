// SPDX-License-Identifier: MIT
// wifiscan - copyright (c) 2026 Lukas Hosek
#include "App.hpp"
#include "Theme.hpp"
#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace gui
{

// Duplicated from ui/App.cpp (mirrored): formats a steady_clock time point as a
// local wall-clock HH:MM:SS string.
static std::string FormatTime(std::chrono::steady_clock::time_point timePoint)
{
	auto wallTime = std::chrono::system_clock::now() +
		(timePoint - std::chrono::steady_clock::now());
	auto timet = std::chrono::system_clock::to_time_t(wallTime);
	struct tm localTime{};
	localtime_r(&timet, &localTime);
	return fmt::format("{:02d}:{:02d}:{:02d}", localTime.tm_hour,
		localTime.tm_min, localTime.tm_sec);
}

App::App(wifi::IScanner& scanner) : _scanner(scanner), _statusBarPanel(scanner)
{
}

void App::ScanLoop(std::stop_token stopToken)
{
	while (!stopToken.stop_requested())
	{
		auto freshNetworks = _scanner.GetNetworks();
		auto scanEnd = std::chrono::steady_clock::now();

		{
			std::lock_guard lock(_mutex);
			_networks = std::move(freshNetworks);

			const std::string& lastError = _scanner.GetLastError();
			if (lastError.empty())
				_statusMsg = "Last update: " + FormatTime(scanEnd);
			else
				_statusMsg = "Error: " + lastError;
		}

		// Wake the render loop. glfwPostEmptyEvent is documented thread-safe.
		glfwPostEmptyEvent();

		_scanner.TriggerScan(stopToken);
	}
}

void App::Run()
{
	if (!glfwInit())
	{
		fprintf(stderr, "wifiscan --gui: failed to initialise GLFW\n");
		return;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	// Desktop content scale. On X11 this is derived from Xft.dpi, which the
	// Cinnamon/Mint display-scaling setting drives, so the window and all UI
	// scale up on HiDPI displays instead of rendering tiny.
	float dpiScale =
		ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
	if (dpiScale <= 0.0f)
		dpiScale = 1.0f;

	_window = glfwCreateWindow(static_cast<int>(1100 * dpiScale),
		static_cast<int>(720 * dpiScale), "wifiscan", nullptr, nullptr);
	if (_window == nullptr)
	{
		fprintf(stderr,
			"wifiscan --gui: failed to create window (no display? try "
			"sudo -E and check $DISPLAY)\n");
		glfwTerminate();
		return;
	}

	glfwMakeContextCurrent(_window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Keyboard nav left disabled so the arrow keys stay free for panel
	// scrolling.
	ImGui::StyleColorsDark();

	// Override ImGui chrome colors to match the dark-navy theme.
	{
		ImVec4* c = ImGui::GetStyle().Colors;
		auto tv = [](theme::UiColor u)
		{ return ImGui::ColorConvertU32ToFloat4(theme::Color(u)); };
		c[ImGuiCol_WindowBg] = tv(theme::UiColor::AppBackground);
		c[ImGuiCol_ChildBg] = tv(theme::UiColor::AppBackground);
		c[ImGuiCol_TableHeaderBg] = tv(theme::UiColor::Surface);
		c[ImGuiCol_TableRowBg] = tv(theme::UiColor::RowBg);
		c[ImGuiCol_TableRowBgAlt] = tv(theme::UiColor::SurfaceAlt);
		c[ImGuiCol_TableBorderLight] = tv(theme::UiColor::Border);
		c[ImGuiCol_TableBorderStrong] = tv(theme::UiColor::BorderStrong);
		c[ImGuiCol_Separator] = tv(theme::UiColor::Border);
		c[ImGuiCol_Button] = tv(theme::UiColor::Surface);
		c[ImGuiCol_ButtonHovered] = tv(theme::UiColor::SurfaceHover);
		c[ImGuiCol_ButtonActive] = tv(theme::UiColor::SurfaceActive);
		c[ImGuiCol_Header] = tv(theme::UiColor::SelectionBg);
		c[ImGuiCol_HeaderHovered] = tv(theme::UiColor::SelectionBgHover);
		c[ImGuiCol_HeaderActive] = tv(theme::UiColor::SelectionBgActive);
	}

	// Apply the desktop scale to widget metrics and fonts. ImGui 1.92's dynamic
	// font system rerasterizes the embedded font at the scaled size, so no TTF
	// needs to be loaded. Panels read style.FontScaleDpi for their own pixel
	// constants.
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(dpiScale);
	style.FontScaleDpi = dpiScale;

	ImGui_ImplGlfw_InitForOpenGL(_window, true);
	ImGui_ImplOpenGL3_Init("#version 130");

	_scanThread = std::jthread(
		[this](std::stop_token stopToken) { ScanLoop(stopToken); });

	ImVec4 clear = ImGui::ColorConvertU32ToFloat4(
		theme::Color(theme::UiColor::AppBackground));

	while (!glfwWindowShouldClose(_window) && !_quit)
	{
		// Idle-efficient: woken by input or by the scan thread's
		// postEmptyEvent.
		glfwWaitEventsTimeout(0.25);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		RenderFrame();

		ImGui::Render();
		int fbWidth = 0;
		int fbHeight = 0;
		glfwGetFramebufferSize(_window, &fbWidth, &fbHeight);
		glViewport(0, 0, fbWidth, fbHeight);
		glClearColor(clear.x, clear.y, clear.z, clear.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(_window);
	}

	// Stop the scan thread BEFORE tearing down GLFW: a late
	// glfwPostEmptyEvent() after glfwTerminate() would crash.
	_scanThread.request_stop();
	if (_scanThread.joinable())
		_scanThread.join();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(_window);
	glfwTerminate();
}

void App::RenderFrame()
{
	std::vector<wifi::Network> networks;
	std::string statusMsg;
	{
		std::lock_guard lock(_mutex);
		networks = _networks;
		statusMsg = _statusMsg;
	}
	_statusBarPanel.SetStatus(statusMsg);

	if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Q))
		_quit = true;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus;

	if (ImGui::Begin("##wifiscan", nullptr, kFlags))
	{
		float statusH = ImGui::GetTextLineHeightWithSpacing();
		float availH = ImGui::GetContentRegionAvail().y;
		float bodyH = std::max(0.0f, availH - statusH);
		float spectrumH = bodyH * 0.4f;
		float tableH = bodyH - spectrumH;

		ImGui::BeginChild("spectrum", ImVec2(0, spectrumH), true);
		_spectrumPanel.Render(networks);
		ImGui::EndChild();

		ImGui::BeginChild("table", ImVec2(0, tableH), true);
		_networkTablePanel.Render(networks);
		ImGui::EndChild();

		_statusBarPanel.Render();
	}
	ImGui::End();
}

} // namespace gui
