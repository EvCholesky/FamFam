
#include "CPU.h"
#include "Debugger.h"
#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"
#include "Platform.h"
#include "PPU.h"
#include "Rom.h"

#include <stdio.h>

s64 g_cTickPerSecond = 1;

int main(int cpChzArg, const char * apChzArg[])
{
	static const int s_nHzTarget = 60;

	if (!FTryInitPlatform(&g_plat, s_nHzTarget))
		return 0;

	if (!FTryCreateWindow(&g_plat, 1200, 800, "FamFam"))
		return 0;

	Cart cart;
	g_fam.m_pCart = &cart;

	StaticInitFamicom(&g_fam, &g_plat);

	if (cpChzArg > 1)
	{
		FPOW fpow = FPOW_None;
		bool fLoaded = FTryLoadRomFromFile(apChzArg[1], &cart, &g_fam, fpow);
		if (!fLoaded)
		{
			printf("failed to load '%s'\n", apChzArg[1]);
			return 0;
		}

		u16 addrStartup = U16PeekMem(&g_fam.m_memmp, kAddrReset);
		printf("loaded '%s'   reset vector = 0x%04x\n", apChzArg[1], addrStartup);
	}

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	
	// Initialize helper Platform and Renderer bindings (here we are using imgui_impl_win32 and imgui_impl_dx11)
	ImGui_ImplGlfw_InitForOpenGL(g_plat.m_pGlfwin, false);
    ImGui_ImplOpenGL2_Init();

//	if (!FTryAllLogTests())
//		return 0;

	Debugger debug;
	InitDebugger(&debug, &g_plat);

	auto cTickLast = CTickWallClock();

	bool fShowImguiDemoWindow = true;
	while (!FShouldWindowClose(&g_plat))
	{
		StartTimer(TVAL_TotalFrame);
		ClearScreen(&g_plat);

         // Feed inputs to dear imgui, start new frame
        ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

        if (fShowImguiDemoWindow)
		{
            ImGui::ShowDemoWindow(&fShowImguiDemoWindow);
		}

		StartTimer(TVAL_FamicomUpdate);
		ExecuteFamicomFrame(&g_fam);
		EndTimer(TVAL_FamicomUpdate);

		StartTimer(TVAL_DebuggerUpdate);
		UpdateDebugger(&debug, &g_plat, &g_fam);
		EndTimer(TVAL_DebuggerUpdate);

		UpdateScreenWindow(&debug, &g_plat, &g_fam);

		// Render dear imgui into screen

		StartTimer(TVAL_ImguiRender);
		ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		EndTimer(TVAL_ImguiRender);


		SwapBuffers(&g_plat);
		PollInput(&g_plat, &g_fam);
		EndTimer(TVAL_TotalFrame);

		WaitUntilFrameEnd(&g_plat.m_pltime, cTickLast);
		FrameEndTimers(&g_plat);
		cTickLast = CTickWallClock();
	}

	// Shutdown
    ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	ShutdownPlatform(&g_plat);
	return 1;
}
