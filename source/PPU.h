#pragma once

static const int kCBVramPhysical = 16 * 1024;

// eight PPU regisers ($2000..$2007) are mirrored every eight bytes through $3FFF

enum PPUREG : u16
{
	PPUREG_Ctrl			= 0x2000,
	PPUREG_Mask			= 0x2001,
	PPUREG_Status		= 0x2002,
	PPUREG_OAMAddr 		= 0x2003,	// Object attribute memory, read/write addr
	PPUREG_OAMData 		= 0x2004,	// OAM data read/write
	PPUREG_PpuScroll	= 0x2005,	// fine scroll position
	PPUREG_PpuAddr		= 0x2006,	// PPU read/write address
	PPUREG_PpuData		= 0x2007,	// PPU data read/write

	PPUREG_OAMDMA		= 0x4014, 	// OAM DMA high address
};

union PpuControl
{
	u8	m_nBits;
	struct
	{
		u8	m_iBaseNametableAddr:2;		//(0==$2000, 1==$2400, 2==$2800, 3==$2C00)
		u8	m_dBVramAddressIncrement:1;	// (0==horizontal:add 1, 1==vertical: add 32)
		u8	m_addrSpritePatternTable:1;	// (0==$0000, 1==$1000; ignored in 8x16 mode)
		u8	m_addrBgPaternTable:1;		// (0==$0000, 1==$1000)
		u8	m_sizeSptrite:1;			// (0==8x8; 1==3x16)
		u8	m_fMasterSlaveSelect:1;		// (0==read backdrop from ext pins; 1==output color on ext pins)
		u8	m_fGenerateVBlankNMI:1;		// generate an NMI at the start of the vertical blanking interval
	};
};

union PpuMask
{
	u8	m_nBits;
	struct
	{
		u8	m_fUseGreyscale:1;
		u8	m_fShowBgLeftEdge:1;		// show background in leftmost 8 pixels
		u8	m_fShowSpriteLeftEdge:1;	// show sprites in leftmost 8 pixels
		u8	m_fShowBg:1;
		u8	m_fShowSprites:1;
		u8	m_fEmphasizeRed:1;
		u8	m_fEmphasizeGreen:1;
		u8	m_fEmphasizeBlue:1;
	};
};

union PpuStatus
{
	u8	m_nBits;
	struct 
	{
		u8	m_bitsPrevWritten:5;		// least significant bits prev written into a PPU register (due to the register not being updated for this address)			
		u8	m_fSpriteOverflow:1;		// was supposed to be set if more than 8 sprites appear on a scanline but has a hardware bug
		u8	m_fSpriteZeroHit:1;			// set when a nonzero pixel of sprite 0 overlaps a nonzero background pixel; cleared at dot 1 of the pre-render line; used for raster timing
		u8	m_fVerticalBlankStarted:1;	// (0==not in vblank, 1==in vblank) set a dot 1 of line 241, cleared after reading PPUREG_Status and at dot 1 of the pre-render line.
	};
};


union OAM	// Object Attribute Memory, aka sprite data
{
	u32		m_nBits;
	struct 
	{
		u8	m_yTop;			// Y-position in scanlines of the top of the sprite. Delayed by one scanline - you must subtract 1 before writing it here
							// Hide a sprite by writing any values in $EF..$FF here. Sprites are never displayed on the first line of the picture, and it is impossible to place a sprite partially off the top of the screen.

		u8	m_bank:1;		// bank of tiles ($0000 or $1000)
		u8	m_iTile:7;		// tile index for top of sprite, bottom half gets the next sprite

		u8	m_palette:2;	// palette (4..7) of sprite
		u8  m_pad:3;		// should always read back as zero
		u8  m_fPriority:1;	// (0==in front of bg, 1==behind bg) 
		u8	m_fFlipHoriz:1;
		u8	m_fFlipVert:1;

		u8	m_xLeft;		// Values of $F9..$FF result in parts of the sprite to be past the right edge of the screen. 
							// It is not possible to have a sprite partially visible on the left edge. Instead left clipping through PPUREG_Mask can be used to simulate this effect
	};
};

struct Ppu
{
			Ppu()
				{ ; }

	OAM		m_soamSecondary[8];		// internal sprite memory inaccessible to the program; used to cache the sprites rendered in the current scanline. 

};