
#include "CPU.h"
#include "PPU.h"
#include "Rom.h"

#include <stdio.h>

u8 U8ReadAddress(Famicom * pFam, u16 addr)
{
	if (addr > 0x8000)
	{
		u16 addrAdj = (addr > 0xBFFF) ? (addr - 0xC000) : (addr - 0x8000);
		return pFam->m_pCart->m_pBPrgRom[addrAdj];
	}

	return 0;
}

u16 U16ReadAddress(Famicom * pFam, u16 addr)
{
	u16 n = U8ReadAddress(pFam, addr) | (u16(U8ReadAddress(pFam, addr+1)) << 8);
	return n;
}

int main(int cpChzArg, const char * apChzArg[])
{
	Famicom	fam;
	Cart cart;
	fam.m_pCart = &cart;

	if (cpChzArg > 1)
	{
		bool fLoaded = FTryLoadRomFromFile(apChzArg[1], &cart);
		if (!fLoaded)
		{
			printf("failed to load '%s'\n", apChzArg[1]);
			return 0;
		}

		u16 addrStartup = U16ReadAddress(&fam, kAddrReset);
		printf("loaded '%s'   reset vector = %x\n", apChzArg[1], addrStartup);
		DumpAsm(&fam, addrStartup, 100);
	}

	return 1;
}