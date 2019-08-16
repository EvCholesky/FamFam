
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

void UpdateSaveState(Platform * pPlat, Famicom * pFam, SaveOutStream * pSos)
{
	static const char * s_pChzSave = "default";
	static const char * s_pChzExtension = ".Famsave";
	char * pChzSave = nullptr;

	if (pPlat->m_fRequestedSaveState || pPlat->m_fRequestedLoadState)
	{
		const char * pChzCart = pFam->m_pCart->m_pChzName;
		if (pChzCart && pChzCart[0] != '\0') 
		{
			size_t iBFilename = 0;
			size_t iBExtension = 0;
			size_t iBEnd = 0;
			SplitFilename(pChzCart, &iBFilename, &iBExtension, &iBEnd);

			size_t cChFilename = iBExtension - iBFilename;
			size_t cBSaveName = cChFilename + strlen(s_pChzExtension) + 1;

			pChzSave = new char  [cBSaveName];
			memcpy(pChzSave, &pChzCart[iBFilename], cChFilename);
			strcpy_s(&pChzSave[cChFilename], cBSaveName - cChFilename, s_pChzExtension);
		}
		else
		{
			size_t cBSaveName = strlen(s_pChzSave) + strlen(s_pChzExtension) + 1;
			pChzSave = new char [cBSaveName];
			sprintf_s(pChzSave, cBSaveName, "%s%s", s_pChzSave, s_pChzExtension);
		}
	}
	
	if (pPlat->m_fRequestedSaveState)
	{
		pPlat->m_fRequestedSaveState = false;

		if (!pSos->FIsEmpty())
		{
			pSos->Clear();
		}

		WriteSave(pSos, pFam);

		FTryWriteSaveToFile(pChzSave, pSos);
	}

	if (pPlat->m_fRequestedLoadState)
	{
		pPlat->m_fRequestedLoadState = false;

		//SaveInStream sis(pSos);
		SaveInStream sis;
		bool fReadSuccess = false;
		if (FTryReadSaveFromFile(pChzSave, &sis))
		{
			fReadSuccess = FTryReadSave(&sis, pFam);
		}

		if (!fReadSuccess)
		{
			printf("failed restoring save state: '%s'\n", pChzSave);
		}
	}

	if (pChzSave)
	{
		free(pChzSave);
		pChzSave = nullptr;
	}
}

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

	SaveOutStream sos;
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

		UpdatePlatformAudio(&g_fam, &g_plat.m_plaud);

		SwapBuffers(&g_plat);
		PollInput(&g_plat, &g_fam);
		EndTimer(TVAL_TotalFrame);

		UpdateSaveState(&g_plat, &g_fam, &sos);

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
