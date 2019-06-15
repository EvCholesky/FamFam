#include "Cpu.h"
#include "Platform.h"
#include "PPU.h"
#include "stdio.h"

Ppu::Ppu()
:m_pctrl{0}
,m_pmask{0}
,m_pstatus{0}
,m_addrV(0)
,m_addrTemp(0)
,m_dXScrollFine(0)
,m_fIsFirstAddrWrite(true)
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

void AppendPpuCommand(PpuCommandList * pPpucl, PCMDK pcmdk, u16 addr, u8 bValue, const PpuTiming & ptim, u16 addrDebug)
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
	pPpucmd->m_addrInstDebug = addrDebug;
}

u8 * PBVram(Ppu * pPpu, u16 addr)
{
	addr &= 0x3FFF;
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
					case PPUREG_Status:
					{
						// reading from PPUCTRL $2000 clears the address latch used by PPUSCROLL and PPUADDR
						pPpu->m_fIsFirstAddrWrite = true;
					} break;
					case PPUREG_PpuData:
					{
						FF_ASSERT(false, "TBD");

						//pPpu->m_addrRegister += (pPpu->m_pctrl.m_dBVramAddressIncrement) ? 1 : 32;
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
					{
						if (pPpu->m_fIsFirstAddrWrite)
						{
							u16 addrT = (pPpu->m_addrTemp & 0xFFE0) | u16(pPpucmd->m_bValue & 0x7F);
							pPpu->m_addrTemp = addrT;
							pPpu->m_dXScrollFine = pPpucmd->m_bValue & 0x7;
						}
						else
						{
							u16 addrT = pPpu->m_addrTemp & 0x181F;
							addrT |= u16(pPpucmd->m_bValue & 0x7) << 13;
							addrT |= u16(pPpucmd->m_bValue & 0xF8) << 5;
							pPpu->m_addrTemp = addrT;
							 // doesn't set addr s here, it will be set when drawing starts ()
						}

						pPpu->m_fIsFirstAddrWrite = !pPpu->m_fIsFirstAddrWrite;
					} break;
				case PPUREG_PpuAddr:
					{
						if (pPpu->m_fIsFirstAddrWrite)
						{
							u16 addrT = pPpu->m_addrTemp;
							pPpu->m_addrTemp = (u16(pPpucmd->m_bValue & 0x7F) << 8) | (addrT & 0x00FF);
						}
						else 
						{
							u16 addrT = pPpu->m_addrTemp;
							addrT = (addrT & 0xFF00) | u16(pPpucmd->m_bValue);
							pPpu->m_addrTemp = addrT;
							pPpu->m_addrV = addrT;
						}

						pPpu->m_fIsFirstAddrWrite = !pPpu->m_fIsFirstAddrWrite;
					} break;
				case PPUREG_PpuData:
					{
						u8 * pB = PBVram(pPpu, pPpu->m_addrV);
						*pB = pPpucmd->m_bValue;
						static int s_index = 0;
					//	printf("%02X%s", pPpucmd->m_bValue, ((s_index++ % 32) == 31) ? "\n" : ",");

						pPpu->m_addrV += (pPpu->m_pctrl.m_dBVramAddressIncrement) ? 32 : 1;
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

void AdvancePpuTiming(Famicom * pFam, s64 cCycleCpu, MemoryMap * pMemmp)
{
	PpuTiming * pPtim = &pFam->m_ptimCpu;
	int dCycle = int(cCycleCpu - pPtim->m_cCycleCpuPrev);
	pPtim->m_cCycleCpuPrev = cCycleCpu;

	bool fBackgroundEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x8;
	bool fSpritesEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x10;
	bool fIsRenderEnabled = fBackgroundEnabled | fSpritesEnabled;

	s64 cFramePrev;
	s64 cPclockSubframePrev;
	SplitPclockFrame(pPtim->m_cPclock, &cFramePrev, &cPclockSubframePrev);

	pPtim->m_cPclock += dCycle * s_cPclockPerCpuCycle;

	s64 cFrame;
	s64 cPclockSubframe;
	SplitPclockFrame(pPtim->m_cPclock, &cFrame, &cPclockSubframe);

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
		// this doesn't handle toggling nmi_output while vblank is set
		bool fNmiOutput = pMemmp->m_aBRaw[PPUREG_Ctrl] & 0x80;
		if (fNmiOutput)
		{
			pFam->m_cpu.m_fTriggerNmi = true;
		}
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
		0xFF7C7C7C, 0xFFFC0000, 0xFFBC0000, 0xFFBC2844, 0xFF840094, 0xFF2000A8, 0xFF0010A8, 0xFF001488, 0xFF003050, 0xFF007800, 0xFF006800, 0xFF005800, 0xFF584000, 0xFF000000, 0xFF000000, 0xFF000000,
		0xFFBCBCBC, 0xFFF87800, 0xFFF85800, 0xFFFC4468, 0xFFCC00D8, 0xFF5800E4, 0xFF0038F8, 0xFF105CE4, 0xFF007CAC, 0xFF00B800, 0xFF00A800, 0xFF44A800, 0xFF888800, 0xFF000000, 0xFF000000, 0xFF000000,
		0xFFF8F8F8, 0xFFFCBC3C, 0xFFFC8868, 0xFFF87898, 0xFFF878F8, 0xFF9858F8, 0xFF5878F8, 0xFF44A0FC, 0xFF00B8F8, 0xFF18F8B8, 0xFF54D858, 0xFF98F858, 0xFFD8E800, 0xFF787878, 0xFF000000, 0xFF000000,
		0xFFFCFCFC, 0xFFFCE4A4, 0xFFF8B8B8, 0xFFF8B8D8, 0xFFF8B8F8, 0xFFC0A4F8, 0xFFB0D0F0, 0xFFA8E0FC, 0xFF78D8F8, 0xFF78F8D8, 0xFFB8F8B8, 0xFFD8F8B8, 0xFFFCFC00, 0xFFF8D8F8, 0xFF000000, 0xFF000000
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
	ZeroAB(pPpu->m_aPVramMap, FF_DIM(pPpu->m_aPVramMap));
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
	MapPpuMemorySpan(pPpu, 0x3C00, PBVram(pPpu, PADDR_NameTable3), 0x3F00 - 0x3C00);

	// palettes
	u16 addrPalette = PADDR_PaletteRam;
	while (addrPalette < PADDR_VramMax)
	{
		MapPpuMemorySpan(pPpu, addrPalette, pPpu->m_aBPalette, kCBPalette);
		addrPalette += kCBPalette;
	}
}

static const int s_dXTileScreen = 32;
static const int s_dYTileScreen = 30;
inline u16 FetchNametableLine(Ppu * pPpu, u16 addrBase, int xTile, int yTile, int ySubtile, u8 * pBIndices, u8 * pIAttribute)
{
	// return an array of 8 palette indices and the corresponding pallete index

	//FF_ASSERT(addr >= PADDR_NameTable0 && addr <= PADDR_PaletteRam, "bad nametable entry");

	// look up the nametable entry
	u8 * pBNameTable = PBVram(pPpu, addrBase);
	u16 iTile = pBNameTable[xTile + yTile * s_dXTileScreen];
	//u16 iTile = (xTile + yTile * s_dXTileScreen) & 0xFF;

	u16 addrPattern = (pPpu->m_pctrl.m_fHighBgPaternTable) ? 0x1000 : 0x0;
	u8 * pBPattern = PBVram(pPpu, addrPattern);

	int dB = (ySubtile & 0x7) | (iTile << 4);
	u8 bPlane0 = pBPattern[dB];
	u8 bPlane1 = pBPattern[dB | 0x8];

	for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
	{
		int nShift = 7-xSubtile;
		pBIndices[xSubtile] = ((bPlane0 >> nShift) & 0x1) | (((bPlane1 >> nShift) & 0x1) << 1);
	}

	static const int s_dXAttributeCell = 8;
	static const int s_dBAttribute = s_dXTileScreen * s_dYTileScreen;
	u8 * pBAttribute = pBNameTable + s_dBAttribute;

	u8 bAttribute = pBAttribute[(xTile>>2) + ((yTile>>2) * s_dXAttributeCell)];
	int xCell = xTile >> 1;
	int yCell = yTile >> 1;
	int cBitShift = (xCell & 0x1) ? 2 : 0;
	cBitShift += (yCell & 0x1) ? 4 : 0;
	*pIAttribute = (bAttribute >> cBitShift) & 0x3;

	return iTile;
}

void DrawNameTableMemory(Ppu * pPpu, Texture * pTex)
{
	u8 aIBitmap[8];
	u8 iPalette;

	// look up hwcol palettes
	/*
	HWCOL aHwcolPalette[4 * 4];
	static_assert(sizeof(aHwcolPalette) == sizeof(pPpu->m_aBPalette), "palette size mismatch");
	CopyAB(m_aBPalette, aHwcolPalette, sizeof(aHwcolPalette));
	HWCOL hwcolBackground = aHwcolPalette[0];
	aHwcolPalette[0x4] = hwcolBackground;
	aHwcolPalette[0x8] = hwcolBackground;
	aHwcolPalette[0xC] = hwcolBackground;
	*/

	static const int s_cRgbaPalette = 4;
	static const int s_cPalette = 4;
	RGBA aRgba[s_cRgbaPalette * s_cPalette];
	RGBA RgbaFromHwcol(HWCOL hwcol);
	for (int iRgb = 0; iRgb < FF_DIM(aRgba); ++iRgb)
	{
		// the first index in each palette is the shared background color
		int iHwcol = ((iRgb % s_cRgbaPalette) == 0) ? 0 : iRgb;
		aRgba[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol]));
	}

	static const int s_dXSubtile = 8;
	static const int s_dYSubtile = 8;
	int dXStride = pTex->m_dX;
	int dBNametableX = 32 * 8 * 3;
	int dBNametableY = 30 * 8 * 3 * dXStride;
	for (int yTile = 0; yTile < s_dYTileScreen; ++yTile)
	{
		for (int ySubtile = 0; ySubtile < s_dYSubtile; ++ySubtile)
		{
			for (int xTile = 0; xTile < s_dXTileScreen; ++xTile)
			{
				int dB = (xTile*s_dXSubtile + ((yTile*s_dYSubtile)+ ySubtile) * dXStride) * 3;

				auto iTile = FetchNametableLine(pPpu, 0x2000, xTile, yTile, ySubtile, aIBitmap, &iPalette);
				u8 * pB = pTex->m_pB + dB;
				for (int xSubtile = 0; xSubtile < s_dXSubtile; ++xSubtile)
				{
					RGBA rgba = aRgba[aIBitmap[xSubtile] + iPalette * s_cRgbaPalette];
					*pB++ = rgba.m_r;
					*pB++ = rgba.m_g;
					*pB++ = rgba.m_b;
				}

				FetchNametableLine(pPpu, 0x2400, xTile, yTile, ySubtile, aIBitmap, &iPalette);
				pB = pTex->m_pB + dB + dBNametableX;
				for (int xSubtile = 0; xSubtile < s_dXSubtile; ++xSubtile)
				{
					RGBA rgba = aRgba[aIBitmap[xSubtile] + iPalette * s_cRgbaPalette];
					*pB++ = rgba.m_r;
					*pB++ = rgba.m_g;
					*pB++ = rgba.m_b;
				}

				FetchNametableLine(pPpu, 0x2800, xTile, yTile, ySubtile, aIBitmap, &iPalette);
				pB = pTex->m_pB + dB + dBNametableY;
				for (int xSubtile = 0; xSubtile < s_dXSubtile; ++xSubtile)
				{
					RGBA rgba = aRgba[aIBitmap[xSubtile] + iPalette * s_cRgbaPalette];
					*pB++ = rgba.m_r;
					*pB++ = rgba.m_g;
					*pB++ = rgba.m_b;
				}

				FetchNametableLine(pPpu, 0x2C00, xTile, yTile, ySubtile, aIBitmap, &iPalette);
				pB = pTex->m_pB + dB + dBNametableX + dBNametableY;
				for (int xSubtile = 0; xSubtile < s_dXSubtile; ++xSubtile)
				{
					RGBA rgba = aRgba[aIBitmap[xSubtile] + iPalette * s_cRgbaPalette];
					*pB++ = rgba.m_r;
					*pB++ = rgba.m_g;
					*pB++ = rgba.m_b;
				}
			}
		}
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

