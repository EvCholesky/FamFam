#include "Platform.h"
#include "Rom.h"

#include <stdio.h>
#include <stdlib.h>         // NULL, malloc

bool FTryLoadRomFromFile(const char * pChzFilename, Cart * pCart)
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
   return FTryLoadRom(pB, cB, pCart);
}

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

//void MapMemorySlicesToRam(MemoryMap * pMemmp, u16 addrMin, u16 addrMax)
//void MapMemorySlicesToRom(MemoryMap * pMemmp, u16 addrMin, u8 * pBRom, int cBRom)
//void MapMirroredMemorySlices(MemoryMap * pMemmp, u16 addSrc, int cB, u16 addrMirrorMin, u16 addrMirrorMax)
//void UnmapMemorySlices(MemoryMap * pMemmp, u16 addrMin, u8 * pBRom, int cBRom)

void MapMemoryCommon(MemoryMap * pMemmp)
{
	/*
	MapMemorySlicesToRam(pMemmp, 0x0, 0x800);					// bottom 2K
	MapMirroredMemorySlices(pMemmp, 0, 0x800, 0x800, 0x2000);	// mirrors to 8K

	MapMemorySlicesToRam(pMemmp, 0x2000, 0x8);	// PPU registers
	MapMirroredMemorySlices(pMemmp, 0x2000, 0x8, 0x2008, 0x4000);	// mirrors to 16k

	MapMemorySlicesToRam(pMemmp, 0x4000, 0x18);						// APU and IO Registers
	MapMirroredMemorySlices(pMemmp, 0x4000, 0x18, 0x4018, 0x401F);	//
	*/

	auto pMemslBase = PMemslMapRam(pMemmp, 0x0, 0x800);				// bottom 2K
	MapMirrored(pMemmp, pMemslBase, 0x800, 0x2000);					// mirrors to 8K

	auto pMemslPpuReg = PMemslMapRam(pMemmp, 0x2000, 0x8);			// PPU registers
	MapMirrored(pMemmp, pMemslPpuReg, 0x2008, 0x4000);				// mirrors to 16k

	auto pMemslApuReg = PMemslMapRam(pMemmp, 0x4000, 0x18);			// APU and IO Registers
	(void)PMemslConfigureUnmapped(pMemmp, 0x4018, 0x4020);			// unused APU and IO test space
	//MapMirrored(pMemmp, pMemslApuReg, 0x4018, 0x401F);
}

bool FTrySetupMapperNrom(MemoryMap* pMemmp, Cart* pCart)
{
	MapMemoryCommon(pMemmp);
	(void)PMemslConfigureUnmapped(pMemmp, 0x4020, 0x8000);
	
	switch (pCart->m_cBPrgRom)
	{
	case FF_KIB(16):
		{
			auto pMemslPrg = PMemslMapRom(pMemmp, 0x8000, FF_KIB(16));
			MapMirrored(pMemmp, pMemslPrg, 0xC000, 0x10000);
		}break;
	case FF_KIB(32):
		(void)PMemslMapRom(pMemmp, 0x8000, FF_KIB(32));
		break;

	default:
		ShowError("unexpected PRG rom size %d bytes", pCart->m_cBPrgRom);
		return false;
	}

	TestMemoryMap(pMemmp);
	return true;
}

bool FTrySetupMapperMmc1(MemoryMap* pMemmp, Cart* pCart)
{
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
		auto pMemslPrgRam = PMemslMapRam(pMemmp, 0x6000, FF_KIB(8));
	}

	(void)PMemslConfigureUnmapped(pMemmp, 0x4020, addrUnmappedMax);

	// * Some tests have found (citation needed) that the power on behavior has the last 32k
	// mapped at 0x8000..0xFFFF

	auto pMemslPrg0 = PMemslMapRom(pMemmp, 0x6000, FF_KIB(8));
	auto pMemslPrg1 = PMemslMapRom(pMemmp, 0x6000, FF_KIB(8));
	TestMemoryMap(pMemmp);
	return true;
}

bool FTrySetupMapperUxrRom(MemoryMap* pMemmp, Cart* pCart)
{
	MapMemoryCommon(pMemmp);
	FF_ASSERT(false, "TBD");

	return false;
}

bool FTrySetupMemoryMapper(MemoryMap * pMemmp, Cart * pCart)
{
	switch(pCart->m_mapperk)
	{
	case MAPPERK_NROM:	return FTrySetupMapperNrom(pMemmp, pCart);
	case MAPPERK_MMC1:	return FTrySetupMapperMmc1(pMemmp, pCart);
	case MAPPERK_UxROM:	return FTrySetupMapperUxRom(pMemmp, pCart);
	default:
		ShowError("Unknown mapper type");
		return false;
	}
}

bool FTryLoadRom(u8 * pB, u64 cB, Cart * pCart)
{
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

	//ResizeBuffer(&pCart->m_bufPrgRom, cPagePrgRom * 16);
	//ResizeBuffer(&pCart->m_bufChrRom, cPageChrRom * 8);
	//pFam->m_pMapper = PMapperFromMapperk(MAPPERK(mapperk));

	// after the header is the trainer 0 or 512 bytes
	u8 * pBTrainer = pB + sizeof(RomHeader);
	int cBTrainer = (romf.m_fHasTrainer) ? 512 : 0; 

	// PRG rom data
	pCart->m_pBPrgRom = pBTrainer + cBTrainer;
	AppendAddrOffset(&pCart->m_aryAddrInstruct, 0xC000, pCart->m_pBPrgRom, pCart->m_cBPrgRom);

	// Chr rom data
	pCart->m_pBChrRom = pCart->m_pBPrgRom + pCart->m_cBPrgRom;

	printf("RomLoaded: PRG %d KiB,  CHR %d KiB\n", pCart->m_cBPrgRom / 1024, pCart->m_cBChrRom / 1024);
	return true;
}

void CloseRom(Cart * pCart)
{

}