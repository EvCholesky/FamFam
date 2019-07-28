#include "Platform.h"
#include "Rom.h"

#include <stdio.h>
#include <stdlib.h>         // NULL, malloc

int CPageRom(int nMSB, int nLSB)
{
	int n = (nMSB << 8) | nLSB;

	// if the most significant nibble is 0xF  use an exponent multiplier 
	if ((n & 0xF00) == 0xF00)
	{
		int mult = (n & 0x3) * 2 + 1;
		int exp = (n & 0xF6) >> 2;
		return (0x1 << exp) * mult;
	}

	return n;
}

void AppendAddrOffset(Ppu * pPpu, DynAry<u16> * pAryAddrInstruct, u16 addrBase, int cB)
{
	FF_ASSERT(pAryAddrInstruct->FIsEmpty() || pAryAddrInstruct->Last() < addrBase, "instructions added out of order");

	int addrIt = addrBase;
	int addrEnd = addrBase + cB;
	while (addrIt < addrEnd)
	{
		u8 bOpcode = *PBVram(pPpu, addrIt);
		auto pOpinfo = POpinfoFromOpcode(bOpcode); 
		auto pAmrwinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

		pAryAddrInstruct->Append(addrIt);
		addrIt += pAmrwinfo->m_cB;
	}
}

void MapMemoryCommon(MemoryMap * pMemmp)
{
	auto addrspBottom = AddrspMapMemory(pMemmp, 0x0, 0x800);		// bottom 2K
	MapMirrored(pMemmp, addrspBottom, 0x800, 0x2000);				// mirrors to 8K

	auto iMemcbWriteOnly = IMemcbAllocate(pMemmp, U8ReadOpenBus, WritePpuReg);
	auto iMemcbStatus = IMemcbAllocate(pMemmp, U8ReadPpuStatus, WritePpuReg);
	auto iMemcbReadWrite = IMemcbAllocate(pMemmp, U8ReadPpuReg, WritePpuReg);

	MemoryDescriptor aMemdesc[] = {
		MemoryDescriptor(FMEM_None,		iMemcbWriteOnly),	//PPUREG_Ctrl			= 0x2000
		MemoryDescriptor(FMEM_None,		iMemcbWriteOnly),	//PPUREG_Mask			= 0x2001
		MemoryDescriptor(FMEM_ReadOnly,	iMemcbStatus),		//PPUREG_Status			= 0x2002
		MemoryDescriptor(FMEM_None,		iMemcbWriteOnly),	//PPUREG_OAMAddr 		= 0x2003
		MemoryDescriptor(FMEM_None,		iMemcbReadWrite),	//PPUREG_OAMData 		= 0x2004
		MemoryDescriptor(FMEM_None,		iMemcbWriteOnly),	//PPUREG_PpuScroll		= 0x2005
		MemoryDescriptor(FMEM_None,		iMemcbWriteOnly),	//PPUREG_PpuAddr		= 0x2006
		MemoryDescriptor(FMEM_None,		iMemcbReadWrite),	//PPUREG_PpuData		= 0x2007
	};
	auto addrspPpuReg = AddrspMapMemory(pMemmp, 0x2000, 0x2008, aMemdesc, FF_DIM(aMemdesc));	// PPU registers
	MapMirrored(pMemmp, addrspPpuReg, 0x2008, 0x4000, aMemdesc, FF_DIM(aMemdesc));				// mirrors to 16k

	auto addrspApuIoReg = AddrspMapMemory(pMemmp, 0x4000, 0x4014);	// APU and IO Registers

	auto iMemcbOamDmaReg = IMemcbAllocate(pMemmp, U8ReadOpenBus, WriteOamDmaRegister);
	auto iMemcbController0 = IMemcbAllocate(pMemmp, U8ReadControllerReg, WriteControllerLatch);
	auto iMemcbController1 = IMemcbAllocate(pMemmp, U8ReadControllerReg, WriteOpenBus);
	auto iMemcbOpenBus = IMemcbAllocate(pMemmp, U8ReadOpenBus, WriteOpenBus);

	MemoryDescriptor aMemdescIO[] = {
		MemoryDescriptor(FMEM_None,		iMemcbOamDmaReg),	//OAMDMA				= 0x4014
		MemoryDescriptor(FMEM_None,		iMemcbOpenBus),		//SND_CHN(unimplemented)= 0x4015
		MemoryDescriptor(FMEM_None,		iMemcbController0),	//JOY1					= 0x4016
		MemoryDescriptor(FMEM_None,		iMemcbController1),	//JOY2			 		= 0x4017
	};

	(void)AddrspMapMemory(pMemmp, 0x4014, 0x4018, aMemdescIO, FF_DIM(aMemdescIO));
	(void)AddrspMarkUnmapped(pMemmp, 0x4018, 0x4020);				// unused APU and IO test space
}

bool FTrySetupMapperNrom(Famicom * pFam, Cart* pCart)
{
	auto pMemmp = &pFam->m_memmp;
	MapMemoryCommon(pMemmp);
	(void)AddrspMarkUnmapped(pMemmp, 0x4020, 0x8000);
	
	MemoryDescriptor memdescReadOnly(FMEM_ReadOnly);
	switch (pCart->m_cBPrgRom)
	{
	case FF_KIB(16):
		{
			auto addrspPrg = AddrspMapMemory(pMemmp, 0x8000, 0xC000, &memdescReadOnly, 1);
			MapMirrored(pMemmp, addrspPrg, 0xC000, 0x10000);
			CopyAB(pCart->m_pBPrgRom, &pMemmp->m_aBRaw[0x8000], FF_KIB(16));

			pCart->m_addrPrgMappedMin = 0x8000;
			pCart->m_addrPrgMappedMax = 0x10000;
			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0xC000);
			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0xC000);
		}break;
	case FF_KIB(32):
		{
			(void)AddrspMapMemory(pMemmp, 0x8000, 0x10000, &memdescReadOnly, 1);
			CopyAB(pCart->m_pBPrgRom, &pMemmp->m_aBRaw[0x8000], FF_KIB(32));

			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0x10000);
		} break;

	default:
		ShowError("unexpected PRG rom size %d bytes", pCart->m_cBPrgRom);
		return false;
	}

	pCart->m_fRecomputeAddrInstruct = true;

	//TestMemoryMap(pMemmp);

	int cBChr = ffMin<int>(FF_DIM(pFam->m_ppu.m_aBChr), pCart->m_cBChrRom);
	NTMIR ntmir = (pCart->m_pHead->m_romf.m_fMirrorVert) ? NTMIR_Vertical : NTMIR_Horizontal;
	InitPpuMemoryMap(&pFam->m_ppu, cBChr, ntmir);
	InitChrMemory(&pFam->m_ppu, 0x0000, pCart->m_pBChrRom, cBChr);
	return true;
}

MapperMMC1::MapperMMC1()
:m_iMemcbWriteMem(0)
,m_cBitShift(0)
,m_bShift(0)
,m_nReg(0)
,m_nRegPrev(0xFFFFFFFF)
{
	ZeroAB(m_aBReg, sizeof(m_aBReg));
	FillAB(0xFF, m_aBRegPrev, sizeof(m_aBRegPrev));
}

struct RomPage // rpage
{
			RomPage()
			:m_iB(0)
			,m_cB(0)
				{ ; }

			RomPage(int iB, size_t cB)
			:m_iB(iB)
			,m_cB(cB)
				{ ; }

	int		m_iB;
	size_t	m_cB;
};

static inline bool FAreSamePage(RomPage * rpageA, RomPage * rpageB)
{
	return rpageA->m_iB == rpageB->m_iB && rpageA->m_cB == rpageB->m_cB;
}

static inline void ComputePrgPagesMmc1(u8 * pBReg, Cart * pCart, RomPage * pRpage0, RomPage * pRpage1)
{
	int nPrgMode = (pBReg[MAPREG1_Control] & 0xC) >> 2;
	switch(nPrgMode)
	{
		case 0:
		case 1: // 32k page
			*pRpage0 = RomPage((pBReg[MAPREG1_PrgPage] & 0xE) * FF_KIB(16), FF_KIB(32));
			*pRpage1 = RomPage();
			break;
		case 2: // 16k page, first bank at 0x8000, and selected bank at 0xC000
			*pRpage0 = RomPage(0, FF_KIB(16));
			*pRpage1 = RomPage((pBReg[MAPREG1_PrgPage] & 0xF) * FF_KIB(16), FF_KIB(16));
			break;
		case 3: // 16k page, selected bank at 0x8000, and last bank at 0xC000
			*pRpage0 = RomPage((pBReg[MAPREG1_PrgPage] & 0xF) * FF_KIB(16), FF_KIB(16));

			int iPage = (pCart->m_cBPrgRom / FF_KIB(16)) - 1;
			*pRpage1 = RomPage(iPage * FF_KIB(16), FF_KIB(16));
			break;
	}
}

static inline void ComputeChrPagesMmc1(u8 * pBReg, RomPage * pRpage0, RomPage * pRpage1)
{
	int nChrMode = (pBReg[MAPREG1_Control] & 0x10) >> 4;
	if (nChrMode)
	{
		// 2 4k banks
		*pRpage0 = RomPage((pBReg[MAPREG1_ChrPage0] & 0x1F) * FF_KIB(4), FF_KIB(4));
		*pRpage1 = RomPage((pBReg[MAPREG1_ChrPage1] & 0x1F) * FF_KIB(4), FF_KIB(4));
	}
	else
	{
		// 1 8k bank
		*pRpage0 = RomPage((pBReg[MAPREG1_ChrPage0] & 0x1E) * FF_KIB(4), FF_KIB(8));
		*pRpage1 = RomPage();
	}
}

void UpdateBanks(Famicom * pFam, MapperMMC1 * pMapr1)
{
	static const u32 s_nMaskAll = 0x1F1F1F1F;
	static const u32 s_nMaskRam = 0x1F00000F;
	static const u32 s_nMaskPrg = 0x1F00000C;
	static const u32 s_nMaskChr = 0x001F1F10;
	static const u32 s_nMaskMirror = 0x00000003;

	u32 nTest = s_nMaskRam;
	u8 * pBTest = (u8*)&nTest;
	FF_ASSERT(pBTest[0] == 0x0F, "endian!!!");

	// we only need to update chr if it's chrRom, chrRam is filled in by the cpu
	Cart * pCart = pFam->m_pCart; 
	bool fUseRom = pCart->m_cBChrRom > 0;
	u32 nMask = (fUseRom) ? s_nMaskAll : s_nMaskRam;

	if ((pMapr1->m_nReg & nMask) == (pMapr1->m_nRegPrev & nMask)) 
		return;

	bool fHasMirrorChanged =(pMapr1->m_nReg & s_nMaskMirror) != (pMapr1->m_nRegPrev & s_nMaskMirror);
	bool fHasPrgChanged = (pMapr1->m_nReg & s_nMaskPrg) != (pMapr1->m_nRegPrev & s_nMaskPrg);
	bool fHasChrChanged = (pMapr1->m_nReg & s_nMaskChr) != (pMapr1->m_nRegPrev & s_nMaskChr);
	pMapr1->m_nRegPrev = pMapr1->m_nReg;

	u8 * aBReg = pMapr1->m_aBReg;
	if (fHasMirrorChanged)
	{
		NTMIR ntmir;
		switch (pMapr1->m_nReg & 0x3)
		{
		case 0: ntmir = NTMIR_OneScreenLow; break; 
		case 1: ntmir = NTMIR_OneScreenHigh; break; 
		case 2: ntmir = NTMIR_Vertical; break; 
		case 3: ntmir = NTMIR_Horizontal; break; 
		}

		SetNametableMapping(&pFam->m_ppu, ntmir);
	}

	if (fHasPrgChanged)
	{
		auto pMemmp = &pFam->m_memmp;
		RomPage rpagePrg0, rpagePrg1;
		ComputePrgPagesMmc1(aBReg, pFam->m_pCart, &rpagePrg0, &rpagePrg1);

		pCart->m_aryAddrInstruct.Clear();

		pCart->m_addrPrgMappedMin = rpagePrg0.m_iB;
		pCart->m_addrPrgMappedMax = rpagePrg0.m_iB + rpagePrg0.m_cB;

		CopyAB(&pCart->m_pBPrgRom[rpagePrg0.m_iB], &pMemmp->m_aBRaw[0x8000], rpagePrg0.m_cB);
		pCart->m_fRecomputeAddrInstruct = true;

		if (rpagePrg1.m_cB != 0)
		{
			FF_ASSERT(rpagePrg1.m_cB == FF_KIB(16));
			CopyAB(&pCart->m_pBPrgRom[rpagePrg1.m_iB], &pMemmp->m_aBRaw[0xC000], rpagePrg1.m_cB);

			pCart->m_addrPrgMappedMin = rpagePrg1.m_iB;
			pCart->m_addrPrgMappedMax = rpagePrg1.m_iB + rpagePrg0.m_cB;
		}
	}

	if (fHasChrChanged && fUseRom)
	{
		RomPage rpageChr0, rpageChr1;
		ComputeChrPagesMmc1(aBReg, &rpageChr0, &rpageChr1);

		if (rpageChr1.m_cB != 0)
		{
			InitChrMemory(&pFam->m_ppu, 0x0000, &pCart->m_pBChrRom[rpageChr0.m_iB], rpageChr0.m_cB);
			InitChrMemory(&pFam->m_ppu, 0x1000, &pCart->m_pBChrRom[rpageChr1.m_iB], rpageChr1.m_cB);
		}
		else
		{
			InitChrMemory(&pFam->m_ppu, 0x0000, &pCart->m_pBChrRom[rpageChr0.m_iB], rpageChr0.m_cB);
		}
	}
}

bool FTrySetupMapperMmc1(Famicom * pFam, Cart* pCart)
{
	auto pMemmp = &pFam->m_memmp;
	MapMemoryCommon(pMemmp);
	u32 addrUnmappedMax = 0x8000;
	if (pCart->m_cBPrgRam)
	{
		if (pCart->m_cBPrgRam != FF_KIB(8))
		{
			ShowError("Unexpected PRG ram size '%d' bytes", pCart->m_cBPrgRam);
			return false;
		}

		addrUnmappedMax = 0x6000;
		(void) AddrspMapMemory(pMemmp, 0x6000, FF_KIB(8));
	}

	(void)AddrspMarkUnmapped(pMemmp, 0x4020, addrUnmappedMax);

	MapperMMC1 * pMapr1 = new MapperMMC1;
	pMapr1->m_iMemcbWriteMem = IMemcbAllocate(pMemmp, nullptr, &WriteRomMmc1);
	pFam->m_pVMapr = pMapr1;
	pMapr1->m_aBReg[0] = 0xC;

/*
	// * Some tests have found (citation needed) that the power on behavior has the last 32k
	// mapped at 0x8000..0xFFFF
	MemoryDescriptor memdescReadOnly(FMEM_ReadOnly);
	auto addrspPrg0 = AddrspMapMemory(pMemmp, 0x6000, FF_KIB(8), &memdescReadOnly);
	auto addrspPrg1 = AddrspMapMemory(pMemmp, 0x6000, FF_KIB(8), &memdescReadOnly);
	TestMemoryMap(pMemmp);

	*/
	// if the ines file lists cBChr == 0 that means 0KiB ChrRom and 8KiB ChrRam
	bool fIsNes2 = pCart->m_pHead->m_romf.m_nFormat == 2;
	bool fIsChrRam = (!fIsNes2 && pCart->m_cBChrRom == 0);
	int cBChr = fIsChrRam ? FF_KIB(8) : pCart->m_cBChrRom;

	cBChr = ffMin<int>(FF_DIM(pFam->m_ppu.m_aBChr), cBChr);
	NTMIR ntmir = (pCart->m_pHead->m_romf.m_fMirrorVert) ? NTMIR_Vertical : NTMIR_Horizontal;
	InitPpuMemoryMap(&pFam->m_ppu, cBChr, ntmir);

	MemoryDescriptor memdescPrgMmc1(FMEM_None, pMapr1->m_iMemcbWriteMem);
	(void) AddrspMapMemory(pMemmp, 0x8000, 0x10000, &memdescPrgMmc1, 1);

	UpdateBanks(pFam, pMapr1);
	return true;
}

bool FTrySetupMapperUxRom(Famicom * pFam, Cart* pCart)
{
	auto pMemmp = &pFam->m_memmp;
	MapMemoryCommon(pMemmp);
	FF_ASSERT(false, "TBD");

	return false;
}

bool FTrySetupMemoryMapper(Famicom * pFam, Cart * pCart)
{
	switch(pCart->m_mapperk)
	{
	case MAPPERK_NROM:	return FTrySetupMapperNrom(pFam, pCart);
	case MAPPERK_MMC1:	return FTrySetupMapperMmc1(pFam, pCart);
	case MAPPERK_UxROM:	return FTrySetupMapperUxRom(pFam, pCart);
	default:
		ShowError("Unknown mapper type");
		return false;
	}
}

bool FTryLoadRomFromFile(const char * pChzFilename, Cart * pCart, Famicom * pFam, FPOW fpow)
{
	FILE * pFile = nullptr;
	FF_FOPEN(pFile, pChzFilename, "rb");
	if (!pFile)
		return false;

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

   fclose(pFile);
   return FTryLoadRom(pB, cB, pCart, pFam, fpow);
}

bool FTryLoadRom(u8 * pB, u64 cB, Cart * pCart, Famicom * pFam, FPOW fpow)
{
	SetPowerUpPreLoad(pFam, fpow);
	RomHeader * pHead = (RomHeader *)pB;

	static const u32 kRomCookie = 0x1a53454E; //"NES"
	if (pHead->m_nCookie != kRomCookie)
	{
		return false;
	}
	
	pCart->m_pHead = pHead;

	const RomFlags & romf  = pHead->m_romf;
	const RomFlagsEx & romfx  = pHead->m_romfx;

	bool fIsNes2 = romf.m_nFormat == 2;
	u32 mapperk = (romf.m_nMapperHigh << 0x4) | romf.m_nMapperLow;
	int sizePrgRomMSB = 0;
	int sizeChrRomMSB = 0;

	if (fIsNes2)
	{
		mapperk |= ((u16)romfx.m_nMapperMSB) << 8;
		sizePrgRomMSB = romfx.m_sizePrgRomMSB;
		sizeChrRomMSB = romfx.m_sizeChrRomMSB;
	}

	pCart->m_cBPrgRom = CPageRom(sizePrgRomMSB, pHead->m_cPagePrgRom) * 16 * 1024;
	pCart->m_cBChrRom = CPageRom(sizeChrRomMSB, pHead->m_cPageChrRom) * 8 * 1024;
	pCart->m_mapperk = (MAPPERK)mapperk;

	// after the header is the trainer 0 or 512 bytes
	u8 * pBTrainer = pB + sizeof(RomHeader);
	int cBTrainer = (romf.m_fHasTrainer) ? 512 : 0; 

	// PRG rom data
	pCart->m_pBPrgRom = pBTrainer + cBTrainer;

	// Chr rom data
	pCart->m_pBChrRom = pCart->m_pBPrgRom + pCart->m_cBPrgRom;

	if (!FTrySetupMemoryMapper(pFam, pCart))
	{
		return false;
	}

	SetPowerUpState(pFam, fpow);
	printf("RomLoaded: PRG %d KiB,  CHR %d KiB\n", pCart->m_cBPrgRom / 1024, pCart->m_cBChrRom / 1024);
	pFam->m_fIsRomLoaded = true;
	return true;
}

void CloseRom(Cart * pCart)
{

}