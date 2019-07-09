#include "Cpu.h"
#include "Platform.h"
#include "PPU.h"
#include "stdio.h"

static const int s_cRgbaPalette = 4;
extern bool s_fTraceCpu;

Ppu::Ppu()
:m_pctrl{0}
,m_pmask{0}
,m_pstatus{0}
,m_iOamSpriteZero(-1)
,m_addrV(0)
,m_addrTemp(0)
,m_dXScrollFine(0)
,m_bOamAddr(0)
,m_fIsFirstAddrWrite(true)
,m_pTexScreen(nullptr)
{
	ZeroAB(m_aBCieram, FF_DIM(m_aBCieram));
	ZeroAB(m_aBChr, FF_DIM(m_aBChr));
	ZeroAB(m_aBPalette, FF_DIM(m_aBPalette));
	ZeroAB(m_aPVramMap, FF_DIM(m_aPVramMap));

	ZeroAB(m_aOam, FF_DIM(m_aOam));
	ZeroAB(m_aOamLine, FF_DIM(m_aOamLine));
}

Ppu::~Ppu()
{
	ClearCommandHistory(this);
}

void ClearPpuCommandList(PpuCommandList * pPpucl)
{
	pPpucl->m_iPpucmdExecute = 0;
	pPpucl->m_aryPpucmd.Clear();
}

PpuCommandList::~PpuCommandList()
{
	ClearPpuCommandList(this);
}

#if FF_RECORD_PPU_HISTORY
void ClearCommandHistory(Ppu * pPpu)
{
	/*for (int ipPpuchf = 0; ipPpuchf < pPpu->m_arypPpuchf.C(); ++ipPpuchf)
	{
		auto pPpuchf = pPpu->m_arypPpuchf[ipPpuchf];
		if (pPpuchf)
		{
			delete pPpuchf;
		}
	}*/
	pPpu->m_aryPpuchf.Clear();
}

void RecordCommandHistory(Ppu * pPpu, u64 tickp, PpuCommand * ppucmd)
{
	s64 cFrame, cSubframe;
	int cScanline, iCol;
	SplitTickpFrame(tickp, &cFrame, &cSubframe);
	SplitTickpScanline(tickp, &cScanline, &iCol);

	auto paryPpuchf = &pPpu->m_aryPpuchf;
	if (paryPpuchf->FIsEmpty() || cFrame != (*paryPpuchf)[0].m_cFrame)
	{
		// insert a new frame
		if (paryPpuchf->C() == paryPpuchf->CMax())
		{
			(void)paryPpuchf->PLast()->m_aryPpuch.Clear();
		}
		else
		{
			(void)paryPpuchf->AppendNew();
		}

		for (size_t iPpuchf = 1; iPpuchf < paryPpuchf->C(); ++iPpuchf)
		{
			(*paryPpuchf)[iPpuchf] = (*paryPpuchf)[iPpuchf - 1];
		}

		(*paryPpuchf)[0].m_cFrame = cFrame;
		(*paryPpuchf)[0].m_addrTempBegin = pPpu->m_addrTemp;
		(*paryPpuchf)[0].m_addrVBegin = pPpu->m_addrV;
		(*paryPpuchf)[0].m_aryPpuch.Clear();
	}

	auto pPpuch = (*paryPpuchf)[0].m_aryPpuch.AppendNew();
	pPpuch->m_cScanline = cScanline;
	pPpuch->m_iCol = iCol;
	pPpuch->m_addrTemp = pPpu->m_addrTemp;
	pPpuch->m_addrV = pPpu->m_addrV;
	pPpuch->m_ppucmd = *ppucmd;
}
#endif

void AppendPpuCommand(PpuCommandList * pPpucl, PCMDK pcmdk, u16 addr, u8 bValue, const PpuTiming & ptim, s8 iPpucres, u16 addrDebug)
{
	auto pPpucmd = pPpucl->m_aryPpucmd.AppendNew();
	pPpucmd->m_addr = addr;
	pPpucmd->m_pcmdk = pcmdk;
	pPpucmd->m_bValue = bValue;
	pPpucmd->m_tickp = ptim.m_tickp;
	pPpucmd->m_addrInstDebug = addrDebug;
	pPpucmd->m_iPpucres = iPpucres;
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
		PpuCommand * pPpucmd;
		s64 tickpNext; 
		if (iPpucmd >= pPpucl->m_aryPpucmd.C())
		{
			pPpucmd = nullptr;
			tickpNext = ptimEnd.m_tickp;
		}
		else
		{
			pPpucmd = &pPpucl->m_aryPpucmd[iPpucmd];
			tickpNext = pPpucmd->m_tickp;
			++iPpucmd;
		}

		// update ppu until the new time
		DrawScreenNew(pPpu, pFam->m_tickp, tickpNext);

		if (pPpucmd)
		{
			if (pPpucmd->m_pcmdk == PCMDK_Read)
			{
				switch (pPpucmd->m_addr)
				{
					case PPUREG_Status:
					{
						// reading from PPUSTATUS $2002 clears the address latch used by PPUSCROLL and PPUADDR
						pPpu->m_fIsFirstAddrWrite = true;

						if (pPpucmd->m_iPpucres >= 0)
						{
							pPpucl->m_aPpucres[pPpucmd->m_iPpucres].m_b = pPpu->m_pstatus.m_nBits;
						}
						pPpu->m_pstatus.m_fVerticalBlankStarted = false;
					} break;
					case PPUREG_PpuData:
					{
						FF_ASSERT(false, "TBD");
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
					{
						u16 addrTPrev = pPpu->m_addrTemp;
						pPpu->m_addrTemp = (pPpu->m_addrTemp & ~0x0C00) | u16(pPpucmd->m_bValue & 0x3) << 10; 
						pPpu->m_pctrl.m_nBits = pPpucmd->m_bValue;
					} break;
				case PPUREG_Mask:
					pPpu->m_pmask.m_nBits = pPpucmd->m_bValue;
					break;
				case PPUREG_Status:
					pPpu->m_pstatus.m_nBits = pPpucmd->m_bValue;
					break;

				case PPUREG_PpuScroll:
					{
						u16 addrTPrev = pPpu->m_addrTemp;
						if (pPpu->m_fIsFirstAddrWrite)
						{
							u16 addrT = (pPpu->m_addrTemp & ~0x1F) | u16((pPpucmd->m_bValue >> 0x3) & 0x1F);
							pPpu->m_addrTemp = addrT;
							pPpu->m_dXScrollFine = pPpucmd->m_bValue & 0x7;
						}
						else
						{
							u16 addrT = pPpu->m_addrTemp & 0x8C1F;
							addrT |= u16(pPpucmd->m_bValue & 0x7) << 12;
							addrT |= u16(pPpucmd->m_bValue & 0xF8) << 2;
							pPpu->m_addrTemp = addrT;

							 // doesn't set addr s here, t will be copied into s at dot 257 of each scanline if rendering is enabled
						}

						pPpu->m_fIsFirstAddrWrite = !pPpu->m_fIsFirstAddrWrite;
					} break;
				case PPUREG_PpuAddr:
					{
						u16 addrTPrev = pPpu->m_addrTemp;
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

			RecordCommandHistory(&pFam->m_ppu, tickpNext, pPpucmd);
		}

		pFam->m_tickp = tickpNext;
	}

	pPpucl->m_aryPpucmd.RemoveSpan(0, iPpucmd);
	pPpucl->m_iPpucmdExecute = 0;
}

void AdvancePpuTiming(Famicom * pFam, s64 tickc, MemoryMap * pMemmp)
{
	Ppu * pPpu = &pFam->m_ppu;
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
	if ((fIsRenderEnabled & fIsOddFrame) & (cFramePrev != cFrame))
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
		pPpu->m_pstatus.m_fVerticalBlankStarted = true;
	}
	else if ((s_tickpStatusClear > tickpSubframePrev) & (s_tickpStatusClear <= tickpSubframe))
	{
		pPpu->m_pstatus.m_nBits = 0;
		pMemmp->m_aBRaw[PPUREG_Status] = 0x0;
		pPpu->m_pstatus.m_fVerticalBlankStarted = false;
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

Texture * PTexCreateStub(Platform * pPlat, s16 dX, s16 dY)
{
	auto pTex = new Texture;
	pTex->m_dX = dX;
	pTex->m_dY = dY;
	pTex->m_pB = new u8[dX * dY * 4];
	pTex->m_nId = -1;
	return pTex;
}


void StaticInitPpu(Ppu * pPpu, Platform * pPlat)
{
	if (pPlat)
	{
		pPpu->m_pTexScreen = PTexCreate(pPlat, 256, 256);
	}
	else
	{
		// null pPlat for log tests
		pPpu->m_pTexScreen = PTexCreateStub(pPlat, 256, 256);
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

	pPpu->m_aPVramMap[0x3F10] = &pPpu->m_aBPalette[0x0];
	pPpu->m_aPVramMap[0x3F14] = &pPpu->m_aBPalette[0x4];
	pPpu->m_aPVramMap[0x3F18] = &pPpu->m_aBPalette[0x8];
	pPpu->m_aPVramMap[0x3F1C] = &pPpu->m_aBPalette[0xC];
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
	u8 bL = pBPattern[dB];
	u8 bH = pBPattern[dB | 0x8];
	pTilOut->m_bLow = bL;
	pTilOut->m_bHigh = bH;

	static const int s_dXAttributeCell = 8;
	static const int s_dBAttribute = s_dXTileScreen * s_dYTileScreen;
	u8 * pBAttribute = pBNameTable + s_dBAttribute;

	u8 bAttrib = pBAttribute[(xTile>>2) + ((yTile>>2) * s_dXAttributeCell)];
	
	int xCell = xTile >> 1;
	int yCell = yTile >> 1;
	int cBitShift = (xCell & 0x1) ? 2 : 0;
	cBitShift += (yCell & 0x1) ? 4 : 0;
	u8 nAttrib = (bAttrib >> cBitShift) & 0x3;
	pTilOut->m_iPalette = nAttrib;

	// Swizzle both 8 bit patterns into one 16bit pattern and add it to our bg shift register
	// bLow  = hgfedcba
	// bHigh = HGFEDCBA -> nShiftBg == HhGgFfEeDdCcBbAa

	// and shift in a 2bit attribute for each pixel too
	// attrib = Pp ->nShiftAttrib = PpPpPpPpPpPpPpPp

	u32 nShiftBg = 0;
	u32 nShiftAttrib = 0;
	for (int iCol = 0; iCol < 8; ++iCol)
	{
		u32 nSel = 0x1 << iCol; 
		nShiftBg	 |= ((u32(bL) & nSel) << iCol) | ((u32(bH) & nSel) << (iCol+1));
		nShiftAttrib |= u32(nAttrib) << (iCol * 2);
	}

	pPpu->m_nShiftBg = (pPpu->m_nShiftBg << 16) | nShiftBg;
	pPpu->m_nShiftBgAttrib = (pPpu->m_nShiftBgAttrib << 16) | nShiftAttrib;
}

static inline void FetchNametableTile(Ppu * pPpu, u16 addrV, TileLine * aTilBackground)
{
	int xTile = (addrV & 0x1F);
	int yTile = (addrV & 0x3E0) >> 5; 
	int ySubtile = (addrV & 0x7000) >> 12;

	// BB - fix nametable problems here!
	//u16 nNametable = 0;
	u16 nNametable = (addrV & 0xC00);// >> 10;

	FetchNametableTile(pPpu, 0x2000 | nNametable, xTile, yTile, ySubtile, &aTilBackground[xTile]);
}

// BB - I tried removing this and just changing all of the textures to 8888 rgba, it seemed to make things slower
// (which makes sense) I need to revisit when I have better timeing tools.
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

void RasterShiftRgb(u32 nShiftPattern, u32 nShiftAttrib, int ySubtile, int xFine, RGBA * aRgbaPalette, RGBA * pRgba)
{
	RGBA * pRgbaIt = pRgba;
	for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
	{
		int x = xFine + xSubtile;

		int nShift = 30-(2*x);
		int iRgba = (nShiftPattern >> nShift) & 0x3;
		int nAttrib = (nShiftAttrib >> nShift) & 0x3;

		int dRgbaPalette = nAttrib * s_cRgbaPalette;
		RGBA rgba = aRgbaPalette[iRgba + dRgbaPalette];
		*pRgbaIt = rgba;
		pRgbaIt->m_a = (iRgba != 0);
		++pRgbaIt;
	}
}

void RasterTileLineRgba(TileLine * pTil, bool fFlipHoriz, int ySubtile, RGBA * aRgbaPalette, RGBA * pRgba)
{
	u8 bLow = pTil->m_bLow;
	u8 bHigh = pTil->m_bHigh;
	RGBA * pRgbaIt = pRgba;
	int dRgbaPalette = pTil->m_iPalette * s_cRgbaPalette;
	static const u8 s_aBAlpha[] = {0, 255, 255, 255};

	if (fFlipHoriz)
	{
		int iPalette; 
		RGBA * pRgbaMax = &pRgba[8];
		while (pRgbaIt != pRgbaMax)
		{
			iPalette = (bLow & 0x1) | ((bHigh & 0x1) << 1);
			bLow >>= 1;
			bHigh >>= 1;

			RGBA rgba = aRgbaPalette[iPalette + dRgbaPalette];
			rgba.m_a = s_aBAlpha[iPalette];
			*pRgbaIt++ = rgba;
		}
	}
	else
	{
		for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
		{
			int nShift = 7-xSubtile;
			int iPalette = ((bLow >> nShift) & 0x1) | (((bHigh >> nShift) & 0x1) << 1);

			RGBA rgba = aRgbaPalette[iPalette + dRgbaPalette];
			rgba.m_a = s_aBAlpha[iPalette];
			*pRgbaIt++ = rgba;
		}
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

u16 AddrIncVramH(u16 addrV)
{
#if 0
	if ((addrV & 0x001F) <= 31) // if coarse X == 31
		return addrV + 1;
	return addrV ^ 0x41F;	// set coarse X == 0 and switch horiz nametable
#else

	//addrV &= ~0x001F;         // coarse X = 0
	//addrV ^= 0x0400;          // switch horizontal nametable
	//return addrV;
	
	if ((addrV & 0x001F) == 31) // if coarse X == 31
	{
		addrV &= ~0x001F;          // coarse X = 0
		addrV ^= 0x0400;          // switch horizontal nametable
	}
	else
	{
	  addrV += 1;                // increment coarse X}
	}
	return addrV;
#endif
}

u16 AddrIncVramV(u16 addrV)
{
#if 0
	if ((addrV & 0x7000) != 0x7000)        // if fine Y < 7
	{
	  return addrV + 0x1000;                // increment fine Y
	}

	// why do this when it's just stripped in the end?
	addrV &= ~0x7000;                     // fine Y = 0

	int yCoarse = (addrV & 0x03E0) >> 5;
	if (yCoarse == 29)
	{
		yCoarse = 0;                        // coarse Y = 0
		addrV ^= 0x0800;                    // switch vertical nametable
	}
	else// if (y == 31)
	{
		// Coarse Y can be set out of bounds and will wrap without adjusting the nametable
		yCoarse = (yCoarse + 1) & 0x1F;
	}
	return (addrV & ~0x03E0) | (yCoarse << 5);
#else

	if ((addrV & 0x7000) != 0x7000)        // if fine Y < 7
	{
		 addrV += 0x1000;                      // increment fine Y
	}
	else
	{
		addrV &= ~0x7000;                     // fine Y = 0
		int y = (addrV & 0x03E0) >> 5;        // let y = coarse Y
		if (y == 29)
		{
			y = 0;                          //0 coarse Y = 0
			addrV ^= 0x0800;                    // switch vertical nametable
		}
		else if (y == 31)
		{
			y = 0;                          // coarse Y = 0, nametable not switched
		}
		else
		{
			y += 1;                         // increment coarse Y
		}
		addrV = (addrV & ~0x03E0) | (y << 5);     // put coarse Y back into v
	}
	return addrV;
#endif
}

void DrawScreenSimple(Ppu * pPpu, u64 tickpMin, u64 tickpMax)
{
	{
		s64 cFrame = CFrameFromTickp(tickpMin);
		int cScanline, tickpScanline;
		SplitTickpScanline(tickpMin, &cScanline, &tickpScanline);
		printf("ds (%lld|%d:%d) ", cFrame, cScanline, tickpScanline);
	}

	TileLine * aTilLine = pPpu->m_aTilLine;
	TileLine * aTilBackground = pPpu->m_aTilBackground;

	static const int cScanlineVisible = 240;
	static const int dTickpTileFetch = 8;

	int cScanlineSprite = (pPpu->m_pctrl.m_fUse8x16Sprite) ? 16 : 8;
	u8 * pBPatternLow = PBVram(pPpu, 0x0);
	u8 * pBPatternHigh = PBVram(pPpu, 0x1000);
	u8 * pBPatternSprite = (pPpu->m_pctrl.m_fHighSpritePatternTable) ? pBPatternHigh : pBPatternLow;

	static const int s_cPalette = 4;
	RGBA aRgbaBackground[8];
	ZeroAB(aRgbaBackground, sizeof(aRgbaBackground));

	int iXSprite = 8;
	RGBA aRgbaBgPalette[s_cRgbaPalette * s_cPalette];
	RGBA aRgbaSpritePalette[s_cRgbaPalette * s_cPalette];
	for (int iRgb = 0; iRgb < FF_DIM(aRgbaBgPalette); ++iRgb)
	{
		// the first index in each palette is the shared background color
		int iHwcol = ((iRgb & 0x3) == 0) ? 0 : iRgb;
		aRgbaBgPalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol]));

		aRgbaSpritePalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol + s_cPalette*s_cRgbaPalette]));
	}

	bool fShowBg = pPpu->m_pmask.m_fShowBg;
	bool fShowSprites = pPpu->m_pmask.m_fShowSprites;

	auto cFrameMax = CFrameFromTickp(tickpMax);

	int cScanlineMin, cScanlineMax;
	int tickpScanlineMin, tickpScanlineMax;
	auto tickpCur = tickpMin;
	while (tickpCur < tickpMax)
	{
		SplitTickpScanline(tickpCur, &cScanlineMin, &tickpScanlineMin);
		auto cFrameCur = CFrameFromTickp(tickpCur);
		if (cFrameCur < cFrameMax)
		{
			cScanlineMax = s_cScanlinesPerFrame;
			tickpScanlineMax = s_dTickpPerScanline;
			tickpCur = (cFrameCur+1) * s_dTickpPerFrame;
		}
		else
		{
			SplitTickpScanline(tickpMax, &cScanlineMax, &tickpScanlineMax);
			tickpCur = tickpMax;
		}

		int tickpScanline = (tickpScanlineMin + 7) & ~7; //round up to the next 8 tickp boundary

		//printf("%d..%d,", cScanlineMin, cScanlineMax);
		auto pTexScreen = pPpu->m_pTexScreen;
		for (int iScanline = cScanlineMin; iScanline <= cScanlineMax; ++iScanline)
		{
			int tickpLineMax = (iScanline == cScanlineMax) ? tickpScanlineMax : s_dTickpPerScanline;
			if (iScanline < 240)
			{
				//visible scanlines
				for ( ; tickpScanline <= tickpLineMax; tickpScanline += 8)
				{	
					if (tickpScanline < 258)
					{
						// Fetch background tile
						if (fShowBg)
						{
							//int yTile = iScanline >> 3;		 // div 8
							//int ySubtile = iScanline & 0x7;	 // mod 8
							//int xTile = tickpScanline >> 3;	 // div 8
							u16 addrV = pPpu->m_addrV;
							int xTile = (addrV & 0x1F);
							int yTile = (addrV & 0x3E0) >> 5; 
							int ySubtile = (addrV & 0x7000) >> 12;
							u16 nNametable = 0;//(addrV & 0xC00) >> 10;
							//FF_ASSERT(ySubtile < 8 && xTile < 32 && yTile < 30, "bad offset");
							if (xTile + 2 < FF_DIM(pPpu->m_aTilBackground))
							{
								FetchNametableTile(pPpu, 0x2000 | nNametable, xTile+2, yTile, ySubtile, &aTilBackground[xTile+2]);
							}

							RasterTileLineRgba(&aTilBackground[xTile], false, ySubtile, aRgbaBgPalette, aRgbaBackground);
							auto addrVPrev = pPpu->m_addrV;
							pPpu->m_addrV = AddrIncVramH(addrV);

							if (iScanline == 0)
								printf("<%04X/%04X->%04X>,", pPpu->m_addrTemp, pPpu->m_addrV, addrV);
						}

						int xTileSprite = tickpScanline >> 3;	 // div 8
						u8 * pBScreen = &pTexScreen->m_pB[((xTileSprite*8) + iScanline * pTexScreen->m_dX) * 3];
						int x = xTileSprite * 8;
						for (int xSubtile=0; xSubtile < 8; ++xSubtile, ++x)
						{
							int iOam = 0;
							RGBA rgbaSprite{0};
							OAM * pOamMax = (fShowSprites) ? FF_PMAX(pPpu->m_aOamLine) : pPpu->m_aOamLine;

							OAM * pOamRaster = nullptr;
							for (OAM * pOam = pPpu->m_aOamLine; pOam != pOamMax; ++pOam, ++iOam)
							{
								int xSubsprite = x - pOam->m_xLeft;
								if ((xSubsprite >= 0) && (xSubsprite < 8))
								{
									RGBA rgba = pPpu->m_aRgbaSpriteLine[iOam * 8 + xSubsprite];

									if (rgba.m_a != 0)
									{
										rgbaSprite = rgba;
										//fSpritePriority = pOam->m_fPriority;
										pOamRaster = pOam;
										break;
									}
								}
							}

							RGBA rgbaBackground = aRgbaBackground[xSubtile];
							if (rgbaSprite.m_a != 0 && (!pOamRaster->m_fBgPriority || rgbaBackground.m_a == 0))
							{
								if (pOamRaster == &pPpu->m_aOamLine[pPpu->m_iOamSpriteZero])
								{
									pPpu->m_pstatus.m_fSpriteZeroHit = true;
								}
								*pBScreen++ = rgbaSprite.m_r;
								*pBScreen++ = rgbaSprite.m_g;
								*pBScreen++ = rgbaSprite.m_b;
							}
							else 
							{
								*pBScreen++ = rgbaBackground.m_r;
								*pBScreen++ = rgbaBackground.m_g;
								*pBScreen++ = rgbaBackground.m_b;
							}
						}
					}
					else if (tickpScanline == 264) // should start at 258, but we'll adjust
					{
						// should be tickpScanline 257
						if (fShowBg)
						{
							//auto addrVPrev = pPpu->m_addrV;
							pPpu->m_addrV = (pPpu->m_addrV & ~0x041F) | (pPpu->m_addrTemp & 0x041F);
							int yCoarse = (pPpu->m_addrV & 0x03E0) >> 5;
							FF_ASSERT(yCoarse < 30, "ack %d)%d:%d", cFrameCur, iScanline, tickpScanline);
						}

						// compute m_oamLine and fetch their tiles
						int cOamLine = 0;
						pPpu->m_iOamSpriteZero = -1;

						RGBA * pRgbaSpriteLine = pPpu->m_aRgbaSpriteLine;
						OAM * pOamMax = (fShowSprites) ? FF_PMAX(pPpu->m_aOam) : pPpu->m_aOam;
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

								RasterTileLineRgba(pTilSprite, pOam->m_fFlipHoriz, ySubtile, aRgbaSpritePalette, pRgbaSpriteLine);
								pRgbaSpriteLine += 8;

								if (pOam == &pPpu->m_aOam[0])
								{
									pPpu->m_iOamSpriteZero = cOamLine;
								}
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

							for (int iRgba = 0; iRgba < 8; ++iRgba)
							{
								*pRgbaSpriteLine++ = RGBA{0};
							}
						}
					}
					// BB - these are actually one tickp early because we're operating eight cycles at a time
					else if ((tickpScanline == 320 || tickpScanline == 328) & fShowBg)
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

				if (fShowBg && tickpScanline >= s_dTickpPerScanline)
				{
					int yFine = (pPpu->m_addrV >> 12) & 0x7;
					int yCoarse = (pPpu->m_addrV >> 5) & 0x1F;
					int iScanlineExpected = yFine + yCoarse * 8;
					FF_ASSERT(iScanlineExpected == iScanline, "scanline mismatch");
					printf("y(%d),", iScanline);
					pPpu->m_addrV = AddrIncVramV(pPpu->m_addrV);
				}

				tickpScanline = 0;
			}
			else if (iScanline < 260)
			{
				tickpScanline = 0;
			}
#if 0// - somehow we skip past the prerender scanline?? from 
			else if (iScanline == 261 && !fShowBg) // pre-render
			{
				printf("{261 - noshow}");
				// fSpriteZeroHit should be cleared on the first dot of the pre-render scanline
				// ... is cleared in AdvancePpuTiming

				static const int s_tickpAddrFlush = 304;
				if (tickpScanline <= s_tickpAddrFlush &&  s_tickpAddrFlush < tickpLineMax )
				{
					//pPpu->m_pstatus.m_fSpriteZeroHit = false;

					printf("NOTflush y addr t=$%04X  %lld)%d:%d\n",pPpu->m_addrTemp, cFrameCur, iScanline, tickpScanline);
				}
			}
#endif
			else if (iScanline == 261 && fShowBg) // pre-render
			{
				// fSpriteZeroHit should be cleared on the first dot of the pre-render scanline
				// ... is cleared in AdvancePpuTiming
				printf("{261  %d}", tickpScanline);

//#why didn't this flush happen? didn't get to this scanline? fShowBg disabled?
				static const int s_tickpAddrFlush = 304;
				if (tickpScanline <= s_tickpAddrFlush &&  s_tickpAddrFlush < tickpLineMax )
				{
					//pPpu->m_pstatus.m_fSpriteZeroHit = false;

					printf("flush y addr t=$%04X  %lld)%d:%d\n",pPpu->m_addrTemp, cFrameCur, iScanline, tickpScanline);
					auto addrVPrev = pPpu->m_addrV;
					pPpu->m_addrV = (pPpu->m_addrV & ~0x73E0) | (pPpu->m_addrTemp & 0x73E0);
					int yCoarse = (pPpu->m_addrV & 0x03E0) >> 5;
					if (yCoarse >= 30)
					{
						printf("");
					}
					tickpScanline = s_tickpAddrFlush;
				}

				static const int s_tickpPreFetch0 = 321;
				static const int s_tickpPreFetch1 = 328;
				if (tickpScanline <= s_tickpPreFetch0 &&  s_tickpPreFetch0 < tickpLineMax )
				{
					printf("nt0,");
					FetchNametableTile(pPpu, 0x2000, 0, 0, 0, &aTilBackground[0]);
					tickpScanline = s_tickpPreFetch0;
				}

				if (tickpScanline <= s_tickpPreFetch1 &&  s_tickpPreFetch1 < tickpLineMax )
				{
					printf("nt1,");
					FetchNametableTile(pPpu, 0x2000, 1, 0, 0, &aTilBackground[1]);
					tickpScanline = 0;
				}
				printf("\n");
			}
		}
	}
}

inline static bool FIsInSpan(u64 i, u64 iMin, u64 iMax)
{
	return (i >= iMin) && (i < iMax);
}

void DrawScreenNew(Ppu * pPpu, u64 tickpMin, u64 tickpMax)
{
	static const int s_iColAddrFlush = 304;
	static const int s_iColPreFetch0 = 321;
	static const int s_iColPreFetch1 = 328;
	static const int s_iColAddrIncV = 256;
	static const int s_iColAddrResetH = 257;
	static const int s_iColScanlineEnd = 341;

	RGBA aRgbaBackground[8];
	ZeroAB(aRgbaBackground, sizeof(aRgbaBackground));

	int cScanlineSprite = (pPpu->m_pctrl.m_fUse8x16Sprite) ? 16 : 8;

	static const int s_cPalette = 4;
	RGBA aRgbaBgPalette[s_cRgbaPalette * s_cPalette];
	RGBA aRgbaSpritePalette[s_cRgbaPalette * s_cPalette];
	for (int iRgb = 0; iRgb < FF_DIM(aRgbaBgPalette); ++iRgb)
	{
		// the first index in each palette is the shared background color
		int iHwcol = ((iRgb & 0x3) == 0) ? 0 : iRgb;
		aRgbaBgPalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol]));

		aRgbaSpritePalette[iRgb] = RgbaFromHwcol(HWCOL(pPpu->m_aBPalette[iHwcol + s_cPalette*s_cRgbaPalette]));
	}

	bool fShowBg = pPpu->m_pmask.m_fShowBg;
	bool fShowSprites = pPpu->m_pmask.m_fShowSprites;

	auto pTexScreen = pPpu->m_pTexScreen;
	TileLine * aTilBackground = pPpu->m_aTilBackground;
	TileLine * aTilLine = pPpu->m_aTilLine;

	u64 tickpIt = tickpMin;
	while (tickpIt < tickpMax)
	{
		s64 cFrame = CFrameFromTickp(tickpIt);
		u64 tickpNextFrame = (cFrame + 1) * s_dTickpPerFrame;
		u64 tickpFrameMax = ffMin(tickpMax, tickpNextFrame);
		while (tickpIt < tickpFrameMax)
		{
			int cScanline, iCol;
			SplitTickpScanline(tickpIt, &cScanline, &iCol);

			if (cScanline < 240) // visible scanlines
			{
				u64 tickpVisibleScanlineMax = (cFrame * s_dTickpPerFrame) + 240 * s_dTickpPerScanline;
				tickpVisibleScanlineMax = ffMin(tickpVisibleScanlineMax, tickpFrameMax);
				while (tickpIt < tickpVisibleScanlineMax)
				{	
					u64 tickpBase  = (cFrame * s_dTickpPerFrame) + cScanline * s_dTickpPerScanline;

					if (tickpIt < tickpBase+1) // first col is idle
					{
						++tickpIt;
						++iCol;
					}

					int iColSprite = iCol;
					u64 tickpRasterMax = ffMin(tickpFrameMax, tickpBase + 257);
					while (tickpIt < tickpRasterMax)
					{
						// we're drawing eight columns at a time on the last column of the tile, round down to the start col
						//  also the 8 col blocks are offset by the one idle column
						iColSprite = ((iColSprite - 1) & ~7) + 1;

						u64 tickpCol = tickpIt - tickpBase;
						FF_ASSERT(tickpCol >= 0, "negative column");

						u64 tickpRaster = tickpBase + ((tickpCol + 7) & ~7); //round up to the next 8 tickp boundary
						if (tickpRaster < tickpRasterMax)
						{
							if (fShowBg)
							{

								int ySubtile = (pPpu->m_addrV & 0x7000) >> 12;
								RasterShiftRgb(pPpu->m_nShiftBg, pPpu->m_nShiftBgAttrib, ySubtile, pPpu->m_dXScrollFine, aRgbaBgPalette, aRgbaBackground);

								int xTile = (pPpu->m_addrV & 0x1F);
								if (xTile < FF_DIM(pPpu->m_aTilBackground)) // BB - get rid of this check??
								{
									FetchNametableTile(pPpu, pPpu->m_addrV, aTilBackground);
								}

								auto addrVPrev = pPpu->m_addrV;
								pPpu->m_addrV = AddrIncVramH(pPpu->m_addrV);
							}

							u8 * pBScreen = &pTexScreen->m_pB[(iColSprite + cScanline * pTexScreen->m_dX) * 3];

							for (int xSubtile=0; xSubtile < 8; ++xSubtile, ++iColSprite)
							{
								int iOam = 0;
								RGBA rgbaSprite{0};
								OAM * pOamMax = (fShowSprites) ? FF_PMAX(pPpu->m_aOamLine) : pPpu->m_aOamLine;

								OAM * pOamRaster = nullptr;
								for (OAM * pOam = pPpu->m_aOamLine; pOam != pOamMax; ++pOam, ++iOam)
								{
									int xSubsprite = iColSprite - pOam->m_xLeft;
									if ((xSubsprite >= 0) && (xSubsprite < 8))
									{
										RGBA rgba = pPpu->m_aRgbaSpriteLine[iOam * 8 + xSubsprite];
										if (rgba.m_a != 0)
										{
											rgbaSprite = rgba;
											pOamRaster = pOam;
											break;
										}
									}
								}

								RGBA rgbaBackground = aRgbaBackground[xSubtile];
								FF_ASSERT((pBScreen - pTexScreen->m_pB) < (pTexScreen->m_dX * pTexScreen->m_dY * 3), "bad screen write");
								if (rgbaSprite.m_a != 0 && (!pOamRaster->m_fBgPriority || rgbaBackground.m_a == 0))
								{
									if (pOamRaster == &pPpu->m_aOamLine[pPpu->m_iOamSpriteZero])
									{
										pPpu->m_pstatus.m_fSpriteZeroHit = true;
									}
									*pBScreen++ = rgbaSprite.m_r;
									*pBScreen++ = rgbaSprite.m_g;
									*pBScreen++ = rgbaSprite.m_b;
								}
								else 
								{
									*pBScreen++ = rgbaBackground.m_r;
									*pBScreen++ = rgbaBackground.m_g;
									*pBScreen++ = rgbaBackground.m_b;
								}

							}

							tickpIt = ffMin(tickpRaster + 1, tickpRasterMax);

							int cScanlineCheck, iColCheck;
							SplitTickpScanline(tickpIt, &cScanlineCheck, &iColCheck);
							FF_ASSERT(cScanlineCheck == cScanline, "mismatch scanline");
							FF_ASSERT(iColCheck == iColSprite, "mismatch iCol");
						}
						else
						{
							tickpIt = tickpRasterMax;
						}
					}

					/* We should be incrementing V here, but col 256 is inside the standard +8 raster  loop
					if (FIsInSpan(tickpBase + s_iColAddrIncV, tickpIt, tickpVisibleScanlineMax))
					{
						pPpu->m_addrV = AddrIncVramV(pPpu->m_addrV);
						++tickpIt;
					}
					*/
					if (FIsInSpan(tickpBase + s_iColAddrResetH, tickpIt, tickpVisibleScanlineMax))
					{
						static int s_iScanlineBreak = 100;
 						if (cScanline == s_iScanlineBreak)
						{
							printf("");
						}

						if (fShowBg)
						{
							pPpu->m_addrV = AddrIncVramV(pPpu->m_addrV);
							pPpu->m_addrV = (pPpu->m_addrV & ~0x041F) | (pPpu->m_addrTemp & 0x041F);
						}

						u8 * pBPatternLow = PBVram(pPpu, 0x0);
						u8 * pBPatternHigh = PBVram(pPpu, 0x1000);
						u8 * pBPatternSprite = (pPpu->m_pctrl.m_fHighSpritePatternTable) ? pBPatternHigh : pBPatternLow;

						// compute m_oamLine and fetch their tiles
						int cOamLine = 0;
						pPpu->m_iOamSpriteZero = -1;

						RGBA * pRgbaSpriteLine = pPpu->m_aRgbaSpriteLine;
						OAM * pOamMax = (fShowSprites) ? FF_PMAX(pPpu->m_aOam) : pPpu->m_aOam;
						for (OAM * pOam = pPpu->m_aOam; pOam != pOamMax; ++pOam)
						{
							int ySubtile = (cScanline - pOam->m_yTop);
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

								RasterTileLineRgba(pTilSprite, pOam->m_fFlipHoriz, ySubtile, aRgbaSpritePalette, pRgbaSpriteLine);
								pRgbaSpriteLine += 8;

								if (pOam == &pPpu->m_aOam[0])
								{
									pPpu->m_iOamSpriteZero = cOamLine;
								}
								pPpu->m_aOamLine[cOamLine++] = *pOam;

								if (cOamLine >= 8)
									break;
							}
						}
						
						// zero out unused OAM line slots
						ZeroAB(pRgbaSpriteLine, (u8*)FF_PMAX(pPpu->m_aRgbaSpriteLine) - (u8*)pRgbaSpriteLine);
						while (cOamLine < FF_DIM(pPpu->m_aOamLine))
						{
							aTilLine[cOamLine] = TileLine{0, 0, 0};
							OAM * pOam = &pPpu->m_aOamLine[cOamLine++];
							pOam->m_yTop = 0xFF;
							pOam->m_xLeft = 0;
							pOam->m_iTile = 0xFF;
						}
						++tickpIt;
					}

					if (fShowBg && FIsInSpan(tickpBase + s_iColPreFetch0, tickpIt, tickpVisibleScanlineMax))
					{
						FetchNametableTile(pPpu, pPpu->m_addrV, aTilBackground);
						pPpu->m_addrV = AddrIncVramH(pPpu->m_addrV);
					}

					if (fShowBg && FIsInSpan(tickpBase + s_iColPreFetch0, tickpIt, tickpVisibleScanlineMax))
					{
						FetchNametableTile(pPpu, pPpu->m_addrV, aTilBackground);
						pPpu->m_addrV = AddrIncVramH(pPpu->m_addrV);
					}

					// scanline end 
					if (FIsInSpan(tickpBase + s_iColScanlineEnd, tickpIt, tickpVisibleScanlineMax))
					{
						++cScanline;
						iCol = 0;

						u64 tickpScanlineMax = tickpBase + 341;
						tickpIt = ffMin(tickpVisibleScanlineMax, tickpScanlineMax);
					}
					else
					{
						tickpIt = tickpVisibleScanlineMax;
					}
				}

			} 
			else if (cScanline < 261) // Ppu idle scanlines
			{
				u64 tickpIdleEnd = (cFrame * s_dTickpPerFrame) + 261 * s_dTickpPerScanline;
				tickpIt = ffMin(tickpIdleEnd, tickpFrameMax);
			}
			else // pre-render scanline #261
			{
				u64 tickpBase = (cFrame * s_dTickpPerFrame) + cScanline * s_dTickpPerScanline;
				u64 tickpPrerenderEnd = tickpFrameMax;
				if (fShowBg && FIsInSpan(tickpBase + s_iColAddrFlush, tickpIt, tickpPrerenderEnd))
				{
					pPpu->m_addrV = (pPpu->m_addrV & ~0x7BE0) | (pPpu->m_addrTemp & 0x7BE0);
				}

				if (fShowBg && FIsInSpan(tickpBase + s_iColPreFetch0, tickpIt, tickpPrerenderEnd))
				{
					FetchNametableTile(pPpu, pPpu->m_addrV, aTilBackground);
					pPpu->m_addrV = AddrIncVramH(pPpu->m_addrV);
				}

				if (fShowBg && FIsInSpan(tickpBase + s_iColPreFetch1, tickpIt, tickpPrerenderEnd))
				{
					FetchNametableTile(pPpu, pPpu->m_addrV, aTilBackground);
					pPpu->m_addrV = AddrIncVramH(pPpu->m_addrV);
				}
				tickpIt = tickpPrerenderEnd;
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

