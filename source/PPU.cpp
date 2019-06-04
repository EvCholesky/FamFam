#include "Platform.h"
#include "PPU.h"

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

void DrawChrMemory(Ppu * pPpu, Texture * pTex, bool fUse8x16)
{
	FF_ASSERT(pTex->m_dX == s_dXChrHalf && pTex->m_dY == s_dYChrHalf, "DrawChrMemory is not very flexible yet.");	

	int yStride = 64; 
	auto pBOut = pTex->m_pB;
	u8 * pBChr = pPpu->m_aBChr;

	int cSubtile = 8;
	int dYTile = 8;

	for (int yTile = 0; yTile < s_cYChrTile; ++yTile)
	{
		for (int xTile = 0; xTile < s_cXChrTile; ++xTile)
		{
			for (int ySubtile = 0; ySubtile < cSubtile; ++ySubtile)
			{
				u8 bPlane0 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile ];
				u8 bPlane1 = pBChr[(xTile << 4) | (yTile << 8) | ySubtile | 0x08];

				int yLine;
				int iPixel;
				if (fUse8x16)
				{
					yLine = ((yTile & 0xFE) + (xTile & 0x1)) * dYTile + ySubtile;
					iPixel = ((xTile >> 1) + ((yTile & 0x1) * 8))*8 + (yLine*s_dYChrHalf);
				}
				else
				{
					yLine = yTile*dYTile + ySubtile;
					iPixel = xTile*8 + (yLine*s_dYChrHalf);
				}

				for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
				{
					u16 i = (bPlane0 >> (7-xSubtile)) & 0x1;
					i |= ((bPlane1 >> (7-xSubtile)) & 0x1) << 1;

					auto pBTest = &pBOut[(iPixel + xSubtile) * 3];
					pBTest[0] = 32 + i * 64; 
					pBTest[1] = 32 + i * 64; 
					pBTest[2] = 32 + i * 64; 
				}
			}
		}
	}
#if 0
	static const int s_cBTileLine = 16;
	static const int s_cBTile = s_cBTileLine * 8;

	for (int yTile = 0; yTile < 8; ++yTile)
	{
		for (int xTile = 0; xTile < 8; ++xTile)
		{
			//int iTile = xTile + yTile * 8;
			//RGBA rgba = RgbaFromHwcol(HWCOL(iTile % HWCOL_Max));
			for (int ySubtile = 0; ySubtile < 8; ++ySubtile)
			{
				auto pBTile = &pBChr[xTile * s_cBTile + (yTile*8 + ySubtile) * s_cBTileLine];
				u16 nTile = *(u16*)pBTile;

				int yLine = yTile*8 + ySubtile;
				int iPixel = xTile*8 + (yLine*64);
				//iPixel *= 3;
				for (int xSubtile = 0; xSubtile < 8; ++xSubtile)
				{
					SetRgbFromTile(nTile, xSubtile, &pBOut[(iPixel + xSubtile) * 3]);

					/*
					auto pBTest = &pBOut[(iPixel + xSubtile) * 3];
					FF_ASSERT(pBTest - pBOut < 64*64*3, "ack");
					pBTest[0] = rgba.m_r;
					pBTest[1] = rgba.m_g;
					pBTest[2] = rgba.m_b;
					*/
				}
			}
		}
	}
#endif
}



void DrawNametables()
{
	// 30x32 table where each byte corresponds with one 8-pixel x 8-pixel region

}
