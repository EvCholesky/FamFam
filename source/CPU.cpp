#include "CPU.h"

#include <cstdlib> 
#include <stdio.h>

Famicom g_fam;

const AddrModeInfo * PAmodinfoFromAmod(AMOD amod)
{
#define INFO(AM, DESC) { AMOD_##AM, DESC },
	static const AddrModeInfo s_mpAmodAmodinfo[] =
	{
		ADDRESS_MODE_INFO
	};
#undef INFO

	if (amod == AMOD_Nil)
		return nullptr;
	if (amod < AMOD_Min || amod > AMOD_Max)
	{
		//ASSERTCHZ(false)
		return nullptr;
	}

	return &s_mpAmodAmodinfo[amod];
}

const AddrModeRwInfo * PAmrwinfoFromAmrw(AMRW amrw)
{
#define INFO(NAME, MODE, CB) { AMRW_##NAME, AMOD_##MODE, CB },
	static const AddrModeRwInfo s_mpAmrwAmrwinfo[] =
	{
		ADDRESS_MODE_RW_INFO
	};
#undef INFO

	if (amrw == AMRW_Nil)
		return nullptr;
	if (amrw < AMRW_Min || amrw > AMRW_Max)
	{
		//ASSERTCHZ(false)
		return nullptr;
	}

	return &s_mpAmrwAmrwinfo[amrw];
}

const OpkInfo * POpkinfoFromOPK(OPK opk)
{
#define OPCODE(NAME, OPC, HINT) {#NAME, #HINT, OPCAT_##OPC},
	static const OpkInfo s_mpOpkOpkinfo[] =
	{
		OPCODE_KIND_INFO
	};
#undef OPCODE

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

u8 U8MemRead(MemoryMap * pMemmp, u16 addr)
{
#if OLD_MAPPER
	MemorySlice * pMemsl = &pMemmp->m_aMemly[addr >> 10];
	u16 addrSlice = addr & 0x3FF;
	u8 * pB = pMemsl->m_aPRead[addrSlice >> pMemsl->m_cBitShift];

	// if memory isn't mapped at this location return the last thing on the bus 
	if (!pB)
		return pMemmp->m_bPrev;

	u8 b = *(pB + (addrSlice & pMemsl->m_nMask));
	pMemmp->m_bPrev = b;
	return b;
#else
	auto iMemsl = pMemmp->m_mpAddrISlice[addr];
	auto pMemsl = &pMemmp->m_aMemsl[iMemsl];
	if ((pMemsl->m_fmemsl & FMEMSL_Unmapped) != 0)
	{
		return pMemmp->m_bPrev;
	}

	u32 dAddr = addr - pMemsl->m_addrBase;
	dAddr = dAddr & pMemsl->m_nMaskMirror;

	u8 b = pMemsl->m_pB[dAddr];
	pMemmp->m_bPrev = b;
	return b;
#endif
}

void U8MemWrite(MemoryMap * pMemmp, u16 addr, u8 b)
{
#if OLD_MAPPER
	MemorySlice * pMemsl = &pMemmp->m_aMemly[addr >> 10];
	u16 addrSlice = addr & 0x3FF;
	u8 * pB = pMemsl->m_aPRead[addrSlice >> pMemsl->m_cBitShift];

	pMemmp->m_bPrev = b;

	// we expect that unmapped memory at this location has been pointed at dummy memory
	*(pB + (addrSlice & pMemsl->m_nMask)) = b;
#else
#endif
}

#if OLD_MAPPER
void MapMemorySlicesToRam(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
}

void MapMemorySlicesToRom(MemoryMap * pMemmp, u32 addrMin, u8 * pBRom, int cBRom)
{
}

void MapMirroredMemorySlices(MemoryMap * pMemmp, u32 addSrc, int cB, u32 addrMirrorMin, u32 addrMirrorMax)
{
}

void UnmapMemorySlices(MemoryMap * pMemmp, u32 addrMin, u8 * pBRom, int cBRom)
{
}
#else
MemoryMap::MemoryMap()
:m_bPrev(0)
,m_cMemsl(1)	// allocate the first memory slice as an invalid slice
{
	ZeroAB(m_aMemsl, sizeof(m_aMemsl));
	ZeroAB(m_mpAddrISlice, sizeof(m_mpAddrISlice));
}

void ClearMemmp(MemoryMap * pMemmp)
{
	MemorySlice * pMemslMax = &pMemmp->m_aMemsl[pMemmp->m_cMemsl];
	for (MemorySlice * pMemslIt = pMemmp->m_aMemsl; pMemslIt != pMemslMax; ++pMemslIt)
	{
		if (pMemslIt->m_pB)
		{
			free(pMemslIt->m_pB);
			ZeroAB(pMemslIt, sizeof(*pMemslIt)); 
		}
	}

	ZeroAB(pMemmp->m_mpAddrISlice, sizeof(pMemmp->m_mpAddrISlice));
}


void VerifyMemorySpanClear(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	// make sure this span is not overlapping any other spans
	MemorySlice * pMemslMax = &pMemmp->m_aMemsl[pMemmp->m_cMemsl];
	for (auto pMemsl = pMemmp->m_aMemsl; pMemsl != pMemslMax; ++pMemsl)
	{
		FF_ASSERT(addrMax < pMemsl->m_addrBase || addrMin >= u32(pMemsl->m_addrBase + pMemsl->m_cB), "detected memory span overlap");
	}

	addrMax = min(addrMax, FF_DIM(pMemmp->m_mpAddrISlice));
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		FF_ASSERT(pMemmp->m_mpAddrISlice[addrIt] == 0);
	}
}

void VerifyRom(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
}

void VerifyMirror(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
}

MemorySlice * IMemslAllocate(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	VerifyMemorySpanClear(pMemmp, addrMin, addrMax);
	auto iMemsl = pMemmp->m_cMemsl++;	

	addrMax = min(addrMin, FF_DIM(pMemmp->m_mpAddrISlice));
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		pMemmp->m_mpAddrISlice[addrIt] = iMemsl;
	}

	MemorySlice * pMemsl = &pMemmp->m_aMemsl[iMemsl];
	pMemsl->m_fmemsl |= FMEMSL_InUse;
	return pMemsl;
}

MemorySlice * PMemslMapRam(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	auto pMemsl = IMemslAllocate(pMemmp, addrMin, addrMax);
	pMemsl->m_addrBase = addrMin;
	pMemsl->m_cB = addrMax - addrMin;
	pMemsl->m_pB = (u8 *)malloc(pMemsl->m_cB);
	return pMemsl;
}

MemorySlice * PMemslMapRom(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	auto pMemsl = IMemslAllocate(pMemmp, addrMin, addrMax);
	pMemsl->m_addrBase = addrMin;
	pMemsl->m_cB = addrMax - addrMin;
	pMemsl->m_pB = (u8 *)malloc(pMemsl->m_cB);
	pMemsl->m_fmemsl |= FMEMSL_ReadOnly;
	return pMemsl;
}

MemorySlice * PMemslConfigureUnmapped(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	auto pMemsl = IMemslAllocate(pMemmp, addrMin, addrMax);
	pMemsl->m_addrBase = addrMin;
	pMemsl->m_cB = addrMax - addrMin;
	pMemsl->m_fmemsl |= FMEMSL_Unmapped;
	return pMemsl;
}

void MapMirrored(MemoryMap * pMemmp, MemorySlice * pMemsl, u32 addrMin, u32 addrMax)
{
	VerifyMemorySpanClear(pMemmp, addrMin, addrMax);

	u8 iMemsl = U8Coerce(IFromP(pMemmp->m_aMemsl, FF_DIM(pMemmp->m_aMemsl), pMemsl));
	addrMax = min(addrMin, FF_DIM(pMemmp->m_mpAddrISlice));
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		pMemmp->m_mpAddrISlice[addrIt] = iMemsl;
	}
}
#endif

void TestMemoryMap(MemoryMap * pMemmp)
{
	// make sure all spans are mapped or unmapped
	for (u32 addr = 0; addr < kCBAddressable; ++addr)
	{
		FF_ASSERT(pMemmp->m_mpAddrISlice[addr] != 0, "unmapped region");
	}

	VerifyRom(pMemmp, 0x0000, 0x0800);
	VerifyMirror(pMemmp, 0x0000, 0x0800);
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
		auto pAmodinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

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

		printf("%s%s ", (pOpkinfo->m_opcat == OPCAT_Illegal) ? "  " : "!!", pOpinfo->m_pChzMnemonic);

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