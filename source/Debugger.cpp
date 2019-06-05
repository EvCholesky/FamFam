#include "Cpu.h"
#include "Debugger.h"
#include "imgui.h"
#include "Platform.h"
#include "Rom.h"
#include "stdio.h"
#include <ctype.h>

static float s_dXRegisterDec = 50.0f;
static float s_dXRegisterHex = 50.0f;
static ImVec2 s_szRegisterHex = {43.0f, 19.0f};

void GetOpcodeColor(OPCAT opcat, ImVec4 * pCol)
{
	struct ColTriplet { float m_r, m_g, m_b; };
	static const ColTriplet s_mpOpcatColt[] =
	{
		{ 1.0f, 0.6f, 0.6f},		// OPCAT_Arithmetic,
		{ 1.0f, 1.0f, 0.6f},		// OPCAT_Branch,
		{ 0.4f, 0.8f, 0.4f},		// OPCAT_Flags,
		{ 0.8f, 0.4f, 0.4f},		// OPCAT_Illegal,
		{ 0.6f, 1.0f, 1.0f},		// OPCAT_Logic,
		{ 0.5f, 0.5f, 0.5f},		// OPCAT_Nop,
		{ 0.6f, 1.0f, 1.0f},		// OPCAT_Memory,
		{ 1.0f, 0.6f, 1.0f},		// OPCAT_Stack,
		{ 0.6f, 0.6f, 1.0f},		// OPCAT_Transfer,
	};
	static_assert(FF_DIM(s_mpOpcatColt) == OPCAT_Max, "missing Operand category color");
	if (opcat < OPCAT_Min || opcat >= OPCAT_Max)
	{
		FF_ASSERT(false, "unexpected operand category");
		*pCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}

	auto & colt = s_mpOpcatColt[opcat];
	*pCol = ImVec4(colt.m_r, colt.m_g, colt.m_b, 1.0f);
}
 static int FFilterHexText(ImGuiInputTextCallbackData* data) 
 { 
	 auto ch = data->EventChar;
	 if (((ch >= int('0')) & (ch <= int('9'))) |
		((ch >= int('a')) & (ch <= int('f'))) |
		((ch >= int('A')) & (ch <= int('F'))) |
		 (ch == int ('$')))
		 return 0; 
	 return 1; 
} 

bool FTryParseU32(const char * pChz, u32 * pN)
{
	auto pChzIt = pChz;
	u32 n = 0;
	bool fIsHex = false; 
	if (pChzIt[0] == '0' && pChzIt[1] == 'x')
	{
		pChzIt += 2;
		fIsHex = true;
	}
	else if (*pChzIt == '$')
	{
		++pChzIt;
		fIsHex = true;
	}

	if (fIsHex)
	{
		// hex literal
		while (*pChzIt != '\0') 
		{
			if		((*pChzIt >= '0') & (*pChzIt <= '9'))	{ n = n*16 + (*pChzIt - '0'); }
			else if ((*pChzIt >= 'a') & (*pChzIt <= 'f'))	{ n = n*16 + (*pChzIt - 'a') + 10; }
			else if ((*pChzIt >= 'A') & (*pChzIt <= 'F'))	{ n = n*16 + (*pChzIt - 'A') + 10; }
			else
			{
			    break;
			}
			++pChzIt;
		}
	}
	else
	{
		// decimal literal
		while (pChzIt != '\0')
		{
			if ((*pChzIt >= '0') & (*pChzIt <= '9'))
			{
				n = n*10 + (*pChzIt - '0');
			}
			else
				break;
			++pChzIt;
	    }
	}

	*pN = n;
	return *pChzIt == '\0';
}

bool FTryParseU8(const char * pChz, u8 * pNAddr)
{
	u32 n;
	if (!FTryParseU32(pChz, &n))
		return false;
	if (n >= 0xFF)
		return false;

	*pNAddr = U8Coerce(n);
	return true;
}

bool FTryParseU16(const char * pChz, u16 * pNAddr)
{
	u32 n;
	if (!FTryParseU32(pChz, &n))
		return false;
	if (n >= 0xFFFF)
		return false;

	*pNAddr = U16Coerce(n);
	return true;
}

int IFindAddr(const DynAry<u16> & aryAddr, u16 addr)
{
	if (aryAddr.FIsEmpty())	
		return -1;

	if (addr < aryAddr[0] || addr > aryAddr.Last())
		return -1;

	int i = 0;
	auto pAddrMax = aryAddr.PMax();
	for (auto pAddrIt = aryAddr.A(); pAddrIt != pAddrMax; ++pAddrIt, ++i)
	{
		if (addr <= *pAddrIt)
			return i;
	}
	return -1;
}

void InitDebugger(Debugger * pDebug, Platform * pPlat)
{
	pDebug->m_ppuwin.m_pTexChr = PTexCreate(pPlat, s_dXChrHalf * 2, s_dYChrHalf);
	pDebug->m_ppuwin.m_fChrNeedsRefresh = true;

	//pDebug->m_pTexNametable;

	/*
	// build pallet stripes
	auto pB = pDebug->m_pTexChr->m_pB;
	for (int y = 0; y < 256; ++y)
	{
		for (int x = 0; x < 256; ++x)
		{
			int xCol = x / 16; 
			int yCol = y / 64; 
			int iCol = xCol + yCol * 16;
			RGBA rgba = RgbaFromHwcol(HWCOL(iCol));
			*pB++ = rgba.m_r;
			*pB++ = rgba.m_g;
			*pB++ = rgba.m_b;
		}
	}*/
}

void UpdateDebugger(Debugger * pDebug, Platform * pPlat, Famicom * pFam)
{
	if (pDebug->m_fShowDisasmWindow)
	{
		UpdateDisassemblyWindow(pFam, &pDebug->m_fShowDisasmWindow);
	}

	if (pDebug->m_fShowRegisterWindow)
	{
		UpdateRegisterWindow(pFam, &pDebug->m_fShowRegisterWindow);
	}

	if (pDebug->m_ppuwin.m_fShowWindow)
	{
		UpdateChrWindow(pDebug, pFam, pPlat);
	}
}

void UpdateDisassemblyWindow(Famicom * pFam, bool * pFShowDisasm)
{
    ImGui::SetNextWindowSize(ImVec2(520,600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Disassembly", pFShowDisasm))
    {
        ImGui::End();
        return;
    }

	// toolbar
	static bool fShowBytes = true;
	static char aChSeek[8] = "$";
    bool fSeek = ImGui::InputText("seek address", aChSeek, FF_DIM(aChSeek), ImGuiInputTextFlags_CallbackCharFilter, FFilterHexText);

	ImGui::SameLine();
    ImGui::Checkbox("Show Bytes", &fShowBytes);

	// 
	auto pCart = pFam->m_pCart;
    ImGui::BeginChild("Listing");

    // Multiple calls to Text(), manually coarsely clipped - demonstrate how to use the ImGuiListClipper helper.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
    ImGuiListClipper clipper(int(pCart->m_aryAddrInstruct.C()));

	char aChOpcode[128];
	
	static float s_dXAddr = 50.0f;
	static float s_dXBytes = 100.0f;
	static ImVec4 colRed(0.6f, 0.2f, 0.2f, 1.0f);
	static ImVec4 colAddr(0.4f, 0.4f, 0.4f, 1.0f);
	static ImVec4 colBytes(0.5f, 0.5f, 0.8f, 1.0f);

    ImGui::Columns(3, "mycolumns3", false);  // 3-ways, no border
	auto pMemmp = &pFam->m_memmp;
    while (clipper.Step())
	{
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
		{
			u16 addr = pCart->m_aryAddrInstruct[i];

			u8 aB[3];
			aB[0] = U8PeekMem(pMemmp, addr);
			auto pOpinfo = POpinfoFromOpcode(aB[0]); 
			auto pOpkinfo = POpkinfoFromOPK(pOpinfo->m_opk);
			auto pAmrwinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

			for (int iB=1; iB<pAmrwinfo->m_cB; ++iB)
			{
				aB[iB] = U8PeekMem(pMemmp, addr + iB);
			}

            ImGui::TextColored(colAddr, "$%04X", addr);
            ImGui::SetColumnWidth(-1, s_dXAddr);
			ImGui::NextColumn();

			if (fShowBytes)
			{
				switch (pAmrwinfo->m_cB)
				{
					case 1: ImGui::TextColored(colBytes, "%02X        ", aB[0]); break;
					case 2: ImGui::TextColored(colBytes, "%02X %02X     ",aB[0], aB[1]); break;
					case 3: ImGui::TextColored(colBytes, "%02X %02X %02X  ", aB[0], aB[1], aB[2]); break;
					default: FF_ASSERT(false, "unexpected opcode size");
				}
			}

            ImGui::SetColumnWidth(-1, s_dXBytes);
            ImGui::NextColumn();


			ImVec4 colOperand;
			GetOpcodeColor(pOpkinfo->m_opcat, &colOperand);
            ImGui::TextColored(colOperand, "%s", pOpinfo->m_pChzMnemonic);
		    if (ImGui::IsItemHovered())
		    {
		        ImGui::BeginTooltip();
				ImGui::Text("%s: %s", pOpkinfo->m_pChzName, pOpkinfo->m_pChzHint);
				ImGui::SameLine();

				auto pAmodinfo = PAmodinfoFromAmod(pAmrwinfo->m_amod);
				ImGui::TextColored(colRed, " %s", pAmodinfo->m_pChzDesc);
		        ImGui::EndTooltip();
			}
			ImGui::SameLine();

			switch(pAmrwinfo->m_amod)
			{
				case AMOD_Implicit:																				break;
				case AMOD_Immediate: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " #$%02X ", aB[1]);				break;
				case AMOD_Relative:
					{
						u16 addrrel = addr + pAmrwinfo->m_cB + s8(aB[1]);
						sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%04x ", addrrel);
					}break;
				case AMOD_Accumulator:		break;
				case AMOD_Absolute: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%02x%02x ", aB[2], aB[1]);		break;
				case AMOD_AbsoluteX: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%02x%02x,x ", aB[2], aB[1]);	break;
				case AMOD_AbsoluteY: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%02x%02x,y ", aB[2], aB[1]);	break;
				case AMOD_ZeroPage: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $00%02x ", aB[1]);				break;
				case AMOD_ZeroPageX: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%02x,x ", aB[1]);				break;
				case AMOD_ZeroPageY: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " $%02x,y ", aB[1]);				break;
				case AMOD_Indirect: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " ($%02x%02x) ", aB[2], aB[1]);		break;
				case AMOD_IndirectX: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " ($%02x,x) ", aB[1]);				break;
				case AMOD_IndirectY: sprintf_s(aChOpcode, FF_DIM(aChOpcode), " ($%02x),y ", aB[1]);				break;
				default:
					FF_ASSERT(false, "unhandled addressing mode.");
			}

            ImGui::Text("%s", aChOpcode);
			//ImGui::SameLine();
            //ImGui::TextColored(colRed, "END");
            ImGui::NextColumn();
		}
	}
    ImGui::PopStyleVar();


	if (fSeek)
	{
		u16 addrSeek;
		if (FTryParseU16(aChSeek, &addrSeek))
		{
			int iAddr = IFindAddr(pCart->m_aryAddrInstruct, addrSeek);
			if (iAddr >= 0)
			{
				ImGui::SetScrollY(iAddr * clipper.ItemsHeight);
			}
		}
	}

    ImGui::EndChild();
    ImGui::End();
}

ImVec4 * PColRegister(bool fEnabled, bool fChanged)
{
	static ImVec4 s_colEnabledSame(0.7f, 0.7f, 0.7f, 1.0f);
	static ImVec4 s_colEnabledChange(0.8f, 0.4f, 0.4f, 1.0f);
	static ImVec4 s_colDisabledSame(0.4f, 0.4f, 0.4f, 1.0f);
	static ImVec4 s_colDisabledChange(0.5f, 0.2f, 0.2f, 1.0f);
	if (fEnabled)
	{
		return (fChanged) ? &s_colEnabledSame : &s_colEnabledChange;
	}

	return (fChanged) ? &s_colDisabledSame : &s_colDisabledChange;
}

bool FInputRegister8(const char * pChzLabel, u8 * pB, u8 bPrev)
{
	ImGui::PushID(pB);
	char aChDec[32];
	char aChHex[32];
	sprintf_s(aChDec, FF_DIM(aChDec), "%d", *pB);

	auto pCol = PColRegister(true, *pB == bPrev);
	ImGui::TextColored(*pCol, aChDec);
	ImGui::SetColumnWidth(-1, s_dXRegisterDec);
	ImGui::NextColumn();

	sprintf_s(aChHex, FF_DIM(aChHex), "$%02x", *pB);
	
	bool fEdited = false;
	if (ImGui::BeginPopup("Edit register"))
	{
		fEdited = ImGui::InputText(pChzLabel, aChHex, FF_DIM(aChHex), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCharFilter, FFilterHexText);
		u8 nReg;
		if (fEdited && FTryParseU8(aChHex, &nReg))
		{
			*pB = nReg;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::Button(aChHex, s_szRegisterHex))
	{
		ImGui::OpenPopup("Edit register");
	}

	ImGui::SetColumnWidth(-1, s_dXRegisterHex);
	ImGui::NextColumn();

	ImGui::Text(pChzLabel);
	ImGui::NextColumn();

	ImGui::PopID();
	return fEdited;
}

bool FInputRegister16(const char * pChzLabel, u16 * pB, u16 bPrev)
{
	ImGui::PushID(pB);
	char aChDec[32];
	char aChHex[32];
	sprintf_s(aChDec, FF_DIM(aChDec), "%d", *pB);

	auto pCol = PColRegister(true, *pB == bPrev);
	ImGui::TextColored(*pCol, aChDec);
	ImGui::SetColumnWidth(-1, s_dXRegisterDec);
	ImGui::NextColumn();

	bool fEdited = false;
	sprintf_s(aChHex, FF_DIM(aChHex), "$%04x", *pB);
	if (ImGui::BeginPopup("Edit register"))
	{
		fEdited = ImGui::InputText(pChzLabel, aChHex, FF_DIM(aChHex), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCharFilter, FFilterHexText);
		u16 nReg;
		if (fEdited && FTryParseU16(aChHex, &nReg))
		{
			*pB = nReg;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::Button(aChHex, s_szRegisterHex))
	{
		ImGui::OpenPopup("Edit register");
	}

	ImGui::SetColumnWidth(-1, s_dXRegisterHex);
	ImGui::NextColumn();

	ImGui::Text(pChzLabel);
	ImGui::NextColumn();

	ImGui::PopID();
	return fEdited;
}



void UpdateFlagsString(const char * aChzLetters, u8 bFlags, u8 bFlagsPrev)
{
	char aChz[2];
	aChz[1] = '\0';

	int iBit = 7;
	for (const char * pCh = aChzLetters; *pCh != '\0'; ++pCh, --iBit)
	{
		u8 bMask = 0x1 << iBit;

		bool fIsSet = (bFlags & bMask) != 0;
		bool fWasSet = (bFlagsPrev & bMask) != 0;
		aChz[0] = (fIsSet) ? toupper(*pCh) : tolower(*pCh);
		auto pCol = PColRegister(fIsSet, fIsSet == fWasSet);

		ImGui::TextColored(*pCol, aChz);

		if (iBit > 0)
		{
			ImGui::SameLine(0, 0);
		}
		else 
		{
			break;
		}
	}
}

void UpdateRegisterWindow(Famicom * pFam, bool * pFShowWindow)
{
    ImGui::SetNextWindowSize(ImVec2(520,300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Register", pFShowWindow))
    {
        ImGui::End();
        return;
    }

	Cpu * pCpu = &pFam->m_cpu;
	Cpu * pCpuPrev = &pFam->m_cpu;
	UpdateFlagsString("no_bdizc", pCpu->m_p, pCpuPrev->m_p);

	ImGui::Columns(3, "regCol", false);  // 3-ways, no border

	FInputRegister8("A", &pCpu->m_a, pCpuPrev->m_a);
	FInputRegister8("X", &pCpu->m_x, pCpuPrev->m_x);
	FInputRegister8("Y", &pCpu->m_y, pCpuPrev->m_y);
	FInputRegister8("SP", &pCpu->m_sp, pCpuPrev->m_sp);
	FInputRegister16("PC", &pCpu->m_pc, pCpuPrev->m_pc);

    ImGui::End();
}

void UpdateChrWindow(Debugger * pDebug, Famicom * pFam, Platform * pPlat)
{
	auto pPpuwin = &pDebug->m_ppuwin;
	if (pPpuwin->m_fChrNeedsRefresh)
	{
		DrawChrMemory(&pFam->m_ppu, pPpuwin->m_pTexChr, pPpuwin->m_fUse8x16Mode);
		UploadTexture(pPlat, pPpuwin->m_pTexChr);
	}

    ImGui::SetNextWindowSize(ImVec2(520,300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("PPU", &pPpuwin->m_fShowWindow))
    {
        ImGui::End();
        return;
    }

	ImGui::Image((void*)(intptr_t)pPpuwin->m_pTexChr->m_nId, ImVec2(512, 256));

    if (ImGui::Checkbox("Use 8x16 Sprites", &pPpuwin->m_fUse8x16Mode))
	{
		pPpuwin->m_fChrNeedsRefresh = true;
	}

    ImGui::End();
}
