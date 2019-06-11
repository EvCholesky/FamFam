#include "Cpu.h"
#include "Platform.h"
#include "PPU.h"
#include "stdio.h"

Ppu::Ppu()
:m_pctrl{0}
,m_pmask{0}
,m_pstatus{0}
,m_addrRegister(0)
,m_fSetAddressLSB(false)
{
	ZeroAB(m_aBCieram, FF_DIM(m_aBCieram));
	ZeroAB(m_aBChr, FF_DIM(m_aBChr));
	ZeroAB(m_aBPalette, FF_DIM(m_aBPalette));
	ZeroAB(m_aPVramMap, FF_DIM(m_aPVramMap));

	ZeroAB(m_aOam, FF_DIM(m_aOam));
	ZeroAB(m_aOamSecondary, FF_DIM(m_aOamSecondary));
}

void ClearPpuCommandList(PpuCommandList * pPpucl)
{
	pPpucl->m_iPpucmdExecute = 0;
	pPpucl->m_aryPpucmd.Clear();
}

void AppendPpuCommand(PpuCommandList * pPpucl, PCMDK pcmdk, u16 addr, u8 bValue, const PpuTiming & ptim)
{
#if DEBUG
	auto pPpucmdPrev = pPpucl->m_aryPpucmd.PLast();
	FF_ASSERT(pPpucmdPrev->m_addr < addr, "change list addres out of order");
	FF_ASSERT(pPpucmdPrev->m_cPclock < cPclock, "change list addres out of order");
#endif

	auto pPpucmd = pPpucl->m_aryPpucmd.AppendNew();
	pPpucmd->m_addr = addr;
	pPpucmd->m_pcmdk = pcmdk;
	pPpucmd->m_bValue = bValue;
	pPpucmd->m_cPclock = ptim.m_cPclock;
}

u8 * PBVram(Ppu * pPpu, u16 addr)
{
	FF_ASSERT(addr < kCBVramAddressable, "bad vram address");
	u8 * pBGranule = pPpu->m_aPVramMap[addr / s_cBVramGranularity];
	return pBGranule + (addr % s_cBVramGranularity);
}

void UpdatePpu(Famicom * pFam, const PpuTiming & ptimEnd)
{
	auto pPpucl = &pFam->m_ppucl;

	int iPpucmd = pPpucl->m_iPpucmdExecute;
	Ppu * pPpu = &pFam->m_ppu;

	while (pFam->m_cPclockPpu < ptimEnd.m_cPclock)
	{
		s64 cPclockNext; 
		if (iPpucmd >= pPpucl->m_aryPpucmd.C())
		{
			cPclockNext = ptimEnd.m_cPclock;
		}
		else
		{
			PpuCommand * pPpucmd = &pPpucl->m_aryPpucmd[iPpucmd];
			cPclockNext = pPpucmd->m_cPclock;
			++iPpucmd;

			if (pPpucmd->m_pcmdk == PCMDK_Read)
			{
				switch (pPpucmd->m_addr)
				{
					case PPUREG_Ctrl:
					{
						// reading from PPUCTRL $2000 clears the address latch used by PPUSCROLL and PPUADDR
						pPpu->m_fSetAddressLSB = false;
					} break;
					case PPUREG_PpuData:
					{
						FF_ASSERT(false, "TBD");

						pPpu->m_addrRegister += (pPpu->m_pctrl.m_dBVramAddressIncrement) ? 1 : 32;
					} break;
					default:
						break;
				}
			}
			else
			{
				FF_ASSERT(pPpucmd->m_pcmdk == PCMDK_Write, "unknown PPU change kind");
				switch (pPpucmd->m_addr)
				{
				case PPUREG_Ctrl:
					pPpu->m_pctrl.m_nBits = pPpucmd->m_bValue;
					break;
				case PPUREG_Mask:
					pPpu->m_pmask.m_nBits = pPpucmd->m_bValue;
					break;
				case PPUREG_Status:
					pPpu->m_pstatus.m_nBits = pPpucmd->m_bValue;
					break;

				case PPUREG_PpuScroll:
				case PPUREG_PpuAddr:
					{
						u16 addr = pPpu->m_addrRegister;
						if (pPpu->m_fSetAddressLSB)	
						{
							addr = (addr & 0xFF00) | u16(pPpucmd->m_bValue);
						}
						else
						{
							addr = (pPpucmd->m_bValue & 0x0FF) | (addr & 0x00FF);
						}
						FF_ASSERT(addr < kCBVramAddressable, "bad address");

						pPpu->m_addrRegister = addr;
						pPpu->m_fSetAddressLSB = true;
					} break;
				case PPUREG_PpuData:
					{
						if (pPpu->m_addrRegister == 0x301f)
						{
							printf("");
						}

						u8 * pB = PBVram(pPpu, pPpu->m_addrRegister);
						*pB = pPpucmd->m_bValue;

						pPpu->m_addrRegister += (pPpu->m_pctrl.m_dBVramAddressIncrement) ? 32 : 1;
					} break;
				default:
					break;
				}
				
			}
		}

		// update ppu until the new time
		pFam->m_cPclockPpu = cPclockNext;
	}

	pPpucl->m_aryPpucmd.RemoveSpan(0, iPpucmd);
	pPpucl->m_iPpucmdExecute = 0;
}

void AdvancePpuTiming(PpuTiming * pPtim, s64 cCycleCpu, MemoryMap * pMemmp)
{
	int dCycle = int(cCycleCpu - pPtim->m_cCycleCpuPrev);
	pPtim->m_cCycleCpuPrev = cCycleCpu;

	bool fBackgroundEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x8;
	bool fSpritesEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x10;
	bool fIsRenderEnabled = fBackgroundEnabled | fSpritesEnabled;

	s64 cFramePrev;
	s64 cPclockSubframePrev;
	SplitPclockFrame(pPtim->m_cPclock, &cFramePrev, &cPclockSubframePrev);

#define TEST_OLD_PPU_TIMING 0
#if TEST_OLD_PPU_TIMING
	int cScanlinePrev;
	int cPclockScanlinePrev;
	SplitPclockScanline(pPtim->m_cPclock, &cScanlinePrev, &cPclockScanlinePrev);
#endif

	pPtim->m_cPclock += dCycle * s_cPclockPerCpuCycle;

	s64 cFrame;
	s64 cPclockSubframe;
	SplitPclockFrame(pPtim->m_cPclock, &cFrame, &cPclockSubframe);

#if TEST_OLD_PPU_TIMING
	s64 cFrameTest = cFramePrev;
	s64 cScanlineTest = cScanlinePrev;
	s64 cPclockScanlineTest = cPclockScanlinePrev;

	cPclockScanlineTest += dCycle * 3;
	if (cPclockScanlineTest >= s_cPclockPerScanline)
	{
		++cScanlineTest;
		cPclockScanlineTest -= s_cPclockPerScanline;

		if (cScanlineTest >= s_cScanlinesPerFrame)
		{
			cScanlineTest -= s_cScanlinesPerFrame;
			++cFrameTest;
		}
	}

	//int cPclock = CPclockThisFrame(pPtim);

	int cScanline;
	int cPclockScanline;
	SplitPclockScanline(pPtim->m_cPclock, &cScanline, &cPclockScanline);
	FF_ASSERT(cFrame == cFrameTest, "bad frame  count");
	FF_ASSERT(cScanline == cScanlineTest, "bad scanline  count");
	FF_ASSERT(cPclockScanline == cPclockScanlineTest, "bad pclock  count");
#endif

	// NOTE: we're skipping the first pclock following an odd frame.
	bool fIsOddFrame = (cFramePrev & 0x1) != 0;

	static const int s_pclockVBlankSet = 241 * s_cPclockPerScanline + 1; // vblank happens on the first cycle of the 241st scanline of a frame.
	static const int s_pclockStatusClear = 261 * s_cPclockPerScanline + 1; // vblank is cleared  on the first cycle of the prerender scanline.
	if (fIsRenderEnabled && fIsOddFrame && cFramePrev != cFrame)
	{
		// Odd frames (with rendering enabled) are one cycle shorter - this cycle is dropped from the
		//  first idle cycle of the  first scanline
		++pPtim->m_cPclock;

		FF_ASSERT(dCycle * s_cPclockPerCpuCycle < s_cPclockPerFrame, "not handling a full frame step here");
		++cPclockSubframe;
	}

	if (s_pclockVBlankSet > cPclockSubframePrev && s_pclockVBlankSet <= cPclockSubframe)
	{
		pMemmp->m_aBRaw[PPUREG_Status] |= 0x80;
	}
	else if (s_pclockStatusClear > cPclockSubframePrev && s_pclockStatusClear <= cPclockSubframe)
	{
		pMemmp->m_aBRaw[PPUREG_Status] = 0;
	}
}

RGBA RgbaFromHwcol(HWCOL hwcol)
{
	// lifted from https://github.com/AndreaOrru/LaiNES/blob/master/src/palette.inc
	static const RGBA s_mpIcolRgba[] =
	{ 
		0x7C7C7C, 0x0000FC, 0x0000BC, 0x4428BC, 0x940084, 0xA80020, 0xA81000, 0x881400, 0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
		0xBCBCBC, 0x0078F8, 0x0058F8, 0x6844FC, 0xD800CC, 0xE40058, 0xF83800, 0xE45C10, 0xAC7C00, 0x00B800, 0x00A800, 0x00A844, 0x008888, 0x000000, 0x000000, 0x000000,
		0xF8F8F8, 0x3CBCFC, 0x6888FC, 0x9878F8, 0xF878F8, 0xF85898, 0xF87858, 0xFCA044, 0xF8B800, 0xB8F818, 0x58D854, 0x58F898, 0x00E8D8, 0x787878, 0x000000, 0x000000,
		0xFCFCFC, 0xA4E4FC, 0xB8B8F8, 0xD8B8F8, 0xF8B8F8, 0xF8A4C0, 0xF0D0B0, 0xFCE0A8, 0xF8D878, 0xD8F878, 0xB8F8B8, 0xB8F8D8, 0x00FCFC, 0xF8D8F8, 0x000000, 0x000000
	};

	if (hwcol < HWCOL_Min || hwcol >= HWCOL_Max)
	{
		FF_ASSERT(false, "bad hardware color index %d", hwcol);
		return RGBA{0};
	}

	return s_mpIcolRgba[hwcol];
}

void MapPpuMemorySpan(Ppu * pPpu, u16 addrMin, u8 * pB, int cB)
{
	int cSpan = cB / s_cBVramGranularity;
	u8 ** ppBMap = &pPpu->m_aPVramMap[addrMin / s_cBVramGranularity];
	u8 * pBTarget = pB;
	for (int iSpan = 0; iSpan < cSpan; ++iSpan)
	{
		*ppBMap = pBTarget;
		pBTarget += s_cBVramGranularity;
		++ppBMap;
	}
}

void InitPpuMemoryMap(Ppu * pPpu, u8 * pBChr, int cBChr, NTMIR ntmir)
{
	CopyAB(pBChr, pPpu->m_aBChr, cBChr);

	FF_ASSERT(cBChr == FF_KIB(8), "expected 8k, need to handle odd size");
	MapPpuMemorySpan(pPpu, 0, pPpu->m_aBChr, cBChr);

	static const int s_cBNameTable = FF_KIB(1);
	u8 * pBNameTable0 = &pPpu->m_aBCieram[0];
	u8 * pBNameTable1 = &pPpu->m_aBCieram[s_cBNameTable];

	switch (ntmir)
	{
	case NTMIR_Vertical:
		MapPpuMemorySpan(pPpu, 0x2000, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2400, pBNameTable1, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2800, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2C00, pBNameTable1, s_cBNameTable);
		break;
	case NTMIR_Horizontal:
		MapPpuMemorySpan(pPpu, 0x2000, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2400, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2800, pBNameTable1, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2C00, pBNameTable1, s_cBNameTable);
		break;
	case NTMIR_OneScreen:
		MapPpuMemorySpan(pPpu, 0x2000, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2400, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2800, pBNameTable0, s_cBNameTable);
		MapPpuMemorySpan(pPpu, 0x2C00, pBNameTable0, s_cBNameTable);
		break;
	case NTMIR_FourScreen:
		{
			u8 * pBNameTable2 = &pPpu->m_aBCieram[s_cBNameTable * 2];
			u8 * pBNameTable3 = &pPpu->m_aBCieram[s_cBNameTable * 3];
			MapPpuMemorySpan(pPpu, 0x2000, pBNameTable0, s_cBNameTable);
			MapPpuMemorySpan(pPpu, 0x2400, pBNameTable1, s_cBNameTable);
			MapPpuMemorySpan(pPpu, 0x2800, pBNameTable2, s_cBNameTable);
			MapPpuMemorySpan(pPpu, 0x2C00, pBNameTable3, s_cBNameTable);
		}
		break;
	}

	MapPpuMemorySpan(pPpu, 0x3000, PBVram(pPpu, PADDR_NameTable0), s_cBNameTable);
	MapPpuMemorySpan(pPpu, 0x3400, PBVram(pPpu, PADDR_NameTable1), s_cBNameTable);
	MapPpuMemorySpan(pPpu, 0x3800, PBVram(pPpu, PADDR_NameTable2), s_cBNameTable);
	MapPpuMemorySpan(pPpu, 0x3C00, PBVram(pPpu, PADDR_NameTable3), s_cBNameTable - kCBPalette);

	// palettes
	u16 addrPalette = PADDR_PaletteRam;
	while (addrPalette < PADDR_VramMax)
	{
		MapPpuMemorySpan(pPpu, addrPalette, pPpu->m_aBPalette, kCBPalette);
		addrPalette += kCBPalette;
	}
}

void DrawChrMemory(Ppu * pPpu, Texture * pTex, bool fUse8x16)
{
	static const int s_cBPerPixel = 3; // bytes per pixel
	int yStride = 64; 
	auto pBOutLeft = pTex->m_pB;
	auto pBOutRight = &pTex->m_pB[s_dYChrHalf * s_cBPerPixel];
	u8 * pBChr = pPpu->m_aBChr;

	int cSubtile = 8;
	int dYTile = 8;
	int dYStride = s_dYChrHalf * 2;

	FF_ASSERT(pTex->m_dX == dYStride && pTex->m_dY == s_dYChrHalf, "DrawChrMemory is not very flexible yet.");	

	for (int yTile = 0; yTile < s_cYChrTile; ++yTile)
	{
		for (int xTile = 0; xTile < s_cXChrTile; ++xTile)
		{
			for (int ySubtile = 0; ySubtile < cSubtile; ++ySubtile)
			{
				u8 bPlaneL0 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile ];
				u8 bPlaneL1 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile | 0x08];

				u8 bPlaneR0 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile | 0x1000];
				u8 bPlaneR1 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile | 0x1008];

				int yLine;
				int iPixel;
				if (fUse8x16)
				{
					yLine = ((yTile & 0xFE) + (xTile & 0x1)) * dYTile + ySubtile;
					iPixel = ((xTile >> 1) + ((yTile & 0x1) * 8))*8 + (yLine*dYStride);
				}
				else
				{
					yLine = yTile*dYTile + ySubtile;
					iPixel = xTile*8 + (yLine*dYStride);
				}

				for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
				{
					u16 i = (bPlaneL0 >> (7-xSubtile)) & 0x1;
					i |= ((bPlaneL1 >> (7-xSubtile)) & 0x1) << 1;

					auto pBLeft = &pBOutLeft[(iPixel + xSubtile) * s_cBPerPixel];
					int nValue = 32 + i * 64; 
					pBLeft[0] = nValue;
					pBLeft[1] = nValue;
					pBLeft[2] = nValue;

					i = (bPlaneR0 >> (7-xSubtile)) & 0x1;
					i |= ((bPlaneR1 >> (7-xSubtile)) & 0x1) << 1;

					auto pBRight = &pBOutRight[(iPixel + xSubtile) * s_cBPerPixel];
					nValue = 32 + i * 64; 
					pBRight[0] = nValue;
					pBRight[1] = nValue;
					pBRight[2] = nValue;
				}
			}
		}
	}
}



void DrawNametables()
{
	// 30x32 table where each byte corresponds with one 8-pixel x 8-pixel region

}
