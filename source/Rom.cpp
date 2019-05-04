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

	// Chr rom data
	pCart->m_pBChrRom = pCart->m_pBPrgRom + pCart->m_cBPrgRom;

	printf("RomLoaded: PRG %d KiB,  CHR %d KiB\n", pCart->m_cBPrgRom / 1024, pCart->m_cBChrRom / 1024);
	return true;
}

void CloseRom(Cart * pCart)
{

}