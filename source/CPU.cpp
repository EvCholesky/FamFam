#include "CPU.h"
#include "Platform.h"
#include "Rom.h"

#include <cstdlib> 
#include <stdio.h>
#include <string.h>

#define FILL_UNINITIALIZED_MEMORY 1
#define ZERO_UNINITIALIZED_MEMORY 0 

Famicom g_fam;
static const char s_chWildcard = '?';

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

void PokeMemU8(MemoryMap * pMemmp, u16 addr, u8 b)
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
	ZeroAB(m_aBRaw, sizeof(m_aBRaw));
	ZeroAB(m_mpAddrPB, sizeof(m_mpAddrPB));
	ZeroAB(m_mpAddrFmem, sizeof(m_mpAddrFmem));
}

void ClearMemmp(MemoryMap * pMemmp)
{
	ZeroAB(pMemmp->m_aBRaw, sizeof(pMemmp->m_aBRaw));
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
		u8 nPrev = U8PeekMem(pMemmp, addr);
		u8 nTest = ~nPrev;
		PokeMemU8(pMemmp, addr, nTest);
		u8 nAfter = U8PeekMem(pMemmp, addr); 
		FF_ASSERT(nAfter == nTest, "cannot write to memory address $%04x", addr);
	}
}

void VerifyPrgRom(MemoryMap * pMemmp, u8 * pBPrgRom, u32 addrMin, u32 addrMax)
{
	u32 iB = 0;
	for (u32 addr = addrMin; addr < addrMax; ++addr, ++iB)
	{
		u8 b = U8PeekMem(pMemmp, addr);
		FF_ASSERT(b == pBPrgRom[iB], "rom map mismatch");	
	}
}

void VerifyRom(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	for (u32 addr = addrMin; addr < addrMax; ++addr)
	{
		u8 nPrev = U8PeekMem(pMemmp, addr);
		PokeMemU8(pMemmp, addr, ~nPrev);
		u8 nAfter = U8PeekMem(pMemmp, addr); 
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
		u8 nBase = U8PeekMem(pMemmp, addrBase);
		u8 nMirror = U8PeekMem(pMemmp, addrIt);
		FF_ASSERT(nBase == nMirror, "mirrored read failed at $%04x", addrIt);

		b++;
		PokeMemU8(pMemmp, addrIt, b);
		u8 nBaseWrite = U8PeekMem(pMemmp, addrBase);
		FF_ASSERT(b == nBaseWrite, "mirrored write failed at $%04x", addrIt);

		b++;
		PokeMemU8(pMemmp, addrBase, b);
		u8 nMirrorWrite = U8PeekMem(pMemmp, addrIt);
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
	u8 * pBRaw = &pMemmp->m_aBRaw[addrMin];
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
			case AMOD_ZeroPageX: printf("$%02X,X", aB[1]);				break;
			case AMOD_ZeroPageY: printf("$%02X,Y", aB[1]);				break;
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

void SetPowerUpPreLoad(Famicom * pFam, u16 fpow)
{
	MemoryMap * pMemmp = &pFam->m_memmp;

#if FILL_UNINITIALIZED_MEMORY
	u8 nFill = 0; // KIL instruction
#elif ZERO_UNINITIALIZED_MEMORY
	u8 nFill = 0;
#endif

	if (fpow & FPOW_LogTest)
	{
		nFill = 0;
	}
	memset(pMemmp->m_aBRaw, nFill, sizeof(pMemmp->m_aBRaw));
}

void SetPowerUpState(Famicom * pFam, u16 fpow)
{
	auto pCpu = &pFam->m_cpu;
	auto pCreg = &pCpu->m_creg;

	//pCreg->m_p = FCPU_Break | FCPU_InterruptDisable | FCPU_Unused;
	pCreg->m_p = FCPU_InterruptDisable | FCPU_Unused;
	pCreg->m_a = 0;
	pCreg->m_x = 0;
	pCreg->m_y = 0;
	pCreg->m_sp = 0xFD;
	pCpu->m_cregPrev = *pCreg;

	MemoryMap * pMemmp = &pFam->m_memmp;
	PokeMemU8(pMemmp, 0x4017, 0); // Frame Irq enabled
	PokeMemU8(pMemmp, 0x4017, 0); // All channels disabled

	for (u16 addr = 0x4000; addr <= 0x400F; ++addr)
	{
		PokeMemU8(pMemmp, addr, 0);
	}

	u16 addrReset = U16ReadMem(pCpu, pMemmp, kAddrReset);
	pCreg->m_pc = addrReset;
	pCpu->m_cCycleCpu = 7;

	SetPpuPowerUpState(pFam, fpow);
}

void SetPpuPowerUpState(Famicom * pFam, u16 fpow)
{
	MemoryMap * pMemmp = &pFam->m_memmp;
	PokeMemU8(pMemmp, PPUREG_Ctrl, 0); // Frame Irq enabled
	PokeMemU8(pMemmp, PPUREG_Mask, 0); // All channels disabled

#if 0 // nope - this was actually nintendulator's weird debug read->0xFF quirk
	if ((fpow & FPOW_LogTest) != 0)
	{
		// For some reason nintendulator (incorrectly) starts the PPU registers at 0xFF
		// We're setting up raw values because the mapper hasn't been loaded yet

		u8 * aBRaw = pFam->m_memmp.m_aBRaw;
		PokeMemU8(pMemmp, PPUREG_Ctrl, 0xFF);
		PokeMemU8(pMemmp, PPUREG_Mask, 0xFF);
		PokeMemU8(pMemmp, PPUREG_Status, 0xFF);
	}
#endif
}

FF_FORCE_INLINE void AddCycleForPageBoundary(Cpu * pCpu, u16 nA, u16 nB)
{
	if ((nA & 0xFF00) != (nB & 0xFF00))
	{
		++pCpu->m_cCycleCpu;
	}
}

FF_FORCE_INLINE u16 NEvalAdrwIMP(Cpu * pCpu, MemoryMap * pMemmp)
{ 
	return 0; 
}

FF_FORCE_INLINE u16 NEvalAdrwABS(Cpu * pCpu, MemoryMap * pMemmp) 
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	return u16(nLow) | (u16(nHigh) << 8); 
}

FF_FORCE_INLINE u16 NEvalAdrwAXR(Cpu * pCpu, MemoryMap * pMemmp)
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u16 addr = (u16(nLow) | (u16(nHigh) << 8));
	u16 addrAdj = (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_x;
	AddCycleForPageBoundary(pCpu, addr, addrAdj); 
	return addrAdj;
}

FF_FORCE_INLINE u16 NEvalAdrwAYR(Cpu * pCpu, MemoryMap * pMemmp)
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u16 addr = (u16(nLow) | (u16(nHigh) << 8));
	u16 addrAdj = (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_y;
	AddCycleForPageBoundary(pCpu, addr, addrAdj); 
	return addrAdj;
}

FF_FORCE_INLINE u16 NEvalAdrwAXW(Cpu * pCpu, MemoryMap * pMemmp)
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	++pCpu->m_cCycleCpu;
	return (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_x; 
}

FF_FORCE_INLINE u16 NEvalAdrwAYW(Cpu * pCpu, MemoryMap * pMemmp)
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	++pCpu->m_cCycleCpu;
	return (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_y; 
}

FF_FORCE_INLINE u16 NEvalAdrwIMM(Cpu * pCpu, MemoryMap * pMemmp) 
{ 
	return U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
}

FF_FORCE_INLINE u16 NEvalAdrwIND(Cpu * pCpu, MemoryMap * pMemmp)
{
	u8 nLow = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u16 addr = u16(nLow) | (u16(nHigh) << 8);

	nLow = U8ReadMem(pCpu, pMemmp, addr); 
	nHigh = U8ReadMem(pCpu, pMemmp, addr+1);
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIXR(Cpu * pCpu, MemoryMap * pMemmp)
{
	u16 addr = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	addr = (addr + pCpu->m_creg.m_x) & 0x00FF;

	u8 nLow = U8ReadMem(pCpu, pMemmp, addr++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, addr); 
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIYR(Cpu * pCpu, MemoryMap * pMemmp)
{
	u16 addr = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 

	u8 nLow = U8ReadMem(pCpu, pMemmp, addr++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, addr); 

	return (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_y;
}

FF_FORCE_INLINE u16 NEvalAdrwIXW(Cpu * pCpu, MemoryMap * pMemmp)
{
	u16 addr = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	addr = (addr + pCpu->m_creg.m_x) & 0x00FF;

	u8 nLow = U8ReadMem(pCpu, pMemmp, addr++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, addr); 
	++pCpu->m_cCycleCpu;
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIYW(Cpu * pCpu, MemoryMap * pMemmp)
{
	u16 addr = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 

	u8 nLow = U8ReadMem(pCpu, pMemmp, addr++); 
	u8 nHigh = U8ReadMem(pCpu, pMemmp, addr); 
	++pCpu->m_cCycleCpu;
	return (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_y;
}

FF_FORCE_INLINE u16 NEvalAdrwREL(Cpu * pCpu, MemoryMap * pMemmp)
{
	s8 n = (s8)U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	u16 nReg = pCpu->m_creg.m_pc + n;
	return nReg;
}

FF_FORCE_INLINE u16 NEvalAdrwZPG(Cpu * pCpu, MemoryMap * pMemmp)
{ 
	return U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
}

FF_FORCE_INLINE u16 NEvalAdrwZPX(Cpu * pCpu, MemoryMap * pMemmp)
{ 
	u16 n = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	++pCpu->m_cCycleCpu;
	return (n + pCpu->m_creg.m_x) & 0xFF;
}

FF_FORCE_INLINE u16 NEvalAdrwZPY(Cpu * pCpu, MemoryMap * pMemmp)
{ 
	u16 n = U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++); 
	++pCpu->m_cCycleCpu;
	return (n + pCpu->m_creg.m_y) & 0xFF;
}

// illegal ops
FF_FORCE_INLINE void EvalOpAHX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpALR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpANC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpISC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpTAS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "Illegal op"); }


FF_FORCE_INLINE u8 NCarry(FCPU fcpu)
{
	static_assert(FCPU_Carry == 1, "need to shift carry");
	return u8(fcpu) & FCPU_Carry;
}

FF_FORCE_INLINE void SetCarryZeroOverflowNegative(CpuRegister * pCreg, u16 nOut)
{
}

FF_FORCE_INLINE void SetZeroNegative(CpuRegister * pCreg, u8 n)
{
	u8 p = pCreg->m_p & ~(FCPU_Zero | FCPU_Negative);
	p |= ((n == 0) ? FCPU_Zero: 0);

	// set FCPU_Negative(0x80) if the seventh bit is set
	static_assert(FCPU_Negative == 0x80, "negative flag changed?");
	p |= (n & 0x80);
	pCreg->m_p = p;
}

FF_FORCE_INLINE void EvalOpADC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	auto pCreg = &pCpu->m_creg;
	u8 b = U8ReadMem(pCpu, pMemmp, addr);
	u16 nOut = pCreg->m_a + b + NCarry(FCPU(pCreg->m_p));
	SetCarryZeroOverflowNegative(pCreg, nOut);

	pCreg->m_a = u8(nOut);
}

FF_FORCE_INLINE void TryBranch(Cpu * pCpu, bool predicate, u16 addrTrue)
{
	// we don't have the dummy read cycle or the page boundary cycle if the branch isn't taken
	if (!predicate)
		return;

	++pCpu->m_cCycleCpu;
	AddCycleForPageBoundary(pCpu, pCpu->m_creg.m_pc, addrTrue); 

	pCpu->m_creg.m_pc = addrTrue;
}

FF_FORCE_INLINE void EvalOpAND(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpARR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpASL(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpAXS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpBCC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpBCS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpBEQ(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }

FF_FORCE_INLINE void EvalOpBIT(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	u8 b = U8ReadMem(pCpu, pMemmp, addr);
	CpuRegister * pCreg = &pCpu->m_creg;
	pCreg->m_p &= ~(FCPU_Overflow | FCPU_Negative | FCPU_Zero);
	pCreg->m_p |= (b & pCreg->m_a & b) ? 0 : FCPU_Zero;
	pCreg->m_p |= (b & FCPU_Overflow) ? FCPU_Overflow : 0;
	pCreg->m_p |= (b & FCPU_Negative) ? FCPU_Negative : 0;
}

FF_FORCE_INLINE void EvalOpBNE(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	TryBranch(pCpu, (pCpu->m_creg.m_p & FCPU_Zero) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBMI(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	TryBranch(pCpu, (pCpu->m_creg.m_p & FCPU_Negative) != 0, addr);
}

FF_FORCE_INLINE void EvalOpBPL(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	TryBranch(pCpu, (pCpu->m_creg.m_p & FCPU_Negative) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBRK(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpBVC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	TryBranch(pCpu, (pCpu->m_creg.m_p & FCPU_Overflow) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBVS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	TryBranch(pCpu, (pCpu->m_creg.m_p & FCPU_Overflow) != 0, addr);
}

FF_FORCE_INLINE void EvalOpCLC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_p &= ~FCPU_Carry;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpCLD(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_p &= ~FCPU_DecimalMode;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpCLI(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_p &= ~FCPU_InterruptDisable;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpCLV(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_p &= ~FCPU_Overflow;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpCMP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	u8 b = s8(U8ReadMem(pCpu, pMemmp, addr));
	u8 dB = s8(pCpu->m_creg.m_a);

	SetZeroNegative(&pCpu->m_creg, dB);
	pCpu->m_creg.m_p |= (pCpu->m_creg.m_a >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpCPX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{ 
	u8 b = s8(U8ReadMem(pCpu, pMemmp, addr));
	u8 dB = s8(pCpu->m_creg.m_x);

	SetZeroNegative(&pCpu->m_creg, dB);
	pCpu->m_creg.m_p |= (pCpu->m_creg.m_x >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpCPY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	u8 b = s8(U8ReadMem(pCpu, pMemmp, addr));
	u8 dB = s8(pCpu->m_creg.m_y);

	SetZeroNegative(&pCpu->m_creg, dB);
	pCpu->m_creg.m_p |= (pCpu->m_creg.m_y >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpDCP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpDEC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpDEX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	--pCpu->m_creg.m_x;
	++pCpu->m_cCycleCpu;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_x);
}

FF_FORCE_INLINE void EvalOpDEY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	--pCpu->m_creg.m_y;
	++pCpu->m_cCycleCpu;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_y);
}

FF_FORCE_INLINE void EvalOpEOR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpINC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpINX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	++pCpu->m_creg.m_x;
	++pCpu->m_cCycleCpu;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_x);
}

FF_FORCE_INLINE void EvalOpINY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	++pCpu->m_creg.m_y;
	++pCpu->m_cCycleCpu;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_y);
}

FF_FORCE_INLINE void EvalOpJMP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_pc = addr;
}

FF_FORCE_INLINE u8 NLowByte(u16 nShort)
{
	return u8(nShort);
}

FF_FORCE_INLINE u8 NHighByte(u16 nShort)
{
	return u8(nShort >> 8);
}

FF_FORCE_INLINE void PushStack(Cpu * pCpu, MemoryMap * pMemmp, u8 b)
{
	return WriteMemU8(pCpu, pMemmp, pCpu->m_creg.m_sp-- | MEMSP_StackMin,  b);
}

FF_FORCE_INLINE u8 NPopStack(Cpu * pCpu, MemoryMap * pMemmp)
{
	return U8ReadMem(pCpu, pMemmp, ++pCpu->m_creg.m_sp | MEMSP_StackMin);
}

FF_FORCE_INLINE void EvalOpJSR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	u16 addrReturn = pCpu->m_creg.m_pc-1;
	++pCpu->m_cCycleCpu;
	PushStack(pCpu, pMemmp, NHighByte(addrReturn));
	PushStack(pCpu, pMemmp, NLowByte(addrReturn));
	pCpu->m_creg.m_pc = addr;
}

FF_FORCE_INLINE void EvalOpKIL(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpLAS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpLAX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpLDA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_a = u8(addr);
	SetZeroNegative(&pCpu->m_creg, u8(addr));
}

FF_FORCE_INLINE void EvalOpLDX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_x = u8(addr);
	SetZeroNegative(&pCpu->m_creg, u8(addr));
}

FF_FORCE_INLINE void EvalOpLDY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_y = u8(addr);
	SetZeroNegative(&pCpu->m_creg, u8(addr));
}

FF_FORCE_INLINE void EvalOpLSR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpNOP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpORA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpPHA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	PushStack(pCpu, pMemmp, pCpu->m_creg.m_a);
}

FF_FORCE_INLINE void EvalOpPHP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	PushStack(pCpu, pMemmp, pCpu->m_creg.m_p | FCPU_Unused | FCPU_Break);
}

FF_FORCE_INLINE void EvalOpPLA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	++pCpu->m_cCycleCpu;
	u8 a = NPopStack(pCpu, pMemmp);
	pCpu->m_creg.m_a;
	SetZeroNegative(&pCpu->m_creg, a);
}
FF_FORCE_INLINE void EvalOpPLP(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	++pCpu->m_cCycleCpu;
	SetZeroNegative(&pCpu->m_creg, NPopStack(pCpu, pMemmp));
}

FF_FORCE_INLINE void EvalOpRLA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpROL(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSAX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSBC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSEC(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_p |= FCPU_Carry;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpSED(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) 
{
	pCpu->m_creg.m_p |= FCPU_DecimalMode;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpSEI(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) 
{
	pCpu->m_creg.m_p |= FCPU_InterruptDisable;
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpSHX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSHY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSLO(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSRE(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpSTA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	WriteMemU8(pCpu, pMemmp, addr, pCpu->m_creg.m_a);
}

FF_FORCE_INLINE void EvalOpSTX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	WriteMemU8(pCpu, pMemmp, addr, pCpu->m_creg.m_x);
}

FF_FORCE_INLINE void EvalOpSTY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	WriteMemU8(pCpu, pMemmp, addr, pCpu->m_creg.m_y);
}

FF_FORCE_INLINE void EvalOpTAX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_x = pCpu->m_creg.m_a;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_a);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpTAY(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_y = pCpu->m_creg.m_a;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_a);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpTSX(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_x = pCpu->m_creg.m_sp;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_sp);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpTXA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_a = pCpu->m_creg.m_x;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_x);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpTXS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_sp = pCpu->m_creg.m_x;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_x);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpTYA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	pCpu->m_creg.m_a = pCpu->m_creg.m_y;
	SetZeroNegative(&pCpu->m_creg, pCpu->m_creg.m_y);
	++pCpu->m_cCycleCpu;
}

FF_FORCE_INLINE void EvalOpROR(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpRTI(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	EvalOpPLP(pCpu, pMemmp, addr);

	u8 nLow = NPopStack(pCpu, pMemmp);
	u8 nHigh = NPopStack(pCpu, pMemmp);
	pCpu->m_creg.m_pc = (nHigh << 0x8) | nLow;
}

FF_FORCE_INLINE void EvalOpRTS(Cpu * pCpu, MemoryMap * pMemmp, u16 addr)
{
	++pCpu->m_cCycleCpu;

	u16 nLow = NPopStack(pCpu, pMemmp);
	u16 nHigh = NPopStack(pCpu, pMemmp);
	++pCpu->m_cCycleCpu;
	pCpu->m_creg.m_pc = (nHigh << 0x8) | nLow;
	(void)U8ReadMem(pCpu, pMemmp, pCpu->m_creg.m_pc++);
}

FF_FORCE_INLINE void EvalOpRRA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }
FF_FORCE_INLINE void EvalOpXAA(Cpu * pCpu, MemoryMap * pMemmp, u16 addr) {FF_ASSERT(false, "TBD"); }

void StepCpu(Famicom * pFamicom)
{
	auto pCpu = &pFamicom->m_cpu;
	auto pMemmp = &pFamicom->m_memmp;
	u8 bOpcode = U8ReadMem(pFamicom, pCpu->m_creg.m_pc++);

#define OP(NAME, OPKIND, ADRW, CYCLE) case 0x##NAME: { u16 addr = NEvalAdrw##ADRW(pCpu, pMemmp); EvalOp##OPKIND(pCpu, pMemmp, addr); } break;
	switch (bOpcode)
	{
		OPCODE_TABLE
	}
#undef OP
}

inline u8 U8PeekMemNintendulator(MemoryMap * pMemmp, u16 addr)
{
	// nintendulator prints 0xFF for logs for these ranges
	if (addr >= MEMSP_PpuRegisterMin && addr <= MEMSP_PpuMirrorsMax)
		return 0xFF;

	return U8PeekMem(pMemmp, addr);
}

int CChPrintDisassmLine(const Cpu * pCpu, MemoryMap * pMemmp, u16 addr, char * pChz, int cChMax)
{
	auto pCreg = &pCpu->m_creg;

	u8 aB[3];
	aB[0] = U8PeekMem(pMemmp, addr);
	auto pOpinfo = POpinfoFromOpcode(aB[0]); 
	auto pOpkinfo = POpkinfoFromOPK(pOpinfo->m_opk);
	auto pAmodinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

	for (int iB=1; iB<pAmodinfo->m_cB; ++iB)
	{
		aB[iB] = U8PeekMem(pMemmp, addr + iB);
	}
	int cChWritten = 0;
	switch (pAmodinfo->m_cB)
	{
		case 1: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X        ", addr, aB[0]); break;
		case 2: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X %02X     ", addr, aB[0], aB[1]); break;
		case 3: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X %02X %02X  ", addr, aB[0], aB[1], aB[2]); break;
		default: FF_ASSERT(false, "unexpected opcode size");
	}

	pChz = pChz + cChWritten;
	cChMax = max(0, cChMax - cChWritten);

	if (cChMax == 0)
		return cChWritten;

	int cChOp = sprintf_s(pChz, cChMax, "%s ", pOpinfo->m_pChzMnemonic);

	cChWritten += cChOp;
	pChz = pChz + cChOp;
	cChMax = max(0, cChMax - cChOp);

	if (cChMax == 0)
		return cChWritten;

	// should match DecodeInstruction() in nintendulator
	switch(pAmodinfo->m_amod)
	{
		case AMOD_Implicit:
			break;
		case AMOD_Accumulator:
			break;
		case AMOD_Immediate:
			{
				cChWritten += sprintf_s(pChz, cChMax, "#$%02X ", aB[1]);				
			} break;
		case AMOD_Relative:
			{
				u16 addrRel = addr + pAmodinfo->m_cB + s8(aB[1]);
				cChWritten += sprintf_s(pChz, cChMax, "$%04X ", addrRel);
			} break;
		case AMOD_Absolute:	 
			{
				u16 addr = (u16(aB[2]) << 8) | u16(aB[1]);
				bool fPeekMem = (pOpinfo->m_opk != OPK_JMP) && (pOpinfo->m_opk != OPK_JSR);
				if (fPeekMem)
				{
					u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
					cChWritten += sprintf_s(pChz, cChMax, "$%04X = %02X", addr, nPeek);		
				}
				else
				{
					cChWritten += sprintf_s(pChz, cChMax, "$%04X", addr);		
				}
			} break;
		case AMOD_AbsoluteX: 
			{
				u16 addr = (u16(aB[1]) | (u16(aB[2]) << 8)) + pCpu->m_creg.m_x; 
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X%02X,X @ %04X = %02X", aB[2], aB[1], addr, nPeek);	
			} break;
		case AMOD_AbsoluteY: 
			{
				u16 addr = (u16(aB[1]) | (u16(aB[2]) << 8)) + pCpu->m_creg.m_y; 
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X%02X,Y @ %04X = %02X", aB[2], aB[1], addr, nPeek);	
			} break;
		case AMOD_ZeroPage:  
			{
				u16 addr = u16(aB[1]);
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$00%02X = %02X", aB[1], nPeek);
			} break;
		case AMOD_ZeroPageX:
			{
				u16 addr = (u16(pCreg->m_x) + aB[1]) & 0xFF;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X,X @ %02X = %02X", aB[1], addr, nPeek);				
			} break;
		case AMOD_ZeroPageY: 
			{
				u16 addr = (u16(pCreg->m_y) + aB[1]) & 0xFF;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X,Y @ %02X = %02X", aB[1], addr, nPeek);				
			} break;
		case AMOD_Indirect:
			{
				u8 nLow = U8PeekMem(pMemmp, aB[1]); 
				u8 nHigh = U8PeekMem(pMemmp, aB[2]);
				u16 addr = u16(nLow) | (u16(nHigh) << 8);

				u16 addrIndirect = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "($%04X) = %04X", addr, addrIndirect);	
			} break;
		case AMOD_IndirectX: 
			{
				u16 addrMid = (aB[1] + pCpu->m_creg.m_x) & 0x00FF;
				u16 addrEffective = U16PeekMem(pMemmp, addrMid);
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
			
				cChWritten += sprintf_s(pChz, cChMax, "($%02X,X) @ %02X = %04X = %02X", aB[1], addrMid, addrEffective, nPeek);
			} break;
		case AMOD_IndirectY:
			{
				u16 addrMid = U8PeekMem(pMemmp, aB[1]); 

				u8 nLow = U8PeekMem(pMemmp, addrMid); 
				u8 nHigh = U8PeekMem(pMemmp, addrMid+1); 

				u16 addrEffective = (u16(nLow) | (u16(nHigh) << 8)) + pCpu->m_creg.m_y;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);

				cChWritten += sprintf_s(pChz, cChMax, "($%02X),Y = %04X @ %04X = %02X", aB[1], addrMid, addrEffective, nPeek);
			} break;
		default:
			FF_ASSERT(false, "unhandled addressing mode.");
	}

	return cChWritten;
}

int CChPrintCpuState(const Cpu * pCpu, char * pChz, int cChMax)
{
	auto pCreg = &pCpu->m_creg;
	int iCh = sprintf_s(pChz, cChMax, "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:???,??? CYC:%I64d", 
		pCreg->m_a, pCreg->m_x, pCreg->m_y, pCreg->m_p, pCreg->m_sp, pCpu->m_cCycleCpu);
	return iCh;
}

int CChPadText(char * pChz, int cChMax, char chPad, int cChPad)
{
	if (cChMax <= 0)
		return 0;

	int cCh = min(cChMax - 1, cChPad);
	auto pChzMax = &pChz[cCh];
	auto pChzIt = pChz;
	for ( ; pChzIt != pChzMax; ++pChzIt)
	{
		*pChzIt = chPad;
	}

	*pChzIt = '\0';
	return int(pChzIt - pChz);
}

void PrintCpuStateForLog(const Cpu * pCpu, MemoryMap * pMemmp, char * pChz, int cChMax)
{
	// meant to match CPU logs from nintedulator

	if (cChMax <= 0)
		return;
	int cChAsm = CChPrintDisassmLine(pCpu, pMemmp, pCpu->m_creg.m_pc, pChz, cChMax);
	pChz += cChAsm;
	cChMax = max(0, cChMax - cChAsm);

	// pad to colunn 49
	int cChPad =  CChPadText(pChz, cChMax, ' ', 48 - cChAsm);
	pChz += cChPad;
	cChMax = max(0, cChMax - cChPad);

	if (cChMax < 0)
		return;

	(void)CChPrintCpuState(pCpu, pChz, cChMax);
}



bool FTryLoadLogFile(const char * pChzFilename, u8 ** ppB, size_t * pCB)
{
	FILE * pFile = nullptr;
	FF_FOPEN(pFile, pChzFilename, "r");

    if (fseek(pFile, 0, SEEK_END)) 
    {
       fclose(pFile); 
       return false; 
    }

    const sSize cBSigned = ftell(pFile);
    if (cBSigned == -1) 
    {
       fclose(pFile); 
       return false; 
    }

    uSize cB = (uSize)cBSigned;
    if (fseek(pFile, 0, SEEK_SET)) 
    {
       fclose(pFile); 
       return false; 
    }

    u8 * pB = (u8*)malloc(cB+1);
    cB = fread(pB, 1, cB, pFile);
    fclose(pFile);

    if (cB == 0)
    {
        free(pB);
        return false;
    }

	*ppB = pB;
	*pCB = cB;

   fclose(pFile);
   return true;
}

static bool FIsWhitespace(int ch)
{
   return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\f';
}

bool FDoLogsMatch(
	const char * pChzLocal,
	const char * pChzLocalMax,
	const char * pChzRef,
	const char * pChzRefMax,
	size_t * pCChAdvance)
{
	size_t cCh = 0;
	bool fMatch = true;
	while (pChzLocal != pChzLocalMax && pChzRef != pChzRefMax)
	{
		auto chLocal = *pChzLocal;
		auto chRef = *pChzRef;
		fMatch &= chLocal == s_chWildcard || chLocal == '\0' || chLocal == chRef;
		if (fMatch == false)
		{
			printf("");
		}

		if (chLocal = '\0' || chRef == '\0' || chRef == '\n')
			break;

		++cCh;
		++pChzLocal;
		++pChzRef;
	}

	while (pChzRef != pChzRefMax && FIsWhitespace(*pChzRef))
	{
		++cCh;
		++pChzRef;
	}

	*pCChAdvance = cCh;
	return fMatch;
}

bool FTryRunLogTest(const char * pChzFilenameRom, const char * pChzFilenameLog) 
{
	Famicom fam;
	Cart cart;
	fam.m_pCart = &cart;

	u16 fpow = FPOW_LogTest;
	SetPowerUpPreLoad(&fam, fpow);

	if (!FTryLoadRomFromFile(pChzFilenameRom, &cart, &fam.m_memmp))
	{
		ShowError("Could not load log test rom file '%s'", pChzFilenameRom);
		return false;
	}

	SetPowerUpState(&fam, fpow);

	u8 * pBLog = nullptr;
	size_t cBLog = 0;
	if (!FTryLoadLogFile(pChzFilenameLog, &pBLog, &cBLog))
	{
		ShowError("Could not load log file '%s'", pChzFilenameLog);
		return false;
	}

	auto pBLogIt = (char *)pBLog;
	auto pBLogMax = (char*)&pBLog[cBLog];

	bool fPassTest = true;
	char aChState[128];
	char aChLog[128];
	int cLineLog = 0;
	int cStep = 0;
	while (pBLogIt != pBLogMax)
	{
		PrintCpuStateForLog(&fam.m_cpu, &fam.m_memmp, aChState, FF_DIM(aChState));
		size_t cChAdvance = 0;
		if (!FDoLogsMatch(aChState, FF_PMAX(aChState), pBLogIt, pBLogMax, &cChAdvance))
		{
		StepCpu(&fam);
			size_t cChState = strlen(aChState);
			strncpy_s(aChLog, FF_DIM(aChLog), pBLogIt, cChState);
			ShowError("Cpu emulation does not match log:\nemu = '%s'\nlog = '%s'\n", aChState, aChLog);
			fPassTest = false;
			break;
		}
		printf("%s\n", aChState);

		pBLogIt += cChAdvance;

		StepCpu(&fam);
	}

	return fPassTest;
}

bool FTryAllLogTests()
{
	struct LogTest
	{
		const char *	m_pChzPath;
		const char *	m_pChzRom;
		const char *	m_pChzLog;
	};

	static const LogTest s_aLogtest[] = {
		{"test\\nes_instr_test", "zero_page.nes", "zero_page.cpulog"}
	};

	char aChRom[256];
	char aChLog[256];
	
	for (int iLogtest = 0; iLogtest < FF_DIM(s_aLogtest); ++iLogtest)
	{
		const LogTest & logtest = s_aLogtest[iLogtest];
		sprintf_s(aChRom, FF_DIM(aChRom), "%s\\%s", logtest.m_pChzPath, logtest.m_pChzRom);
		sprintf_s(aChLog, FF_DIM(aChRom), "%s\\%s", logtest.m_pChzPath, logtest.m_pChzLog);

		if (!FTryRunLogTest(aChRom, aChLog))
		{
			ShowError("log test %d failed", iLogtest);
			return false;
		}
	}
	return true;
}

