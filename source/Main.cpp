
#include "CPU.h"
#include "imgui.h"
#include "examples/imgui_impl_glfw.h"
#include "examples/imgui_impl_opengl2.h"
#include "Platform.h"
#include "PPU.h"
#include "Rom.h"

#include <stdio.h>

u8 U8ReadAddress(Famicom * pFam, u16 addr)
{
	if (addr > 0x8000)
	{
		u16 addrAdj = (addr > 0xBFFF) ? (addr - 0xC000) : (addr - 0x8000);
		return pFam->m_pCart->m_pBPrgRom[addrAdj];
	}

	return 0;
}

u16 U16ReadAddress(Famicom * pFam, u16 addr)
{
	u16 n = U8ReadAddress(pFam, addr) | (u16(U8ReadAddress(pFam, addr+1)) << 8);
	return n;
}

int main(int cpChzArg, const char * apChzArg[])
{
	Famicom	fam;
	Cart cart;
	fam.m_pCart = &cart;

	if (cpChzArg > 1)
	{
		bool fLoaded = FTryLoadRomFromFile(apChzArg[1], &cart);
		if (!fLoaded)
		{
			printf("failed to load '%s'\n", apChzArg[1]);
			return 0;
		}

		u16 addrStartup = U16ReadAddress(&fam, kAddrReset);
		printf("loaded '%s'   reset vector = %x\n", apChzArg[1], addrStartup);
		DumpAsm(&fam, addrStartup, 100);
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
