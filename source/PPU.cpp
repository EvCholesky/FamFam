#include "Cpu.h"
#include "Platform.h"
#include "PPU.h"
#include "stdio.h"

static const int s_cRgbaPalette = 4;

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
	ZeroAB(m_aOamLine, FF_DIM(m_aOamLine));
}

void ClearPpuCommandList(PpuCommandList * pPpucl)
{
	pPpucl->m_iPpucmdExecute = 0;
	pPpucl->m_aryPpucmd.Clear();
}

void AppendPpuCommand(PpuCommandList * pPpucl, PCMDK pcmdk, u16 addr, u8 bValue, const PpuTiming & ptim, u16 addrDebug)
{
	auto pPpucmd = pPpucl->m_aryPpucmd.AppendNew();
	pPpucmd->m_addr = addr;
	pPpucmd->m_pcmdk = pcmdk;
	pPpucmd->m_bValue = bValue;
	pPpucmd->m_tickp = ptim.m_tickp;
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

	size_t iPpucmd = pPpucl->m_iPpucmdExecute;
	Ppu * pPpu = &pFam->m_ppu;

	while (pFam->m_tickp < ptimEnd.m_tickp)
	{
		s64 tickpNext; 
		if (iPpucmd >= pPpucl->m_aryPpucmd.C())
		{
			tickpNext = ptimEnd.m_tickp;
		}
		else
		{
			PpuCommand * pPpucmd = &pPpucl->m_aryPpucmd[iPpucmd];
			tickpNext = pPpucmd->m_tickp;
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

						pPpu->m_addrV += (pPpu->m_pctrl.m_dBVramAddressIncrement) ? 32 : 1;
					} break;
				case PPUREG_OAMAddr:
					{
						pPpu->m_bOamAddr = pPpucmd->m_bValue;
					} break;
				case PPUREG_OAMData:
					{
						// This is generally a bad way to write to OAM memory, 
						// It's known to cause corruption unless all 256 bytes are written, and there's not time to 
						// write them all during vblank

						auto pB = (u8 *)pPpu->m_aOam;
						pB[pPpu->m_bOamAddr++] = pPpucmd->m_bValue;
					} break;
				case PPUREG_OAMDMA:
					{
						// Handled by the CPU's memory write callback 
					} break;
				default:
					break;
				}
				
			}
		}

		// update ppu until the new time

		DrawScreenSimple(pPpu, pFam->m_tickp, tickpNext);
		pFam->m_tickp = tickpNext;
	}


	pPpucl->m_aryPpucmd.RemoveSpan(0, iPpucmd);
	pPpucl->m_iPpucmdExecute = 0;
}

void AdvancePpuTiming(Famicom * pFam, s64 tickc, MemoryMap * pMemmp)
{
	PpuTiming * pPtim = &pFam->m_ptimCpu;
	int dTickc = int(tickc - pPtim->m_tickcPrev);
	pPtim->m_tickcPrev = tickc;

	bool fBackgroundEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x8;
	bool fSpritesEnabled = pMemmp->m_aBRaw[PPUREG_Mask] & 0x10;
	bool fIsRenderEnabled = fBackgroundEnabled | fSpritesEnabled;

	s64 cFramePrev;
	s64 tickpSubframePrev;
	SplitTickpFrame(pPtim->m_tickp, &cFramePrev, &tickpSubframePrev);

	pPtim->m_tickp += dTickc * s_dTickpPerTickc;

	s64 cFrame;
	s64 tickpSubframe;
	SplitTickpFrame(pPtim->m_tickp, &cFrame, &tickpSubframe);

	// NOTE: we're skipping the first tickp following an odd frame.
	bool fIsOddFrame = (cFramePrev & 0x1) != 0;

	static const int s_tickpVBlankSet = 241 * s_dTickpPerScanline + 1; // vblank happens on the first cycle of the 241st scanline of a frame.
	static const int s_tickpStatusClear = 261 * s_dTickpPerScanline + 1; // vblank is cleared  on the first cycle of the prerender scanline.
	if (fIsRenderEnabled & fIsOddFrame & (cFramePrev != cFrame))
	{
		// Odd frames (with rendering enabled) are one cycle shorter - this cycle is dropped from the
		//  first idle cycle of the  first scanline
		++pPtim->m_tickp;

		FF_ASSERT(dTickc * s_dTickpPerTickc < s_dTickpPerFrame, "not handling a full frame step here");
		++tickpSubframe;
	}

	if ((s_tickpVBlankSet > tickpSubframePrev) & (s_tickpVBlankSet <= tickpSubframe))
	{
		// this doesn't handle toggling nmi_output while vblank is set
		bool fNmiOutput = pMemmp->m_aBRaw[PPUREG_Ctrl] & 0x80;
		if (fNmiOutput)
		{
			pFam->m_cpu.m_fTriggerNmi = true;
		}
		pMemmp->m_aBRaw[PPUREG_Status] |= 0x80;
	}
	else if ((s_tickpStatusClear > tickpSubframePrev) & (s_tickpStatusClear <= tickpSubframe))
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

void StaticInitPpu(Ppu * pPpu, Platform * pPlat)
{
	pPpu->m_pTexScreen = PTexCreate(pPlat, 256, 256);
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
void FetchNametableTile(Ppu * pPpu, u16 addrBase, int xTile, int yTile, int ySubtile, TileLine * pTilOut)
{
	u8 * pBNameTable = PBVram(pPpu, addrBase);
	u16 iTile = pBNameTable[xTile + yTile * s_dXTileScreen];

	u16 addrPattern = (pPpu->m_pctrl.m_fHighBgPaternTable) ? 0x1000 : 0x0;
	u8 * pBPattern = PBVram(pPpu, addrPattern);

	int dB = (ySubtile & 0x7) | (iTile << 4);
	pTilOut->m_bLow = pBPattern[dB];
	pTilOut->m_bHigh = pBPattern[dB | 0x8];

	static const int s_dXAttributeCell = 8;
	static const int s_dBAttribute = s_dXTileScreen * s_dYTileScreen;
	u8 * pBAttribute = pBNameTable + s_dBAttribute;

	u8 bAttribute = pBAttribute[(xTile>>2) + ((yTile>>2) * s_dXAttributeCell)];
	int xCell = xTile >> 1;
	int yCell = yTile >> 1;
	int cBitShift = (xCell & 0x1) ? 2 : 0;
	cBitShift += (yCell & 0x1) ? 4 : 0;
	pTilOut->m_iPalette = (bAttribute >> cBitShift) & 0x3;
}

void RasterTileLineRgb(TileLine * pTil, bool fFlipHoriz, int ySubtile, RGBA * aRgbaPalette, u8 * pRgb, u8 * pBAlpha)
{
	u8 bLow = pTil->m_bLow;
	u8 bHigh = pTil->m_bHigh;
	u8 * pRgbIt = pRgb;
	int dRgbaPalette = pTil->m_iPalette * s_cRgbaPalette;
	for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
	{
		int iPalette; 

		if (fFlipHoriz)
		{
			iPalette = (bLow & 0x1) | ((bHigh & 0x1) << 1);
			bLow >>= 1;
			bHigh >>= 1;
		}
		else
		{
			int nShift = 7-xSubtile;
			iPalette = ((bLow >> nShift) & 0x1) | (((bHigh >> nShift) & 0x1) << 1);
		}

		RGBA rgba = aRgbaPalette[iPalette + dRgbaPalette];
		*pRgbIt++ = rgba.m_r;
		*pRgbIt++ = rgba.m_g;
		*pRgbIt++ = rgba.m_b;
		*pBAlpha++ = (iPalette != 0);
	}
}

void DrawNameTableMemory(Ppu * pPpu, Texture * pTex)
{
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

	static const int s_cPalette = 4;
	RGBA aRgba[s_cRgbaPalette * s_cPalette];
	for (int iRgb = 0; iRgb < FF_DIM(aRgba); ++iRgb)
	{
		// the first index in each palette is the shared background color
		int iHwcol = ((iRgb % s_cRgbaPalette) == 0) ? 0 : iRgb;
		aRgba[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol]));
	}

	u8 aBAlpha[8];
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

				TileLine til;
				FetchNametableTile(pPpu, 0x2000, xTile, yTile, ySubtile, &til);
				RasterTileLineRgb(&til, false, ySubtile, aRgba, pTex->m_pB + dB, aBAlpha);

				FetchNametableTile(pPpu, 0x2400, xTile, yTile, ySubtile, &til);
				RasterTileLineRgb(&til, false, ySubtile, aRgba, pTex->m_pB + dB + dBNametableX, aBAlpha);

				FetchNametableTile(pPpu, 0x2800, xTile, yTile, ySubtile, &til);
				RasterTileLineRgb(&til, false, ySubtile, aRgba, pTex->m_pB + dB + dBNametableY, aBAlpha);

				FetchNametableTile(pPpu, 0x2C00, xTile, yTile, ySubtile, &til);
				RasterTileLineRgb(&til, false, ySubtile, aRgba, pTex->m_pB + dB + dBNametableY + dBNametableX, aBAlpha);
			}
		}
	}
}

void DrawScreenSimple(Ppu * pPpu, u64 tickpMin, u64 tickpMax)
{
	// Don't worry too much about being cycle accurate
	TileLine * aTilLine = pPpu->m_aTilLine;
	TileLine * aTilBackground = pPpu->m_aTilBackground;

	int cScanlineMin, cScanlineMax;
	int tickpScanlineMin, tickpScanlineMax;
	SplitTickpScanline(tickpMin, &cScanlineMin, &tickpScanlineMin);
	SplitTickpScanline(tickpMax, &cScanlineMax, &tickpScanlineMax);

	static const int cScanlineVisible = 240;
	static const int dTickpTileFetch = 8;
	static const int s_tickpScanineMax = 341;

	int cScanlineSprite = (pPpu->m_pctrl.m_fUse8x16Sprite) ? 16 : 8;
	int tickpScanline = (tickpScanlineMin + 7) & ~7; //round up to the next 8 tickp boundary

	u8 * pBPatternLow = PBVram(pPpu, 0x0);
	u8 * pBPatternHigh = PBVram(pPpu, 0x1000);
	u8 * pBPatternSprite = (pPpu->m_pctrl.m_fHighSpritePatternTable) ? pBPatternHigh : pBPatternLow;

	static const int s_cPalette = 4;
	u8 aBBackground[8 * 3];
	u8 aBSprite[8 * 3];
	int iXSprite = 8;
	RGBA aRgbaBgPalette[s_cRgbaPalette * s_cPalette];
	RGBA aRgbaSpritePalette[s_cRgbaPalette * s_cPalette];
	for (int iRgb = 0; iRgb < FF_DIM(aRgbaBgPalette); ++iRgb)
	{
		// the first index in each palette is the shared background color
		int iHwcol = ((iRgb % s_cRgbaPalette) == 0) ? 0 : iRgb;
		aRgbaBgPalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol]));

		aRgbaSpritePalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol + s_cPalette*s_cRgbaPalette]));
	}

	auto pTexScreen = pPpu->m_pTexScreen;

	u8 aBAlpha[8];
	u8 aBAlphaBg[8];

	for (int iScanline = cScanlineMin; iScanline <= cScanlineMax; ++iScanline)
	{
		int tickpLineMax = (iScanline == cScanlineMax) ? tickpScanlineMax : s_tickpScanineMax;
		if (iScanline < 240)
		{
			//visible scanlines
			for ( ; tickpScanline <= tickpLineMax; tickpScanline += 8)
			{	
				if (tickpScanline < 258)
				{
					// Fetch background tile
					int yTile = iScanline / 8;
					int ySubtile = iScanline % 8;
					int xTile = tickpScanline / 8;
					if (xTile + 2 < FF_DIM(pPpu->m_aTilBackground))
					{
						FetchNametableTile(pPpu, 0x2000, xTile+2, yTile, ySubtile, &aTilBackground[xTile + 2]);
					}

					RasterTileLineRgb(&aTilBackground[xTile], false, ySubtile, aRgbaBgPalette, aBBackground, aBAlphaBg);

					u8 * pBScreen = &pTexScreen->m_pB[((xTile*8) + iScanline * pTexScreen->m_dX) * 3];
					for (int xSubtile=0; xSubtile < 8; ++xSubtile)
					{
						if (iXSprite >= 8)
						{
							int x = xTile * 8 + xSubtile;
							OAM * pOamMax = FF_PMAX(pPpu->m_aOamLine);
							int iOam = 0;
							for (OAM * pOam = pPpu->m_aOamLine; pOam != pOamMax; ++pOam, ++iOam)
							{
								int xSubsprite = x - pOam->m_xLeft;
								if ((xSubsprite >= 0) && (xSubsprite < 8))
								{
									iXSprite = xSubsprite;
									int ySubtileSprite = iScanline - pOam->m_yTop;
									RasterTileLineRgb(&aTilLine[iOam], pOam->m_fFlipHoriz, ySubtileSprite, aRgbaSpritePalette, aBSprite, aBAlpha);
									break;
								}
							}
						}
						
						if (iXSprite < 8 && aBAlpha[iXSprite])
						{
							u8 * pBSprite = &aBSprite[iXSprite * 3];
							*pBScreen++ = *pBSprite++;
							*pBScreen++ = *pBSprite++;
							*pBScreen++ = *pBSprite++;
						}
						else 
						{
							u8 * pBBackground = &aBBackground[xSubtile * 3];
							*pBScreen++ = *pBBackground++;
							*pBScreen++ = *pBBackground++;
							*pBScreen++ = *pBBackground++;
						}
						++iXSprite;
					}
				}
				else if (tickpScanline == 264) // should start at 258, but we'll adjust
				{
					// compute m_oamLine and fetch their tiles
					int cOamLine = 0;
					OAM * pOamMax = FF_PMAX(pPpu->m_aOam);
					for (OAM * pOam = pPpu->m_aOam; pOam != pOamMax; ++pOam)
					{
						int ySubtile = (iScanline - pOam->m_yTop);
						if  (ySubtile >= 0 && ySubtile < cScanlineSprite)
						{
							auto pTilSprite = &aTilLine[cOamLine];

							// handle 8x16 tiles
							int iTile = pOam->m_iTile;
							u8 * pBPattern = pBPatternSprite;

							if (cScanlineSprite == 16)
							{
								pBPattern = (iTile & 0x1) ? pBPatternHigh : pBPatternLow;
								iTile = (iTile & 0xFE) + ((ySubtile >= 8) ? 1 : 0);
							}
							int dB = (ySubtile & 0x7) | (iTile << 4);
							pTilSprite->m_bLow = pBPattern[dB];
							pTilSprite->m_bHigh = pBPattern[dB | 0x8];
							pTilSprite->m_iPalette = pOam->m_palette;

							pPpu->m_aOamLine[cOamLine++] = *pOam;
							if (cOamLine >= 8)
								break;
						}
					}
					
					while (cOamLine < FF_DIM(pPpu->m_aOamLine))
					{
						aTilLine[cOamLine] = TileLine{0, 0, 0};
						OAM * pOam = &pPpu->m_aOamLine[cOamLine++];
						pOam->m_yTop = 0xFF;
						pOam->m_xLeft = 0;
						pOam->m_iTile = 0xFF;
					}
				}
				// BB - these are actually one tickp early because we're operating eight cycles at a time
				else if (tickpScanline == 320 || tickpScanline == 328)
				{
					// fetch two background tiles for the next scanline
					// BB - what's the base address here ACTUALLY supposed to be?
					int iScanlineAdj = iScanline + 1;
					int yTile = iScanlineAdj / 8;
					int ySubtile = iScanlineAdj % 8;
					FetchNametableTile(pPpu, 0x2000, 0, yTile, ySubtile, &aTilBackground[0]);
					FetchNametableTile(pPpu, 0x2000, 1, yTile, ySubtile, &aTilBackground[1]);
				}
			}
			tickpScanline = 0;
		}
		else if (iScanline = 261)
		{
			//pre render line
			FetchNametableTile(pPpu, 0x2000, 0, 0, 0, &aTilBackground[0]);
			FetchNametableTile(pPpu, 0x2000, 1, 0, 0, &aTilBackground[1]);
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

