#include "Cpu.h"
#include "Debugger.h"
#include "imgui.h"
#include "Rom.h"
#include "stdio.h"


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
 static int FFilterAddressHex(ImGuiInputTextCallbackData* data) 
 { 
	 auto ch = data->EventChar;
	 if (((ch >= int('0')) & (ch <= int('9'))) |
		((ch >= int('a')) & (ch <= int('f'))) |
		((ch >= int('A')) & (ch <= int('F'))) |
		 (ch == int ('$')))
		 return 0; 
	 return 1; 
} 

bool FTryParseAddress(const char * pChz, u16 * pNAddr)
{
	auto pChzIt = pChz;
	u32 n = 0;
	if (*pChzIt == '$')
	{
		++pChzIt;

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

	if (n >= 0xFFFF)
		return false;

	*pNAddr = U16Coerce(n);
	return *pChzIt == '\0';
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
    bool fSeek = ImGui::InputText("seek address", aChSeek, FF_DIM(aChSeek), ImGuiInputTextFlags_CallbackCharFilter, FFilterAddressHex);

	ImGui::SameLine();
    ImGui::Checkbox("Show Bytes", &fShowBytes);

	// 
	auto pCart = pFam->m_pCart;
    ImGui::BeginChild("Listing");

    // Multiple calls to Text(), manually coarsely clipped - demonstrate how to use the ImGuiListClipper helper.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
    ImGuiListClipper clipper(pCart->m_aryAddrInstruct.C());

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
		if (FTryParseAddress(aChSeek, &addrSeek))
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