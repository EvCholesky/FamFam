#include "CPU.h"

#include <cstdlib> 
#include <stdio.h>

Famicom g_fam;

const AddrModeInfo * PAmodinfoFromAmrw(AMRW amrw)
{
#define ADDRMODE(NAME, MODE, CB) { AMRW_##NAME, AMOD_##MODE, CB },
	static const AddrModeInfo s_mpAmrwAmodinfo[] =
	{
		ADDRESSING_MODE_INFO
	};
#undef ADDRMODE

	if (amrw == AMRW_Nil)
		return nullptr;
	if (amrw < AMRW_Min || amrw > AMRW_Max)
	{
		//ASSERTCHZ(false)
		return nullptr;
	}

	return &s_mpAmrwAmodinfo[amrw];
}

const OpkInfo * POpkinfoFromOPK(OPK opk)
{
#define OPCODE(NAME, HINT) {#NAME, #HINT, true},
#define ILLOPC(NAME, HINT) {#NAME, #HINT, false},
	static const OpkInfo s_mpOpkOpkinfo[] =
	{
		OPCODE_KIND_INFO
	};
#undef OPCODE
#undef ILLOPC

	if (opk == OPK_Nil)
		return nullptr;
	if (opk < OPK_Min || opk > OPK_Max)
	{
		//ASSERTCHZ(false)
		return nullptr;
	}

	return &s_mpOpkOpkinfo[opk];
}

const OpInfo * POpinfoFromOpcode(u8 nOpcode)
{
#define OP(NAME, OPKIND, ADDR, CYCLE) {#OPKIND, OPK_##OPKIND, AMRW_##ADDR, U8Coerce(abs(CYCLE)), CYCLE < 0},
	static const OpInfo s_mpOpcodeOpinfo[] =
	{
		OPCODE_TABLE
	};
#undef OP

	return &s_mpOpcodeOpinfo[nOpcode];
}

IMapper * PMapperFromMapperk(MAPPERK mapperk)
{
	return nullptr;
}

void DumpAsm(Famicom * pFam, u16 addrMin, int cB)
{
	u32 addrMax = addrMin + cB;
	u32 addr = addrMin;
	while (addr < addrMax)
	{
		u8 aB[3];
		aB[0] = U8ReadAddress(pFam, addr);
		auto pOpinfo = POpinfoFromOpcode(aB[0]); 
		auto pOpkinfo = POpkinfoFromOPK(pOpinfo->m_opk);
		auto pAmodinfo = PAmodinfoFromAmrw(pOpinfo->m_amrw);

		for (int iB=1; iB<pAmodinfo->m_cB; ++iB)
		{
			aB[iB] = U8ReadAddress(pFam, addr + iB);
		}
		switch (pAmodinfo->m_cB)
		{
			case 1: printf("$%04X: %02X        ", addr, aB[0]); break;
			case 2: printf("$%04X: %02X %02X     ", addr, aB[0], aB[1]); break;
			case 3: printf("$%04X: %02X %02X %02X  ", addr, aB[0], aB[1], aB[2]); break;
			default: FF_ASSERT(false, "unexpected opcode size");
		}

		printf("%s%s ", (pOpkinfo->m_fIsLegal) ? "  " : "!!", pOpinfo->m_pChzMnemonic);

		switch(pAmodinfo->m_amod)
		{
			case AMOD_Implicit:											break;
			case AMOD_Immediate: printf("#$%02X ", aB[1]);				break;
			case AMOD_Relative:
				{
					u16 addrRel = addr + pAmodinfo->m_cB + s8(aB[1]);
					printf("$%04X ", addrRel);
				}break;
			case AMOD_Accumulator:		break;
			case AMOD_Absolute: printf("$%02X%02X ", aB[2], aB[1]);		break;
			case AMOD_AbsoluteX: printf("$%02X%02X,X ", aB[2], aB[1]);	break;
			case AMOD_AbsoluteY: printf("$%02X%02X,Y ", aB[2], aB[1]);	break;
			case AMOD_ZeroPage: printf("$00%02X ", aB[1]);				break;
			case AMOD_ZeroPageX: printf("$%02X,X ", aB[1]);				break;
			case AMOD_ZeroPageY: printf("$%02X,Y ", aB[1]);				break;
			case AMOD_Indirect: printf("($%02X%02X) ", aB[2], aB[1]);	break;
			case AMOD_IndirectX: printf("($%02X,X) ", aB[1]);			break;
			case AMOD_IndirectY: printf("($%02X),Y ", aB[1]);			break;
			default:
				FF_ASSERT(false, "unhandled addressing mode.");
		}
		printf("\n");
		addr += pAmodinfo->m_cB;
	}
}