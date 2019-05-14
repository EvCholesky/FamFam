#include "CPU.h"

#include <cstdlib> 
#include <stdio.h>
#include <string.h>

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

u8 U8PeekMem(MemoryMap * pMemmp, u16 addr)
{
	u8 * pB = pMemmp->m_mpAddrPB[addr];

	u8 b = *pB; 
	return b;
}

void WriteMemU8(MemoryMap * pMemmp, u16 addr, u8 b)
{
	pMemmp->m_bPrevBus = b;
	FMEM fmem = pMemmp->m_mpAddrFmem[addr];

	u8 bDummy;
	u8 * pB = ((fmem & FMEM_ReadOnly) == 0) ? pMemmp->m_mpAddrPB[addr] : &bDummy;
	*pB = b;
}

MemoryMap::MemoryMap()
:m_bPrevBus(0)
{
	ZeroAB(m_pBRaw, sizeof(m_pBRaw));
	ZeroAB(m_mpAddrPB, sizeof(m_mpAddrPB));
	ZeroAB(m_mpAddrFmem, sizeof(m_mpAddrFmem));
}

void ClearMemmp(MemoryMap * pMemmp)
{
	ZeroAB(pMemmp->m_pBRaw, sizeof(pMemmp->m_pBRaw));
	ZeroAB(pMemmp->m_mpAddrPB, sizeof(pMemmp->m_mpAddrPB));
	ZeroAB(pMemmp->m_mpAddrFmem, sizeof(pMemmp->m_mpAddrFmem));
}

void VerifyMemorySpanClear(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	// make sure this span is not overlapping any other spans

	addrMax = min<u32>(addrMax, kCBAddressable);
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		FF_ASSERT(pMemmp->m_mpAddrPB[addrIt] == nullptr);
		FF_ASSERT(pMemmp->m_mpAddrFmem[addrIt] == FMEM_None);
	}
}

void VerifyRam(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	for (u32 addr = addrMin; addr < addrMax; ++addr)
	{
		u8 nPrev = U8ReadMem(pMemmp, addr);
		u8 nTest = ~nPrev;
		WriteMemU8(pMemmp, addr, nTest);
		u8 nAfter = U8ReadMem(pMemmp, addr); 
		FF_ASSERT(nAfter == nTest, "cannot write to memory address $%04x", addr);
	}
}

void VerifyPrgRom(MemoryMap * pMemmp, u8 * pBPrgRom, u32 addrMin, u32 addrMax)
{
	u32 iB = 0;
	for (u32 addr = addrMin; addr < addrMax; ++addr, ++iB)
	{
		u8 b = U8ReadMem(pMemmp, addr);
		FF_ASSERT(b == pBPrgRom[iB], "rom map mismatch");	
	}
}

void VerifyRom(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	for (u32 addr = addrMin; addr < addrMax; ++addr)
	{
		u8 nPrev = U8ReadMem(pMemmp, addr);
		WriteMemU8(pMemmp, addr, ~nPrev);
		u8 nAfter = U8ReadMem(pMemmp, addr); 
		FF_ASSERT(nPrev == nAfter, "Wrote to read-only memory $%04x", addr);
	}
}

void VerifyMirror(MemoryMap * pMemmp, u32 addrBaseMin, u32 addrBaseMax, u32 addrMirrorMin, u32 addrMirrorMax)
{
	u8 b = 1;
	u32 cBBase = addrBaseMax - addrBaseMin;
	for (u32 addrIt = addrMirrorMin; addrIt < addrMirrorMax; ++addrIt)
	{
		u32 addrBase = addrBaseMin + ((addrIt - addrMirrorMin) % cBBase);
		u8 nBase = U8ReadMem(pMemmp, addrBase);
		u8 nMirror = U8ReadMem(pMemmp, addrIt);
		FF_ASSERT(nBase == nMirror, "mirrored read failed at $%04x", addrIt);

		b++;
		WriteMemU8(pMemmp, addrIt, b);
		u8 nBaseWrite = U8ReadMem(pMemmp, addrBase);
		FF_ASSERT(b == nBaseWrite, "mirrored write failed at $%04x", addrIt);

		b++;
		WriteMemU8(pMemmp, addrBase, b);
		u8 nMirrorWrite = U8ReadMem(pMemmp, addrIt);
		FF_ASSERT(b == nMirrorWrite, "mirrored write failed at $%04x", addrIt);
	}
}

AddressSpan AddrspMapMemory(MemoryMap * pMemmp, u32 addrMin, u32 addrMax, u8 fmem)
{
	VerifyMemorySpanClear(pMemmp, addrMin, addrMax);
	FF_ASSERT(addrMax > addrMin, "bad min/max addresses");
	FF_ASSERT(addrMin <= kCBAddressable && addrMin <= kCBAddressable, "address outside of range");

	fmem |= FMEM_Mapped;
	static_assert(sizeof(fmem) == 1, "expected one byte flags");
	memset(&pMemmp->m_mpAddrFmem[addrMin], fmem, addrMax - addrMin);

	u8 ** ppBMax = &pMemmp->m_mpAddrPB[addrMax];
	u8 * pBRaw = &pMemmp->m_pBRaw[addrMin];
	for (u8 ** ppB = &pMemmp->m_mpAddrPB[addrMin]; ppB != ppBMax; ++ppB, ++pBRaw)
	{
		*ppB = pBRaw;
	}

	AddressSpan addrsp;
	addrsp.m_addrMin = addrMin;
	addrsp.m_addrMax = addrMax;
	return addrsp;
}

AddressSpan AddrspMarkUnmapped(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	FF_ASSERT(addrMax > addrMin, "bad min/max addresses");
	FF_ASSERT(addrMin <= kCBAddressable && addrMin <= kCBAddressable, "address outside of range");

	FMEM fmem = FMEM_ReadOnly; // !unmapped
	static_assert(sizeof(fmem) == 1, "Expected one byte flags");
	memset(&pMemmp->m_mpAddrFmem[addrMin], fmem, addrMax - addrMin);

	// Unmapped memory maps the pB array at the last value left on the bus 
	// If we spend too much time setting / clearing this we could check a flag on memory writes, but that seems worse.

	u8 ** ppBMax = &pMemmp->m_mpAddrPB[addrMax];
	for (u8 ** ppB = &pMemmp->m_mpAddrPB[addrMin]; ppB != ppBMax; ++ppB)
	{
		*ppB = &pMemmp->m_bPrevBus;
	}

	AddressSpan addrsp;
	addrsp.m_addrMin = addrMin;
	addrsp.m_addrMax = addrMax;
	return addrsp;
}

void MapMirrored(MemoryMap * pMemmp, AddressSpan addrspBase, u32 addrMin, u32 addrMax, u8 fmem)
{
	fmem |= FMEM_Mapped;
	static_assert(sizeof(fmem) == 1, "expected one byte flags");
	memset(&pMemmp->m_mpAddrFmem[addrMin], fmem, addrMax - addrMin);
	
	u32 cBBase = max<u32>(1, addrspBase.m_addrMax - addrspBase.m_addrMin);
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		u32 dAddr = (addrIt - addrMin) % cBBase;
		pMemmp->m_mpAddrPB[addrIt] = pMemmp->m_mpAddrPB[addrspBase.m_addrMin + dAddr];
	}
}

void TestMemoryMap(MemoryMap * pMemmp)
{
	// make sure all spans are mapped or unmapped
	for (u32 addr = 0; addr < kCBAddressable; ++addr)
	{
		u8 fmem = pMemmp->m_mpAddrFmem[addr];
		FF_ASSERT(fmem != 0, "unhandled region at %04x. unmapped memory regions must be configured.", addr);
		if ((fmem & FMEM_Mapped) == 0)
		{
			FF_ASSERT(pMemmp->m_mpAddrPB[addr] = &pMemmp->m_bPrevBus, "unmapped memory should point at the previous bus value");
		}
	}

	VerifyRam(pMemmp, 0x0000, 0x0800);
	VerifyMirror(pMemmp, 0x0, 0x800, 0x800, 0x2000);

	VerifyRam(pMemmp, 0x2000, 0x2008);
	VerifyMirror(pMemmp, 0x2000, 0x2008, 0x2008, 0x4000);
}

void DumpAsm(Famicom * pFam, u16 addrMin, int cB)
{
	auto pMemmp = &pFam->m_memmp;
	u32 addrMax = addrMin + cB;
	u32 addr = addrMin;
	while (addr < addrMax)
	{
		u8 aB[3];
		aB[0] = U8PeekMem(pMemmp, addr);
		auto pOpinfo = POpinfoFromOpcode(aB[0]); 
		auto pOpkinfo = POpkinfoFromOPK(pOpinfo->m_opk);
		auto pAmodinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

		for (int iB=1; iB<pAmodinfo->m_cB; ++iB)
		{
			aB[iB] = U8PeekMem(pMemmp, addr + iB);
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