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
bool s_fTraceCpu = false;



inline MapperMMC1 * PMapr1(Famicom * pFam)
{
	FF_ASSERT(pFam->m_pCart->m_mapperk == MAPPERK_MMC1, "casting to wrong mapper type");
	return (MapperMMC1 *)pFam->m_pVMapr;
}

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
#define OP(NAME, OPKIND, ADDR, CYCLE) {#OPKIND, OPK_##OPKIND, AMRW_##ADDR, 0x##NAME, S8Coerce(abs(CYCLE)), true, CYCLE < 0},
#define IL(NAME, OPKIND, ADDR, CYCLE) {#OPKIND, OPK_##OPKIND, AMRW_##ADDR, 0x##NAME, S8Coerce(abs(CYCLE)), false, CYCLE < 0},
#define OPA(NAME, OPKIND, ADDR, CYCLE) {#OPKIND, OPK_##OPKIND, AMRW_##ADDR, 0x##NAME, S8Coerce(abs(CYCLE)), true, CYCLE < 0},
	static const OpInfo s_mpOpcodeOpinfo[] =
	{
		OPCODE_TABLE
	};
#undef OPA
#undef IL
#undef OP

	return &s_mpOpcodeOpinfo[nOpcode];
}

u8 U8ReadOpenBus(Famicom * pFam, u16 addr)
{
	return pFam->m_memmp.m_bPrevBusPpu;
}

void WriteOpenBus(Famicom * pFam, u16 addr, u8 b)
{
	auto pMemmp = &pFam->m_memmp;
	pMemmp->m_bPrevBus = b;
}


int IPpucresAllocate(PpuCommandList * pPpucl)
{
	for (int iPpucresIt = 0; iPpucresIt != FF_DIM(pPpucl->m_aPpucres); ++iPpucresIt)
	{
		auto pPpucres = &pPpucl->m_aPpucres[iPpucresIt];
		if (pPpucres->m_fInUse == false)
		{
			pPpucres->m_fInUse = true;
			return iPpucresIt;
		}
	}

	FF_ASSERT(false, "no more result slots");
	return -1;
}

u8 U8ReadPpuStatus(Famicom * pFam, u16 addr)
{
    auto pMemmp = &pFam->m_memmp;

	// There's a tricky problem that happens here... we want to read the value of PPUREG_Status as the ppu executes to this 
	//  cycle, but the ppu will clear the NMI flag after it's read.
	PpuCommandList * pPpucl = &pFam->m_ppucl;
	int iPpucres = IPpucresAllocate(pPpucl);

	bool fNmiStarted = pFam->m_ppu.m_pstatus.m_fVerticalBlankStarted;

    AppendPpuCommand(pPpucl, PCMDK_Read, addr, 0, pFam->m_ptimCpu, iPpucres, pFam->m_cpu.m_pc);
	UpdatePpu(pFam, pFam->m_ptimCpu);
	
	auto pPpucres = &pPpucl->m_aPpucres[iPpucres];
	u8 bStatus = pPpucres->m_b;
	pPpucres->m_fInUse = false;
    pMemmp->m_aBRaw[PPUREG_Status] = bStatus & ~0x80;

    u8 bRet = pMemmp->m_bPrevBusPpu & 0x1F;
    bRet |= (bStatus & 0xE0);

    pMemmp->m_bPrevBusPpu = bRet;
	return bRet;
}

u8 U8ReadPpuReg(Famicom * pFam, u16 addr)
{
	PpuCommandList * pPpucl = &pFam->m_ppucl;
	auto pMemmp = &pFam->m_memmp;
	u8 b;

	// if this is a ppu register that is updated by the ppu we need to simulate the ppu forward
	if (addr == PPUREG_PpuData)
	{
		int iPpucres = IPpucresAllocate(pPpucl);
		AppendPpuCommand(pPpucl, PCMDK_Read, addr, 0, pFam->m_ptimCpu, iPpucres, pFam->m_cpu.m_pc);
		UpdatePpu(pFam, pFam->m_ptimCpu);

		auto pPpucres = &pPpucl->m_aPpucres[iPpucres];
		b = pPpucres->m_b;
		pPpucres->m_fInUse = false;
	}
	else
	{
		AppendPpuCommand(pPpucl, PCMDK_Read, addr, 0, pFam->m_ptimCpu, -1, pFam->m_cpu.m_pc);

		u8 * pB = pMemmp->m_mpAddrPB[addr];
		b = *pB; 
	}

	pMemmp->m_bPrevBusPpu = b;
	return b;
}

void WritePpuReg(Famicom * pFam, u16 addr, u8 b)
{
	auto pMemmp = &pFam->m_memmp;
	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];

	AppendPpuCommand(&pFam->m_ppucl, PCMDK_Write, addr, b, pFam->m_ptimCpu, -1, pFam->m_cpu.m_pc);

	pMemmp->m_bPrevBusPpu = b;
	u8 bDummy;
	u8 * pB = ((memdesc.m_fmem & FMEM_ReadOnly) == 0) ? pMemmp->m_mpAddrPB[addr] : &bDummy;
	*pB = b;
}

u8 U8ReadControllerReg(Famicom * pFam, u16 addr)
{
	int iGpad  = (addr == 0x4017) ? 1 : 0;
	Gamepad * pGpad = &pFam->m_famin.m_aGpad[iGpad];

	EDGES edges = pGpad->m_mpButkEdges[pGpad->m_iBShift++];

	u8 b = (edges >= EDGES_Press) ? 1 : 0;
	return b;
}

void DumpOam(Famicom * pFam)
{
	OAM * pOamMax = FF_PMAX(pFam->m_ppu.m_aOam);
	int i = 0;
	for (OAM * pOam = pFam->m_ppu.m_aOam; pOam != pOamMax; ++pOam, ++i)
	{
		if (pOam->m_yTop >= 0xEF)
			continue;

		printf("%2d: tile=%02X, (%d, %d) pal=%dx pri=%d %s%s\n",
			i,
			pOam->m_iTile,
			pOam->m_xLeft, pOam->m_yTop,
			pOam->m_palette,
			pOam->m_fBgPriority,
			(pOam->m_fFlipHoriz)? "flipH,":"",
			(pOam->m_fFlipVert) ? "flipV,":"");
	}
}

void ExecuteOamDma(Famicom * pFam, u8 b)
{
	TickCpu(pFam);

	// +1 extra cycle if we're on an odd cycle count
	if (pFam->m_cpu.m_tickc & 0x1)
	{
		++pFam->m_cpu.m_tickc;
	}

	// 256 read/write pairs
	pFam->m_cpu.m_tickc += 512;

	// Start at the OAM addr and wrap around
	u8 bOamAddr = pFam->m_ppu.m_bOamAddr;
	auto pBOam = (u8 *)pFam->m_ppu.m_aOam;

	size_t cBWrap = sizeof(pFam->m_ppu.m_aOam) - bOamAddr;
	u16 addrStart = u16(b) << 8;

	auto pMemmp = &pFam->m_memmp;
	u8 * pBSource = pMemmp->m_mpAddrPB[addrStart];
#if _DEBUG
	// BB: This will fail if the source span mapped here is not contiguous for 256 bytes.
	for (int iB = 0; iB < 256; ++iB)
	{
		FF_ASSERT(pBSource + iB == pMemmp->m_mpAddrPB[addrStart+iB], "sprite source is not contiguous, I guess we have to do it the slow way");
	}
#endif	

	CopyAB(pBSource, &pBOam[bOamAddr], cBWrap);

	if (bOamAddr != 0)
	{
		CopyAB(pBSource + cBWrap, pBOam, bOamAddr);
	}

	//DumpOam(pFam);
}

void WriteOamDmaRegister(Famicom * pFam, u16 addr, u8 b)
{
	UpdatePpu(pFam, pFam->m_ptimCpu);
	ExecuteOamDma(pFam, b);

	auto pMemmp = &pFam->m_memmp;
	pMemmp->m_bPrevBusPpu = b;
}

void WriteControllerLatch(Famicom * pFam, u16 addr, u8 b)
{
	FamicomInput * pFamin = &pFam->m_famin;
	u8 nLatchPrev = pFamin->m_nControllerLatch;
	pFamin->m_nControllerLatch = b & 0x7;

	if ((nLatchPrev & 0x1) == 0 && (pFamin->m_nControllerLatch & 0x1) != 0)
	{
		KeyPressState * pKeyps = pFam->m_pKeyps;

		int iGpad = 0;
		auto pGpadMax = FF_PMAX(pFamin->m_aGpad);
		for (Gamepad * pGpad = pFamin->m_aGpad; pGpad != pGpadMax; ++pGpad, ++iGpad)
		{
			pGpad->m_iBShift = 0;

			for (int butk = BUTK_Min; butk < BUTK_Max; ++butk)
			{
				KEYCODE keycode0 = KEYCODE(pGpad->m_mpButkKeycode0[butk]);
				KEYCODE keycode1 = KEYCODE(pGpad->m_mpButkKeycode1[butk]);
				EDGES edges0 = pKeyps->m_mpKeycodeEdges[keycode0];
				EDGES edges1 = pKeyps->m_mpKeycodeEdges[keycode1];
				pGpad->m_mpButkEdges[butk] = ffMax(edges0, edges1);
			}
		}
	}

	auto pMemmp = &pFam->m_memmp;
	pMemmp->m_bPrevBusPpu = b;
}

void WriteRomMmc1(Famicom * pFam, u16 addr, u8 b)
{
	if (addr > 0x8000)
	{
		auto pMapr1 = PMapr1(pFam);
		if (b & 0x80)
		{
			// clear the shift register
			pMapr1->m_cBitShift = 0;
			pMapr1->m_bShift = 0;
		}
		else
		{
			// shift bit 0 into the register
			++pMapr1->m_cBitShift;
			pMapr1->m_bShift = ((b & 0x1) << 4) | (pMapr1->m_bShift >> 1);

			if (pMapr1->m_cBitShift > 4)
			{
				int iBReg = (addr >> 13) & 0x3; // register selected by bits 13..14 of the address for the fifth write
				pMapr1->m_aBReg[iBReg] = pMapr1->m_bShift;
				((u8*)&pMapr1->m_nReg)[iBReg] = pMapr1->m_bShift;
				pMapr1->m_cBitShift = 0;
				pMapr1->m_bShift = 0;

				UpdateBanks(pFam, pMapr1);
			}
		}
	}

	// basic write mem inlined without the callback checks
	auto pMemmp = &pFam->m_memmp;
	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];
	pMemmp->m_bPrevBus = b;

	TickCpu(pFam);
}

u8 U8PeekMem(MemoryMap * pMemmp, u16 addr)
{
	// NOTE: peek memory will not call memory callback routines
	u8 * pB = pMemmp->m_mpAddrPB[addr];

	u8 b = *pB; 
	pMemmp->m_bPrevBus = b;
	return b;
}

void PokeMemU8(MemoryMap * pMemmp, u16 addr, u8 b)
{
	// NOTE: poke memory will not call memory callback routines
	pMemmp->m_bPrevBus = b;

	u8 bDummy;
	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];
	u8 * pB = ((memdesc.m_fmem & FMEM_ReadOnly) == 0) ? pMemmp->m_mpAddrPB[addr] : &bDummy;
	*pB = b;
}

MemoryMap::MemoryMap()
:m_bPrevBus(0)
,m_bPrevBusPpu(0)
{
	ZeroAB(m_aBRaw, sizeof(m_aBRaw));
	ZeroAB(m_mpAddrPB, sizeof(m_mpAddrPB));
	ZeroAB(m_mpAddrMemdesc, sizeof(m_mpAddrMemdesc));

	// waste a few bytes to allow index zero to indicate no callbacks
	MemoryCallback memcb = {nullptr, nullptr};
	m_aryMemcb.Append(memcb);
}

void ClearMemmp(MemoryMap * pMemmp)
{
	ZeroAB(pMemmp->m_aBRaw, sizeof(pMemmp->m_aBRaw));
	ZeroAB(pMemmp->m_mpAddrPB, sizeof(pMemmp->m_mpAddrPB));
	ZeroAB(pMemmp->m_mpAddrMemdesc, sizeof(pMemmp->m_mpAddrMemdesc));
}

void VerifyMemorySpanClear(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	// make sure this span is not overlapping any other spans

	addrMax = ffMin<u32>(addrMax, kCBAddressable);
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		FF_ASSERT(pMemmp->m_mpAddrPB[addrIt] == nullptr);
		FF_ASSERT(pMemmp->m_mpAddrMemdesc[addrIt].m_fmem == FMEM_None);
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

AddressSpan AddrspMapMemory(MemoryMap * pMemmp, u32 addrMin, u32 addrMax, MemoryDescriptor * aMemdesc, int cMemdesc)
{
	VerifyMemorySpanClear(pMemmp, addrMin, addrMax);
	FF_ASSERT(addrMax > addrMin, "bad min/max addresses");
	FF_ASSERT(addrMin <= kCBAddressable && addrMin <= kCBAddressable, "address outside of range");

	MemoryDescriptor memdesc;
	if (cMemdesc == 0)
	{
		FF_ASSERT(aMemdesc == nullptr, "cMemdesc is zero, but passed in an array of descriptors");
		cMemdesc = 1;
		aMemdesc = &memdesc;
	}

	{
		auto pMemdescMax = &aMemdesc[cMemdesc];
		for (auto pMemdescIt = aMemdesc; pMemdescIt != pMemdescMax; ++pMemdescIt)
		{
			auto pMemcb = &pMemmp->m_aryMemcb[pMemdescIt->m_iMemcb];
			pMemdescIt->m_fmem |=	((pMemcb->m_pFnReadmem) ? FMEM_HasReadCb : 0) |
									((pMemcb->m_pFnWritemem) ? FMEM_HasWriteCb : 0);
		}
	}

	int iMemdesc = 0;
	auto pMemdescMax = &pMemmp->m_mpAddrMemdesc[addrMax];
	for (auto pMemdescIt = &pMemmp->m_mpAddrMemdesc[addrMin]; pMemdescIt != pMemdescMax; ++pMemdescIt)
	{
		*pMemdescIt = aMemdesc[iMemdesc];
		pMemdescIt->m_fmem |= FMEM_Mapped;
		iMemdesc = (iMemdesc + 1) % cMemdesc;
	}

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

	MemoryDescriptor memdesc(FMEM_ReadOnly);
	auto pMemdescMax = &pMemmp->m_mpAddrMemdesc[addrMax];
	for (auto pMemdescIt = &pMemmp->m_mpAddrMemdesc[addrMin]; pMemdescIt != pMemdescMax; ++pMemdescIt)
	{
		*pMemdescIt = memdesc;
	}

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

void UnmapMemory(MemoryMap * pMemmp, u32 addrMin, u32 addrMax)
{
	u8 ** ppBMax = &pMemmp->m_mpAddrPB[addrMax];
	for (auto ppB = &pMemmp->m_mpAddrPB[addrMin]; ppB != ppBMax; ++ppB)
	{
		*ppB = nullptr;
	}

	// NOTE: This function may be passed empty spans where addrdMin == addrMax
	auto pMemdescMax = &pMemmp->m_mpAddrMemdesc[addrMax];
	for (auto pMemdescIt = &pMemmp->m_mpAddrMemdesc[addrMin]; pMemdescIt != pMemdescMax; ++pMemdescIt)
	{
		*pMemdescIt = MemoryDescriptor();
	}
}

void MapMirrored(MemoryMap * pMemmp, AddressSpan addrspBase, u32 addrMin, u32 addrMax, const MemoryDescriptor * aMemdesc, int cMemdesc)
{
	MemoryDescriptor memdesc;
	if (cMemdesc == 0)
	{
		cMemdesc = 1;
		aMemdesc = &memdesc;
	}

	int iMemdesc = 0;
	auto pMemdescMax = &pMemmp->m_mpAddrMemdesc[addrMax];
	for (auto pMemdescIt = &pMemmp->m_mpAddrMemdesc[addrMin]; pMemdescIt != pMemdescMax; ++pMemdescIt)
	{
		*pMemdescIt = aMemdesc[iMemdesc];
		pMemdescIt->m_fmem |= FMEM_Mapped;
		iMemdesc = (iMemdesc + 1) % cMemdesc;
	}
	
	u32 cBBase = ffMax<u32>(1, addrspBase.m_addrMax - addrspBase.m_addrMin);
	for (u32 addrIt = addrMin; addrIt < addrMax; ++addrIt)
	{
		u32 dAddr = (addrIt - addrMin) % cBBase;
		pMemmp->m_mpAddrPB[addrIt] = pMemmp->m_mpAddrPB[addrspBase.m_addrMin + dAddr];
	}
}

u8 IMemcbAllocate(MemoryMap * pMemmp, PFnReadMemCallback pFnRead, PFnWriteMemCallback pFnWrite) 
{
	u8 iMemcb = U8Coerce(pMemmp->m_aryMemcb.C());
	auto pMemcb = pMemmp->m_aryMemcb.AppendNew();
	pMemcb->m_pFnReadmem = pFnRead;
	pMemcb->m_pFnWritemem = pFnWrite;
	return iMemcb;
}

void TestMemoryMap(MemoryMap * pMemmp)
{
	// make sure all spans are mapped or unmapped
	for (u32 addr = 0; addr < kCBAddressable; ++addr)
	{
		auto pMemdesc = &pMemmp->m_mpAddrMemdesc[addr];
		u8 fmem = pMemdesc->m_fmem;
		FF_ASSERT(fmem != 0, "unhandled region at %04x. unmapped memory regions must be configured.", addr);
		if ((fmem & FMEM_Mapped) == 0)
		{
			FF_ASSERT(pMemmp->m_mpAddrPB[addr] = &pMemmp->m_bPrevBus, "unmapped memory should point at the previous bus value");
		}

		if (pMemdesc->m_iMemcb)
		{
			auto pMemcb = &pMemmp->m_aryMemcb[pMemdesc->m_iMemcb];
			FF_ASSERT((pMemcb->m_pFnReadmem==nullptr) == ((pMemdesc->m_fmem & FMEM_HasReadCb) != 0), "missing read cb flag");
			FF_ASSERT((pMemcb->m_pFnWritemem==nullptr) == ((pMemdesc->m_fmem & FMEM_HasWriteCb) != 0), "missing write cb flag");
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

		printf("%s%s ", (pOpinfo->m_fIsOfficial) ? "  " : "!!", pOpinfo->m_pChzMnemonic);

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
	pCpu->m_p = FCPU_InterruptDisable | FCPU_Unused;
	pCpu->m_a = 0;
	pCpu->m_x = 0;
	pCpu->m_y = 0;
	pCpu->m_sp = 0xFD;


	MemoryMap * pMemmp = &pFam->m_memmp;
	PokeMemU8(pMemmp, 0x4017, 0); // Frame Irq enabled
	PokeMemU8(pMemmp, 0x4017, 0); // All channels disabled

	for (u16 addr = 0x4000; addr <= 0x400F; ++addr)
	{
		PokeMemU8(pMemmp, addr, 0);
	}

	// Note: Peeking memory here to prevent the reset memory reads counting PPU cycles
	u16 addrReset = U16PeekMem(&pFam->m_memmp, kAddrReset);
	pCpu->m_pc = addrReset;

	// Reset: 
	// (1,2)	Fetch two unused instruction bytes, 
	// (3,4,5)	Push pc(high, low) on the stack, push P on the stack...
	// (6,7)	Read reset vector into m_pc
	pCpu->m_tickc = 7;
	pFam->m_cpuPrev = *pCpu;

	PpuTiming * pPtim = &pFam->m_ptimCpu;
	pPtim->m_tickcPrev = pCpu->m_tickc;

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

FF_FORCE_INLINE void AddCycleForPageBoundary(Famicom * pFam, u16 nA, u16 nB)
{
	if ((nA & 0xFF00) != (nB & 0xFF00))
	{
		TickCpu(pFam);
	}
}

FF_FORCE_INLINE u16 NEvalAdrwIMP(Famicom * pFam)
{ 
	return 0; 
}

FF_FORCE_INLINE u16 NEvalAdrwACC(Famicom * pFam)
{ 
	return 0; 
}

FF_FORCE_INLINE u16 NEvalAdrwABS(Famicom * pFam) 
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	return u16(nLow) | (u16(nHigh) << 8); 
}

FF_FORCE_INLINE u16 NEvalAdrwAXR(Famicom * pFam)
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u16 addr = (u16(nLow) | (u16(nHigh) << 8));
	u16 addrAdj = (u16(nLow) | (u16(nHigh) << 8)) + pFam->m_cpu.m_x;
	AddCycleForPageBoundary(pFam, addr, addrAdj); 
	return addrAdj;
}

FF_FORCE_INLINE u16 NEvalAdrwAYR(Famicom * pFam)
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u16 addr = (u16(nLow) | (u16(nHigh) << 8));
	u16 addrAdj = (u16(nLow) | (u16(nHigh) << 8)) + pFam->m_cpu.m_y;
	AddCycleForPageBoundary(pFam, addr, addrAdj); 
	return addrAdj;
}

FF_FORCE_INLINE u16 NEvalAdrwAXW(Famicom * pFam)
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	return (u16(nLow) | (u16(nHigh) << 8)) + pFam->m_cpu.m_x; 
}

FF_FORCE_INLINE u16 NEvalAdrwAYW(Famicom * pFam)
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	return (u16(nLow) | (u16(nHigh) << 8)) + pFam->m_cpu.m_y; 
}

FF_FORCE_INLINE u16 NEvalAdrwIMM(Famicom * pFam) 
{ 
	return pFam->m_cpu.m_pc++; 
}

FF_FORCE_INLINE u16 NEvalAdrwIND(Famicom * pFam)
{
	u8 nLow = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u8 nHigh = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u16 addr = u16(nLow) | (u16(nHigh) << 8);

	u16 addrHigh = addr & 0xFF00;
	nLow = U8ReadMem(pFam, addr); 
	nHigh = U8ReadMem(pFam, addrHigh | ((addr+1) & 0xFF));
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIXR(Famicom * pFam)
{
	u16 addr = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	addr = (addr + pFam->m_cpu.m_x) & 0x00FF;

	u8 nLow = U8ReadMem(pFam, addr); 
	u8 nHigh = U8ReadMem(pFam, (addr + 1) & 0xFF); 
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIYR(Famicom * pFam)
{
	u16 addr = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 

	u8 nLow = U8ReadMem(pFam, addr); 
	u8 nHigh = U8ReadMem(pFam, (addr + 1) & 0xFF); 
	u16 addrBase = (u16(nLow) | (u16(nHigh) << 8));
	u16 addrIndex = addrBase + pFam->m_cpu.m_y;
	AddCycleForPageBoundary(pFam, addrBase, addrIndex); 
	return addrIndex;
}

FF_FORCE_INLINE u16 NEvalAdrwIXW(Famicom * pFam)
{
	u16 addr = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	addr = (addr + pFam->m_cpu.m_x) & 0x00FF;

	u8 nLow = U8ReadMem(pFam, addr); 
	u8 nHigh = U8ReadMem(pFam, (addr + 1) & 0xFF); 
	return u16(nLow) | (u16(nHigh) << 8);
}

FF_FORCE_INLINE u16 NEvalAdrwIYW(Famicom * pFam)
{
	u16 addr = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 

	u8 nLow = U8ReadMem(pFam, addr); 
	u8 nHigh = U8ReadMem(pFam, (addr + 1) & 0xFF); 
	TickCpu(pFam);
	return (u16(nLow) | (u16(nHigh) << 8)) + pFam->m_cpu.m_y;
}

FF_FORCE_INLINE u16 NEvalAdrwREL(Famicom * pFam)
{
	s8 n = (s8)U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	u16 nReg = pFam->m_cpu.m_pc + n;
	return nReg;
}

FF_FORCE_INLINE u16 NEvalAdrwZPG(Famicom * pFam)
{ 
	return U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
}

FF_FORCE_INLINE u16 NEvalAdrwZPX(Famicom * pFam)
{ 
	u16 n = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	return (n + pFam->m_cpu.m_x) & 0xFF;
}

FF_FORCE_INLINE u16 NEvalAdrwZPY(Famicom * pFam)
{ 
	u16 n = U8ReadMem(pFam, pFam->m_cpu.m_pc++); 
	TickCpu(pFam);
	return (n + pFam->m_cpu.m_y) & 0xFF;
}



FF_FORCE_INLINE u8 NCarry(FCPU fcpu)
{
	static_assert(FCPU_Carry == 1, "need to shift carry");
	return u8(fcpu) & FCPU_Carry;
}

FF_FORCE_INLINE void SetCarryZeroOverflowNegative(Cpu * pCpu, u16 n)
{
	u8 p = pCpu->m_p & ~(FCPU_Carry | FCPU_Zero | FCPU_Overflow | FCPU_Negative);
	p |= ((n == 0) ? FCPU_Zero: 0);

	// BB - not setting carry or negative yet!

	// set FCPU_Negative(0x80) if the seventh bit is set
	static_assert(FCPU_Negative == 0x80, "negative flag changed?");
	p |= (n & 0x80);
	pCpu->m_p = p;
}

FF_FORCE_INLINE void SetZeroNegative(Cpu * pCpu, u8 n)
{
	u8 p = pCpu->m_p & ~(FCPU_Zero | FCPU_Negative);
	p |= ((n == 0) ? FCPU_Zero: 0);

	// set FCPU_Negative(0x80) if the seventh bit is set
	static_assert(FCPU_Negative == 0x80, "negative flag changed?");
	p |= (n & 0x80);
	pCpu->m_p = p;
}

FF_FORCE_INLINE void EvalOpADC(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	s16 nSum = pFam->m_cpu.m_a + b + NCarry(FCPU(pFam->m_cpu.m_p));

	pFam->m_cpu.m_p &= ~(FCPU_Carry | FCPU_Overflow);
	pFam->m_cpu.m_p |= (nSum & 0xFF00) ? FCPU_Carry : 0;
	pFam->m_cpu.m_p |= ((( pFam->m_cpu.m_a ^ nSum ) & ( b ^ nSum )) & 0x80 ) ? FCPU_Overflow : 0;	// did the sign bit change?
	SetZeroNegative(&pFam->m_cpu, u8(nSum));

	pFam->m_cpu.m_a = u8(nSum);
}

FF_FORCE_INLINE void EvalOpSBC(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	s16 nDif = pFam->m_cpu.m_a - b - (1 - NCarry(FCPU(pFam->m_cpu.m_p)));

	pFam->m_cpu.m_p &= ~(FCPU_Carry | FCPU_Overflow);
	pFam->m_cpu.m_p |= (nDif & 0xFF00) ? 0 : FCPU_Carry;  // carry is set if result is outside 0..255
	pFam->m_cpu.m_p |= ((( pFam->m_cpu.m_a ^ b ) & ( pFam->m_cpu.m_a ^ nDif )) & 0x80 ) ? FCPU_Overflow : 0;  // did the sign bit change?
	SetZeroNegative(&pFam->m_cpu, u8(nDif));

	pFam->m_cpu.m_a = u8(nDif);
}

FF_FORCE_INLINE void TryBranch(Famicom * pFam, bool predicate, u16 addrTrue)
{
	// we don't have the dummy read cycle or the page boundary cycle if the branch isn't taken
	if (!predicate)
		return;

	TickCpu(pFam);
	AddCycleForPageBoundary(pFam, pFam->m_cpu.m_pc, addrTrue); 
	pFam->m_cpu.m_pc = addrTrue;
}

FF_FORCE_INLINE void EvalOpAND(Famicom * pFam, u16 addr)
{
	u8 bOpcode = U8ReadMem(pFam, addr); 
	pFam->m_cpu.m_a &= bOpcode;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpASL_ACC(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);
	pFam->m_cpu.m_p &= ~(FCPU_Zero | FCPU_Negative | FCPU_Carry);
	pFam->m_cpu.m_p |= ((pFam->m_cpu.m_a & 0x80) != 0) ? FCPU_Carry : 0;
	pFam->m_cpu.m_a = pFam->m_cpu.m_a << 1;
	pFam->m_cpu.m_p |= ((pFam->m_cpu.m_a & 0x80) != 0) ? FCPU_Negative : 0;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_a == 0) ? FCPU_Zero : 0;
}

FF_FORCE_INLINE void EvalOpASL(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	pFam->m_cpu.m_p &= ~(FCPU_Zero | FCPU_Negative | FCPU_Carry);
	pFam->m_cpu.m_p |= ((b & 0x80) != 0) ? FCPU_Carry : 0;
	b = b << 1;
	pFam->m_cpu.m_p |= ((b & 0x80) != 0) ? FCPU_Negative : 0;
	pFam->m_cpu.m_p |= (b == 0) ? FCPU_Zero : 0;
	WriteMemU8(pFam, addr, b);
}

FF_FORCE_INLINE void EvalOpBCC(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Carry) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBCS(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Carry) != 0, addr);
}

FF_FORCE_INLINE void EvalOpBEQ(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Zero) != 0, addr);
}

FF_FORCE_INLINE void EvalOpBIT(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	u8 * pP = &pFam->m_cpu.m_p;
	*pP &= ~(FCPU_Overflow | FCPU_Negative | FCPU_Zero);
	*pP |= (b & pFam->m_cpu.m_a) ? 0 : FCPU_Zero;
	*pP |= (b & FCPU_Overflow) ? FCPU_Overflow : 0;
	*pP |= (b & FCPU_Negative) ? FCPU_Negative : 0;
}

FF_FORCE_INLINE void EvalOpBNE(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Zero) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBMI(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Negative) != 0, addr);
}

FF_FORCE_INLINE void EvalOpBPL(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Negative) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBVC(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Overflow) == 0, addr);
}

FF_FORCE_INLINE void EvalOpBVS(Famicom * pFam, u16 addr)
{
	TryBranch(pFam, (pFam->m_cpu.m_p & FCPU_Overflow) != 0, addr);
}

FF_FORCE_INLINE void EvalOpCLC(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpCLD(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_p &= ~FCPU_DecimalMode;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpCLI(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_p &= ~FCPU_InterruptDisable;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpCLV(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_p &= ~FCPU_Overflow;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpCMP(Famicom * pFam, u16 addr)
{
	u8 b = u8(U8ReadMem(pFam, addr));
	u8 dB = u8(pFam->m_cpu.m_a) - b;

	SetZeroNegative(&pFam->m_cpu, dB);
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_a >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpCPX(Famicom * pFam, u16 addr)
{ 
	u8 b = s8(U8ReadMem(pFam, addr));
	u8 dB = s8(pFam->m_cpu.m_x) - b;

	SetZeroNegative(&pFam->m_cpu, dB);
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_x >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpCPY(Famicom * pFam, u16 addr)
{
	u8 b = s8(U8ReadMem(pFam, addr));
	u8 dB = s8(pFam->m_cpu.m_y) - b;

	SetZeroNegative(&pFam->m_cpu, dB);
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_y >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpDEC(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	WriteMemU8(pFam, addr, --b);
	SetZeroNegative(&pFam->m_cpu, b);
}

FF_FORCE_INLINE void EvalOpDEX(Famicom * pFam, u16 addr)
{
	--pFam->m_cpu.m_x;
	TickCpu(pFam);
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_x);
}

FF_FORCE_INLINE void EvalOpDEY(Famicom * pFam, u16 addr)
{
	--pFam->m_cpu.m_y;
	TickCpu(pFam);
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_y);
}

FF_FORCE_INLINE void EvalOpEOR(Famicom * pFam, u16 addr) 
{
	u8 bOpcode = U8ReadMem(pFam, addr); 
	pFam->m_cpu.m_a ^= bOpcode;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpINC(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	WriteMemU8(pFam, addr, ++b);
	SetZeroNegative(&pFam->m_cpu, b);
}

FF_FORCE_INLINE void EvalOpINX(Famicom * pFam, u16 addr)
{
	++pFam->m_cpu.m_x;
	TickCpu(pFam);
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_x);
}

FF_FORCE_INLINE void EvalOpINY(Famicom * pFam, u16 addr)
{
	++pFam->m_cpu.m_y;
	TickCpu(pFam);
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_y);
}

FF_FORCE_INLINE void EvalOpJMP(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_pc = addr;
}

FF_FORCE_INLINE u8 NLowByte(u16 nShort)
{
	return u8(nShort);
}

FF_FORCE_INLINE u8 NHighByte(u16 nShort)
{
	return u8(nShort >> 8);
}

FF_FORCE_INLINE void PushStack(Famicom * pFam, u8 b)
{
	return WriteMemU8(pFam, pFam->m_cpu.m_sp-- | MEMSP_StackMin,  b);
}

FF_FORCE_INLINE u8 NPopStack(Famicom * pFam)
{
	return U8ReadMem(pFam, ++pFam->m_cpu.m_sp | MEMSP_StackMin);
}

FF_FORCE_INLINE void EvalOpJSR(Famicom * pFam, u16 addr)
{
	u16 addrReturn = pFam->m_cpu.m_pc-1;
	TickCpu(pFam);
	PushStack(pFam, NHighByte(addrReturn));
	PushStack(pFam, NLowByte(addrReturn));
	pFam->m_cpu.m_pc = addr;
}

FF_FORCE_INLINE void EvalOpLDA(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_a = U8ReadMem(pFam, addr);
	SetZeroNegative(&pFam->m_cpu, u8(pFam->m_cpu.m_a));
}

FF_FORCE_INLINE void EvalOpLDX(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_x = U8ReadMem(pFam, addr);
	SetZeroNegative(&pFam->m_cpu, u8(pFam->m_cpu.m_x));
}

FF_FORCE_INLINE void EvalOpLDY(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_y = U8ReadMem(pFam, addr);
	SetZeroNegative(&pFam->m_cpu, u8(pFam->m_cpu.m_y));
}

FF_FORCE_INLINE void EvalOpLSR_ACC(Famicom * pFam, u16 addr) 
{
	TickCpu(pFam);
	static_assert(FCPU_Carry == 0x1, "expected carry to be bit 0");
	pFam->m_cpu.m_p &= ~(FCPU_Zero | FCPU_Negative | FCPU_Carry);
	pFam->m_cpu.m_p |= pFam->m_cpu.m_a & FCPU_Carry;
	pFam->m_cpu.m_a = pFam->m_cpu.m_a >> 1;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_a == 0) ? FCPU_Zero : 0;
}

FF_FORCE_INLINE void EvalOpLSR(Famicom * pFam, u16 addr) 
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	pFam->m_cpu.m_p &= ~(FCPU_Zero | FCPU_Negative | FCPU_Carry);

	static_assert(FCPU_Carry == 0x1, "expected carry to be bit 0");
	pFam->m_cpu.m_p |= b & FCPU_Carry;
	b = b >> 1;
	pFam->m_cpu.m_p |= (b == 0) ? FCPU_Zero : 0;
	WriteMemU8(pFam, addr, b);
}

FF_FORCE_INLINE void EvalOpNOP(Famicom * pFam, u16 addr) 
{
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpORA(Famicom * pFam, u16 addr) 
{
	u8 bOpcode = U8ReadMem(pFam, addr); 
	pFam->m_cpu.m_a |= bOpcode;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpPHA(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);
	PushStack(pFam, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpPHP(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);

	// PHP and BRK always set bits 4 & 5. See http://wiki.nesdev.com/w/index.php/Status_flags
	PushStack(pFam, pFam->m_cpu.m_p | FCPU_Break | FCPU_Unused);
}

FF_FORCE_INLINE void EvalOpPLA(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);
	TickCpu(pFam);
	u8 bA = NPopStack(pFam);
	pFam->m_cpu.m_a = bA;
	SetZeroNegative(&pFam->m_cpu, bA);
}
FF_FORCE_INLINE void EvalOpPLP(Famicom * pFam, u16 addr)
{
	// PHP and RTI ignore bits 4 & 5 when setting P. See http://wiki.nesdev.com/w/index.php/Status_flags
	static const u8 kBMaskStatus = ~(FCPU_Break | FCPU_Unused);

	TickCpu(pFam);
	TickCpu(pFam);
	u8 bP = NPopStack(pFam) & kBMaskStatus;
	pFam->m_cpu.m_p = bP;
}

FF_FORCE_INLINE void EvalOpROL_ACC(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);
	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bA = pFam->m_cpu.m_a;
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= ((bA & 0x80) != 0) ? FCPU_Carry : 0;
	pFam->m_cpu.m_a = (bA << 1) | bCarry;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpROL(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);

	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= ((b & 0x80) != 0) ? FCPU_Carry : 0;

	b = (b << 1) | bCarry;
	WriteMemU8(pFam, addr, b);
	SetZeroNegative(&pFam->m_cpu, b);
}

FF_FORCE_INLINE void EvalOpSEC(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_p |= FCPU_Carry;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpSED(Famicom * pFam, u16 addr) 
{
	pFam->m_cpu.m_p |= FCPU_DecimalMode;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpSEI(Famicom * pFam, u16 addr) 
{
	pFam->m_cpu.m_p |= FCPU_InterruptDisable;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpSTA(Famicom * pFam, u16 addr)
{
	WriteMemU8(pFam, addr, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpSTX(Famicom * pFam, u16 addr)
{
	WriteMemU8(pFam, addr, pFam->m_cpu.m_x);
}

FF_FORCE_INLINE void EvalOpSTY(Famicom * pFam, u16 addr)
{
	WriteMemU8(pFam, addr, pFam->m_cpu.m_y);
}

FF_FORCE_INLINE void EvalOpTAX(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_x = pFam->m_cpu.m_a;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpTAY(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_y = pFam->m_cpu.m_a;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpTSX(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_x = pFam->m_cpu.m_sp;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_sp);
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpTXA(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_a = pFam->m_cpu.m_x;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_x);
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpTXS(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_sp = pFam->m_cpu.m_x;
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpTYA(Famicom * pFam, u16 addr)
{
	pFam->m_cpu.m_a = pFam->m_cpu.m_y;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_y);
	TickCpu(pFam);
}

FF_FORCE_INLINE void EvalOpROR_ACC(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);
	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bA = pFam->m_cpu.m_a;
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= (bA & FCPU_Carry);
	pFam->m_cpu.m_a = (bA >> 1) | (bCarry << 7);
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpROR(Famicom * pFam, u16 addr)
{
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);

	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= (b & FCPU_Carry);
	b = (b >> 1) | (bCarry << 7);
	WriteMemU8(pFam, addr, b);
	SetZeroNegative(&pFam->m_cpu, b);
}

FF_FORCE_INLINE void EvalOpRTI(Famicom * pFam, u16 addr)
{
	EvalOpPLP(pFam, addr);

	u8 nLow = NPopStack(pFam);
	u8 nHigh = NPopStack(pFam);
	pFam->m_cpu.m_pc = (nHigh << 0x8) | nLow;
}

FF_FORCE_INLINE void EvalOpRTS(Famicom * pFam, u16 addr)
{
	TickCpu(pFam);

	u16 nLow = NPopStack(pFam);
	u16 nHigh = NPopStack(pFam);
	TickCpu(pFam);
	pFam->m_cpu.m_pc = (nHigh << 0x8) | nLow;
	(void)U8ReadMem(pFam, pFam->m_cpu.m_pc++);
}

// unofficial ops
FF_FORCE_INLINE void EvalOpAHX(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpALR(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpANC(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpARR(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpAXS(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpDCP(Famicom * pFam, u16 addr)
{
	// dec
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	WriteMemU8(pFam, addr, --b);

	// cmp
	//u8 b = u8(U8ReadMem(pFam, addr));
	u8 dB = u8(pFam->m_cpu.m_a) - b;

	SetZeroNegative(&pFam->m_cpu, dB);
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	pFam->m_cpu.m_p |= (pFam->m_cpu.m_a >= b) ? FCPU_Carry : FCPU_None;
}

FF_FORCE_INLINE void EvalOpISB(Famicom * pFam, u16 addr)
{
	// inc
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	WriteMemU8(pFam, addr, ++b);
	SetZeroNegative(&pFam->m_cpu, b);

	// sbc
	s16 nDif = pFam->m_cpu.m_a - b - (1 - NCarry(FCPU(pFam->m_cpu.m_p)));

	pFam->m_cpu.m_p &= ~(FCPU_Carry | FCPU_Overflow);
	pFam->m_cpu.m_p |= (nDif & 0xFF00) ? 0 : FCPU_Carry;  // carry is set if result is outside 0..255
	pFam->m_cpu.m_p |= ((( pFam->m_cpu.m_a ^ b ) & ( pFam->m_cpu.m_a ^ nDif )) & 0x80 ) ? FCPU_Overflow : 0;  // did the sign bit change?
	SetZeroNegative(&pFam->m_cpu, u8(nDif));

	pFam->m_cpu.m_a = u8(nDif);
}

FF_FORCE_INLINE void EvalOpKIL(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpLAS(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpLAX(Famicom * pFam, u16 addr)
{
	// LDA imm
	//pFam->m_cpu.m_a = U8ReadMem(pFam, pFam->m_cpu.m_pc);
	pFam->m_cpu.m_a = U8ReadMem(pFam, addr);

	// TAX
	pFam->m_cpu.m_x = pFam->m_cpu.m_a;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpRLA(Famicom * pFam, u16 addr)
{
	//ROL
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);

	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= ((b & 0x80) != 0) ? FCPU_Carry : 0;

	b = (b << 1) | bCarry;
	WriteMemU8(pFam, addr, b);

	//AND
	pFam->m_cpu.m_a &= b;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpRRA(Famicom * pFam, u16 addr)
{
	//ROR
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);

	static_assert(FCPU_Carry == 0x1, "expected carry to be bit zero");
	u8 bCarry = (pFam->m_cpu.m_p & FCPU_Carry);

	pFam->m_cpu.m_p &= ~(FCPU_Carry);
	pFam->m_cpu.m_p |= (b & FCPU_Carry);
	b = (b >> 1) | (bCarry << 7);
	WriteMemU8(pFam, addr, b);

	//ADC
	s16 nSum = pFam->m_cpu.m_a + b + NCarry(FCPU(pFam->m_cpu.m_p));

	pFam->m_cpu.m_p &= ~(FCPU_Carry | FCPU_Overflow);
	pFam->m_cpu.m_p |= (nSum & 0xFF00) ? FCPU_Carry : 0;
	pFam->m_cpu.m_p |= ((( pFam->m_cpu.m_a ^ nSum ) & ( b ^ nSum )) & 0x80 ) ? FCPU_Overflow : 0;	// did the sign bit change?
	SetZeroNegative(&pFam->m_cpu, u8(nSum));

	pFam->m_cpu.m_a = u8(nSum);
}

FF_FORCE_INLINE void EvalOpSAX(Famicom * pFam, u16 addr)
{
	WriteMemU8(pFam, addr, pFam->m_cpu.m_a & pFam->m_cpu.m_x);
}

FF_FORCE_INLINE void EvalOpSHX(Famicom * pFam, u16 addr)
{
	u16 addrHigh = addr & 0xFF00;
	WriteMemU8(pFam, addr, pFam->m_cpu.m_x & addrHigh);
}

FF_FORCE_INLINE void EvalOpSHY(Famicom * pFam, u16 addr)
{
	u16 addrHigh = addr & 0xFF00;
	WriteMemU8(pFam, addr, pFam->m_cpu.m_y & addrHigh);
}

FF_FORCE_INLINE void EvalOpSLO(Famicom * pFam, u16 addr) 
{
	//ASL
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);
	pFam->m_cpu.m_p &= ~FCPU_Carry;
	pFam->m_cpu.m_p |= ((b & 0x80) != 0) ? FCPU_Carry : 0;
	b = b << 1;
	WriteMemU8(pFam, addr, b);

	//ORA
	//u8 bOpcode = U8ReadMem(pFam, addr); 
	pFam->m_cpu.m_a |= b;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpSRE(Famicom * pFam, u16 addr)
{
	//LSR
	u8 b = U8ReadMem(pFam, addr);
	WriteMemU8(pFam, addr, b);

	pFam->m_cpu.m_p &= ~FCPU_Carry;
	static_assert(FCPU_Carry == 0x1, "expected carry to be bit 0");
	pFam->m_cpu.m_p |= b & FCPU_Carry;
	b = b >> 1;
	WriteMemU8(pFam, addr, b);

	//EOr
	pFam->m_cpu.m_a ^= b;
	SetZeroNegative(&pFam->m_cpu, pFam->m_cpu.m_a);
}

FF_FORCE_INLINE void EvalOpTAS(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }
FF_FORCE_INLINE void EvalOpXAA(Famicom * pFam, u16 addr) {FF_ASSERT(false, "Illegal op"); }

FF_FORCE_INLINE void EvalOpBRK(Famicom * pFam, u16 addr)
{
	(void)U8ReadMem(pFam, pFam->m_cpu.m_pc++);
	(void)U8ReadMem(pFam, pFam->m_cpu.m_pc++);
	PushStack(pFam, u8(pFam->m_cpu.m_pc >> 8));
	PushStack(pFam, u8(pFam->m_cpu.m_pc & 0xFF));

	PushStack(pFam, pFam->m_cpu.m_p | FCPU_Break);
	u16 nPcLow = U8ReadMem(pFam, addr);
	pFam->m_cpu.m_p |= FCPU_InterruptDisable;
	u16 nPcHigh = U8ReadMem(pFam, addr+1);
	pFam->m_cpu.m_pc = (nPcHigh << 8) | nPcLow;
}

FF_FORCE_INLINE void ExecuteInterrupt(Famicom * pFam, u16 addrVector) 
{
	(void)U8ReadMem(pFam, pFam->m_cpu.m_pc);
	(void)U8ReadMem(pFam, pFam->m_cpu.m_pc);
	PushStack(pFam, u8(pFam->m_cpu.m_pc >> 8));
	PushStack(pFam, u8(pFam->m_cpu.m_pc & 0xFF));

	PushStack(pFam, pFam->m_cpu.m_p & ~FCPU_Break);
	u16 nPcLow = U8ReadMem(pFam, addrVector);
	pFam->m_cpu.m_p |= FCPU_InterruptDisable;
	u16 nPcHigh = U8ReadMem(pFam, addrVector+1);
	pFam->m_cpu.m_pc = (nPcHigh << 8) | nPcLow;
}

void StepCpu(Famicom * pFam)
{
	auto pCpu = &pFam->m_cpu;
	auto pMemmp = &pFam->m_memmp;
	u8 bOpcode = U8ReadMem(pFam, pCpu->m_pc++);

#define OP(NAME, OPKIND, ADRW, CYCLE) case 0x##NAME: { u16 addr = NEvalAdrw##ADRW(pFam); EvalOp##OPKIND(pFam, addr); } break;
#define IL(NAME, OPKIND, ADRW, CYCLE) case 0x##NAME: { u16 addr = NEvalAdrw##ADRW(pFam); EvalOp##OPKIND(pFam, addr); } break;
#define OPA(NAME, OPKIND, ADRW, CYCLE) case 0x##NAME: { u16 addr = NEvalAdrw##ADRW(pFam); EvalOp##OPKIND##_ACC(pFam, addr); } break;
	switch (bOpcode)
	{
		OPCODE_TABLE
	}
#undef OPA
#undef IL
#undef OP

	if (pFam->m_cpu.m_fTriggerNmi)
	{
		ExecuteInterrupt(pFam, kAddrNmi);
		pFam->m_cpu.m_fTriggerNmi = false;
	}
}

inline u8 U8PeekMemNintendulator(MemoryMap * pMemmp, u16 addr)
{
	// nintendulator prints 0xFF for logs for these ranges
	if (addr >= MEMSP_PpuRegisterMin && addr <= MEMSP_PpuMirrorsMax)
		return 0xFF;

	if (addr >= MEMSP_IoRegisterMin && addr <= MEMSP_IoRegisterMax)
		return 0xFF;

	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];
	if ((memdesc.m_fmem & FMEM_Mapped) == 0)
		return 0xFF;

	return U8PeekMem(pMemmp, addr);
}

int CChPrintDisassmLine(const Cpu * pCpu, MemoryMap * pMemmp, u16 addr, char * pChz, int cChMax)
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
	int cChWritten = 0;
	switch (pAmodinfo->m_cB)
	{
		case 1: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X       ", addr, aB[0]); break;
		case 2: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X %02X    ", addr, aB[0], aB[1]); break;
		case 3: cChWritten = sprintf_s(pChz, cChMax, "%04X  %02X %02X %02X ", addr, aB[0], aB[1], aB[2]); break;
		default: FF_ASSERT(false, "unexpected opcode size");
	}

	pChz = pChz + cChWritten;
	cChMax = ffMax(0, cChMax - cChWritten);

	if (cChMax == 0)
		return cChWritten;

	char ch = (pOpinfo->m_fIsOfficial) ? ' ' : '*';
	int cChOp = sprintf_s(pChz, cChMax, "%c%s ", ch, pOpinfo->m_pChzMnemonic);

	cChWritten += cChOp;
	pChz = pChz + cChOp;
	cChMax = ffMax(0, cChMax - cChOp);

	if (cChMax == 0)
		return cChWritten;

	// should match DecodeInstruction() in nintendulator
	switch(pAmodinfo->m_amod)
	{
		case AMOD_Implicit:
			break;
		case AMOD_Accumulator:
			{
				cChWritten += sprintf_s(pChz, cChMax, "A");				
			} break;
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
				u16 addr = (u16(aB[1]) | (u16(aB[2]) << 8)) + pCpu->m_x; 
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X%02X,X @ %04X = %02X", aB[2], aB[1], addr, nPeek);	
			} break;
		case AMOD_AbsoluteY: 
			{
				u16 addr = (u16(aB[1]) | (u16(aB[2]) << 8)) + pCpu->m_y; 
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X%02X,Y @ %04X = %02X", aB[2], aB[1], addr, nPeek);	
			} break;
		case AMOD_ZeroPage:  
			{
				u16 addr = u16(aB[1]);
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X = %02X", aB[1], nPeek);
			} break;
		case AMOD_ZeroPageX:
			{
				u16 addr = (u16(pCpu->m_x) + aB[1]) & 0xFF;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X,X @ %02X = %02X", aB[1], addr, nPeek);				
			} break;
		case AMOD_ZeroPageY: 
			{
				u16 addr = (u16(pCpu->m_y) + aB[1]) & 0xFF;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addr);
				cChWritten += sprintf_s(pChz, cChMax, "$%02X,Y @ %02X = %02X", aB[1], addr, nPeek);				
			} break;
		case AMOD_Indirect:
			{
				u16 addrHigh = u16(aB[2]) << 8;
				u16 addrBase = u16(aB[1]) | addrHigh;

				u8 nLow = U8PeekMem(pMemmp, addrBase); 
				u8 nHigh = U8PeekMem(pMemmp, addrHigh | ((addrBase+1) & 0xFF));
				u16 addrInd = u16(nLow) | (u16(nHigh) << 8);

				cChWritten += sprintf_s(pChz, cChMax, "($%04X) = %04X", addrBase, addrInd);	
			} break;
		case AMOD_IndirectX: 
			{
				u16 addrMid = (aB[1] + pCpu->m_x) & 0x00FF;
				u16 addrEffective = U8PeekMem(pMemmp, addrMid);

				addrEffective |= U8PeekMem(pMemmp, (addrMid + 1) & 0xFF) << 0x8;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addrEffective);
			
				cChWritten += sprintf_s(pChz, cChMax, "($%02X,X) @ %02X = %04X = %02X", aB[1], addrMid, addrEffective, nPeek);
			} break;
		case AMOD_IndirectY:
			{
				u16 addrMid = U8PeekMem(pMemmp, aB[1]); 
				addrMid |= U8PeekMem(pMemmp, u16(aB[1] + 1) &0xff) << 8;

				u16 addrEffective = addrMid + pCpu->m_y;
				u8 nPeek = U8PeekMemNintendulator(pMemmp, addrEffective);

				cChWritten += sprintf_s(pChz, cChMax, "($%02X),Y = %04X @ %04X = %02X", aB[1], addrMid, addrEffective, nPeek);
			} break;
		default:
			FF_ASSERT(false, "unhandled addressing mode.");
	}

	return cChWritten;
}

int CChPrintCpuState(Famicom * pFam, char * pChz, int cChMax)
{
	const Cpu * pCpu = &pFam->m_cpu;
	const PpuTiming * pPtim = &pFam->m_ptimCpu;
	int cScanline;
	int tickpScanline;
	SplitTickpScanline(pFam->m_ptimCpu.m_tickp, &cScanline, &tickpScanline);

	u8 p = pCpu->m_p | FCPU_Unused;
	int iCh = sprintf_s(pChz, cChMax, "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3i,%3i CYC:%I64d", 
		pCpu->m_a, pCpu->m_x, pCpu->m_y, p, pCpu->m_sp, tickpScanline, cScanline, pCpu->m_tickc);
	return iCh;
}

int CChPadText(char * pChz, int cChMax, char chPad, int cChPad)
{
	if (cChMax <= 0)
		return 0;

	int cCh = ffMin(cChMax - 1, cChPad);
	auto pChzMax = &pChz[cCh];
	auto pChzIt = pChz;
	for ( ; pChzIt != pChzMax; ++pChzIt)
	{
		*pChzIt = chPad;
	}

	*pChzIt = '\0';
	return int(pChzIt - pChz);
}

void PrintCpuStateForLog(Famicom * pFam, char * pChz, int cChMax)
{
	// meant to match CPU logs from nintedulator

	if (cChMax <= 0)
		return;
	int cChAsm = CChPrintDisassmLine(&pFam->m_cpu, &pFam->m_memmp, pFam->m_cpu.m_pc, pChz, cChMax);
	pChz += cChAsm;
	cChMax = ffMax(0, cChMax - cChAsm);

	// pad to colunn 49
	int cChPad =  CChPadText(pChz, cChMax, ' ', 48 - cChAsm);
	pChz += cChPad;
	cChMax = ffMax(0, cChMax - cChPad);

	if (cChMax < 0)
		return;

	(void)CChPrintCpuState(pFam, pChz, cChMax);
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

static const int s_cLlineHistory = 100; 
static const int s_cChLine = 128; 
struct LogHistory;
void ClearLogHistory(LogHistory * pLoghist);

struct LogLine // tag = lline
{
	char		m_aChz[s_cChLine];
};

struct LogHistory // tag = lhist
{
				LogHistory()
					{ ClearLogHistory(this); }

	int			m_iLlineNext;
	LogLine		m_aLline[s_cLlineHistory];
};

void ClearLogHistory(LogHistory * pLhist)
{
	LogLine * pLlineMax = &pLhist->m_aLline[s_cLlineHistory];
	for (LogLine * pLlineIt = pLhist->m_aLline; pLlineIt != pLlineMax; ++pLlineIt)
	{
		pLlineIt->m_aChz[0] = '\0';	
	}
	pLhist->m_iLlineNext = 0;
}

LogLine * PLlineAppend(LogHistory * pLhist)
{
	LogLine * pLlineRet = &pLhist->m_aLline[pLhist->m_iLlineNext];
	pLhist->m_iLlineNext = (pLhist->m_iLlineNext + 1) % FF_DIM(pLhist->m_aLline);

	pLlineRet->m_aChz[0] = '\0';
	return pLlineRet;
}

void PrintLogHistory(LogHistory * pLhist)
{
	int  iLline = pLhist->m_iLlineNext;
	for (int cLline = 0; cLline < s_cLlineHistory; ++cLline)
	{
		const char * pChz = pLhist->m_aLline[iLline].m_aChz;
		if (*pChz != '\0')
		{
			printf("%s\n", pChz);
		}
		iLline = (iLline + 1) % FF_DIM(pLhist->m_aLline);
	}
}

bool FTryRunLogTest(const char * pChzFilenameRom, const char * pChzFilenameLog, int addrStart) 
{
	LogHistory lhist;

	Famicom fam;
	Cart cart;
	fam.m_pCart = &cart;

	StaticInitFamicom(&fam, nullptr);

	FPOW fpow = FPOW_LogTest;
	if (!FTryLoadRomFromFile(pChzFilenameRom, &cart, &fam, fpow))
	{
		ShowError("Could not load log test rom file '%s'", pChzFilenameRom);
		return false;
	}

	if (addrStart >= 0)
	{
		fam.m_cpu.m_pc = u16(addrStart);
	}

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
	char aChLog[128];
	int cLineLog = 0;
	int cStep = 0;
	auto cFramePrev = CFrameFromTickp(fam.m_ptimCpu.m_tickp);
	while (pBLogIt != pBLogMax)
	{
		LogLine * pLline = PLlineAppend(&lhist);
		PrintCpuStateForLog(&fam, pLline->m_aChz, FF_DIM(pLline->m_aChz));

		size_t cChAdvance = 0;
		if (!FDoLogsMatch(pLline->m_aChz, FF_PMAX(pLline->m_aChz), pBLogIt, pBLogMax, &cChAdvance))
		{
			PrintLogHistory(&lhist);
	//		StepCpu(&fam);

			size_t cChState = strlen(pLline->m_aChz);
			strncpy_s(aChLog, FF_DIM(aChLog), pBLogIt, cChState);
			ShowError("Cpu emulation does not match log:\nemu = '%s'\nlog = '%s'\n", pLline->m_aChz, aChLog);
			fPassTest = false;
			break;
		}

		pBLogIt += cChAdvance;

		StepCpu(&fam);

		auto cFrameNew = CFrameFromTickp(fam.m_ptimCpu.m_tickp);
		if (cFrameNew != cFramePrev)
		{
			UpdatePpu(&fam, fam.m_ptimCpu);
		}
	}

	printf("Test '%s' success at cycle %lld\n", pChzFilenameLog, fam.m_cpu.m_tickc);
	return fPassTest;
}

bool FTryAllLogTests()
{
	struct LogTest
	{
		const char *	m_pChzPath;
		const char *	m_pChzRom;
		const char *	m_pChzLog;
		int				m_addrStart;
	};

	static const LogTest s_aLogtest[] = {
		{"test\\nes_instr_test",	"zero_page.nes",	"zero_page.cpulog",		-1},
		{"test\\nestest",			"nestest.nes",		"nestest.cpulog",		0xC000}
	};

	char aChRom[256];
	char aChLog[256];
	
	for (int iLogtest = 0; iLogtest < FF_DIM(s_aLogtest); ++iLogtest)
	{
		const LogTest & logtest = s_aLogtest[iLogtest];
		sprintf_s(aChRom, FF_DIM(aChRom), "%s\\%s", logtest.m_pChzPath, logtest.m_pChzRom);
		sprintf_s(aChLog, FF_DIM(aChRom), "%s\\%s", logtest.m_pChzPath, logtest.m_pChzLog);

		if (!FTryRunLogTest(aChRom, aChLog, logtest.m_addrStart))
		{
			ShowError("log test %d failed", iLogtest);
			return false;
		}
	}
	return true;
}

void StaticInitFamicom(Famicom * pFam, Platform * pPlat)
{
	StaticInitPpu(&pFam->m_ppu, pPlat);

	SetupDefaultInputMapping(pFam);
}

void ExecuteFamicomFrame(Famicom * pFam)
{
	if (!pFam->m_fIsRomLoaded)
		return;
	// run the CPU from cycle 0 until the last cycle of the last pixel of scanline 261

	if (pFam->m_stepk == STEPK_Run)
	{
		s64 cFramePrev = CFrameFromTickp(pFam->m_ptimCpu.m_tickp);
		while (pFam->m_stepk == STEPK_Run && cFramePrev == CFrameFromTickp(pFam->m_ptimCpu.m_tickp))
		{
			if (s_fTraceCpu)
			{
				char aChz[s_cChLine];
				PrintCpuStateForLog(pFam, aChz, FF_DIM(aChz));
				printf("%s\n", aChz);
			}

			StepCpu(pFam);
		}
	}

	s64 cFrame;
	s64 tickpSubframe;
	SplitTickpFrame(pFam->m_ptimCpu.m_tickp, &cFrame, &tickpSubframe);

	PpuTiming ptimFrameEnd = pFam->m_ptimCpu;
	ptimFrameEnd.m_tickp -= tickpSubframe;
	UpdatePpu(pFam, ptimFrameEnd);
}
 