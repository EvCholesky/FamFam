
#include "CPU.h"
#include "Debugger.h"
#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"
#include "Platform.h"
#include "PPU.h"
#include "Rom.h"

#include <stdio.h>



int main(int cpChzArg, const char * apChzArg[])
{
	Cart cart;
	g_fam.m_pCart = &cart;

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
		printf("loaded '%s'   reset vector = %x\n", apChzArg[1], addrStartup);

	}

	Platform plat;
	if (!FTryInitPlatform(&plat))
		return 0;

	if (!FTryCreateWindow(&plat, 1200, 800, "FamFam"))
		return 0;

	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	
	// Initialize helper Platform and Renderer bindings (here we are using imgui_impl_win32 and imgui_impl_dx11)
	ImGui_ImplGlfw_InitForOpenGL(plat.m_pGlfwin, false);
    ImGui_ImplOpenGL2_Init();

//	if (!FTryAllLogTests())
//		return 0;

	Debugger debug;
	InitDebugger(&debug, &plat);

	bool fShowImguiDemoWindow = true;
	while (!FShouldWindowClose(&plat))
	{
		ClearScreen(&plat);

         // Feed inputs to dear imgui, start new frame
        ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

        if (fShowImguiDemoWindow)
		{
            ImGui::ShowDemoWindow(&fShowImguiDemoWindow);
		}

		ExecuteFamicomFrame(&g_fam);

		UpdateDebugger(&debug, &plat, &g_fam);

		// Render dear imgui into screen

		ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

		SwapBuffers(&plat);
		PollInput(&plat);
	}

	// Shutdown
    ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	ShutdownPlatform(&plat);
	return 1;
}
