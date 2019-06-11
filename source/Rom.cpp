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

void AppendAddrOffset(DynAry<u16> * pAryAddrInstruct, u16 addrBase, u8 * pBPrg, int cB)
{
	FF_ASSERT(pAryAddrInstruct->FIsEmpty() || pAryAddrInstruct->Last() < addrBase, "instructions added out of order");

	int dB = 0;
	while (dB < cB)
	{
		u8 bOpcode = pBPrg[dB];
		auto pOpinfo = POpinfoFromOpcode(bOpcode); 
		auto pOpkinfo = POpkinfoFromOPK(pOpinfo->m_opk);
		auto pAmrwinfo = PAmrwinfoFromAmrw(pOpinfo->m_amrw);

		pAryAddrInstruct->Append(dB + addrBase);
		dB += pAmrwinfo->m_cB;
	}
}

void MapMemoryCommon(MemoryMap * pMemmp)
{
	auto addrspBottom = AddrspMapMemory(pMemmp, 0x0, 0x800);		// bottom 2K
	MapMirrored(pMemmp, addrspBottom, 0x800, 0x2000);				// mirrors to 8K

	auto iMemcbWriteOnly = IMemcbAllocate(pMemmp, U8ReadPpuRegWriteOnly, WritePpuReg);
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

	auto addrspApuIoReg = AddrspMapMemory(pMemmp, 0x4000, 0x4018);	// APU and IO Registers
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
			auto addrspPrg = AddrspMapMemory(pMemmp, 0x8000, 0xC000, &memdescReadOnly);
			MapMirrored(pMemmp, addrspPrg, 0xC000, 0x10000);
			CopyAB(pCart->m_pBPrgRom, &pMemmp->m_aBRaw[0x8000], FF_KIB(16));

			AppendAddrOffset(&pCart->m_aryAddrInstruct, 0x8000, pCart->m_pBPrgRom, pCart->m_cBPrgRom);
			AppendAddrOffset(&pCart->m_aryAddrInstruct, 0xC000, pCart->m_pBPrgRom, pCart->m_cBPrgRom);

			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0xC000);
			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0xC000);
		}break;
	case FF_KIB(32):
		{
			(void)AddrspMapMemory(pMemmp, 0x8000, 0x10000, &memdescReadOnly);
			CopyAB(pCart->m_pBPrgRom, &pMemmp->m_aBRaw[0x8000], FF_KIB(32));

			AppendAddrOffset(&pCart->m_aryAddrInstruct, 0x8000, pCart->m_pBPrgRom, pCart->m_cBPrgRom);

			VerifyPrgRom(pMemmp, pCart->m_pBPrgRom, 0x8000, 0x10000);
		} break;

	default:
		ShowError("unexpected PRG rom size %d bytes", pCart->m_cBPrgRom);
		return false;
	}

	//TestMemoryMap(pMemmp);

	int cBChr = min<int>(FF_DIM(pFam->m_ppu.m_aBChr), pCart->m_cBChrRom);
	NTMIR ntmir = (pCart->m_pHead->m_romf.m_fMirrorVert) ? NTMIR_Vertical : NTMIR_Horizontal;
	InitPpuMemoryMap(&pFam->m_ppu, pFam->m_ppu.m_aBChr, cBChr, ntmir);
	return true;
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

	// * Some tests have found (citation needed) that the power on behavior has the last 32k
	// mapped at 0x8000..0xFFFF

	MemoryDescriptor memdescReadOnly(FMEM_ReadOnly);
	auto addrspPrg0 = AddrspMapMemory(pMemmp, 0x6000, FF_KIB(8), &memdescReadOnly);
	auto addrspPrg1 = AddrspMapMemory(pMemmp, 0x6000, FF_KIB(8), &memdescReadOnly);
	TestMemoryMap(pMemmp);

	int cBChr = min<int>(FF_DIM(pFam->m_ppu.m_aBChr), pCart->m_cBChrRom);
	NTMIR ntmir = (pCart->m_pHead->m_romf.m_fMirrorVert) ? NTMIR_Vertical : NTMIR_Horizontal;
	InitPpuMemoryMap(&pFam->m_ppu, pFam->m_ppu.m_aBChr, cBChr, ntmir);
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

	auto pPpu = &pFam->m_ppu;
	int cBChr = min<int>(FF_DIM(pPpu->m_aBChr), pCart->m_cBChrRom);
	CopyAB(pCart->m_pBChrRom, pPpu->m_aBChr, cBChr);

	if (!FTrySetupMemoryMapper(pFam, pCart))
	{
		return false;
	}

	SetPowerUpState(pFam, fpow);
	printf("RomLoaded: PRG %d KiB,  CHR %d KiB\n", pCart->m_cBPrgRom / 1024, pCart->m_cBChrRom / 1024);
	return true;
}

void CloseRom(Cart * pCart)
{

}