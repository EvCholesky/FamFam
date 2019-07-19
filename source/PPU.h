#pragma once
#include "Array.h"
#include "Common.h"

static const int kCBCieramMax = FF_KIB(4);
static const int kCBChrMapped = FF_KIB(8);
static const int kCBVramAddressable = FF_KIB(16);
static const int kCBPalette = 32;
static const int s_cBVramGranularity = 1;		

static const int s_cXChrTile = 16;
static const int s_cYChrTile = 16;
static const int s_dXChrHalf = s_cXChrTile * 8;
static const int s_dYChrHalf = s_cYChrTile * 8;

static const int s_dTickpPerTickc = 3;
static const int s_dTickpPerScanline = 341;
static const int s_cScanlinesPerFrame = 262;
static const int s_cScanlinesVisible = 240;
static const int s_dTickpPerFrame = s_dTickpPerScanline * s_cScanlinesPerFrame;

struct Famicom;
struct MemoryMap;
struct Platform;
struct Ppu;
struct Texture;

enum NTMIR	// Name Table MIRroring
{
	NTMIR_Vertical,		// Vertical mirroring: $2000 equals $2800 and $2400 equals $2C00 (e.g. Super Mario Bros.)
	NTMIR_Horizontal,	// Horizontal mirroring: $2000 equals $2400 and $2800 equals $2C00 (e.g. Kid Icarus)
	NTMIR_OneScreen,	// One-screen mirroring: All nametables refer to the same memory at any given time, and the mapper directly manipulates CIRAM address bit 10
	NTMIR_FourScreen,	// Four-screen mirroring: CIRAM is disabled, and the cartridge contains additional VRAM used for all nametables 
};

struct PpuTiming // tag = ptim
{
					PpuTiming()
					:m_tickp(0)
					,m_tickcPrev(0)
						{ ; }

	u64				m_tickp;			// ppu clock cycle, not resetting every frame
	s64				m_tickcPrev;		// count of cycles at the time of the last ppu update
};

void AdvancePpuTiming(Famicom * pFam, s64 dTickc, MemoryMap * pMemmp);

inline s64 CFrameFromTickp(s64 tickp)
{
	return tickp / s_dTickpPerFrame;
}

inline void SplitTickpFrame(s64 tickp, s64 * pCFrame, s64 * pTickpSubframe)
{
	s64 cFrame = tickp / s_dTickpPerFrame;
	*pCFrame = cFrame;
	*pTickpSubframe = tickp - (cFrame * s_dTickpPerFrame);
}

inline void SplitTickpScanline(s64 tickp, int * pCScanline, int * pTickpScanline )
{
	s64 tickpSubframe = tickp % s_dTickpPerFrame;
	*pCScanline = int(tickpSubframe / s_dTickpPerScanline);
	*pTickpScanline = int(tickpSubframe % s_dTickpPerScanline);
}



enum PCMDK : s8 //Ppu CoMmanD Kind
{
	PCMDK_Read,
	PCMDK_Write,
};


struct PpuCommand  // tag = ppucmd
{
	u8				m_bValue;
	PCMDK			m_pcmdk;
	s8				m_iPpucres;			// result slot to the read result; -1 for no store.
	u16				m_addr;
	u16				m_addrInstDebug;	// address of the instruction that caused this command
	u64				m_tickp;			// BB - could save space if these were stored as dTickp, and we adjusted when we removed the executed spans  
};

struct PpuCommandResult	// tag ppucres
{
	u8				m_fInUse;
	u8				m_b;
};

#define FF_RECORD_PPU_HISTORY 0
static const int s_cPpuchf = 4;
struct PpuCommandHistory // tag = ppuch
{
	PpuCommand		m_ppucmd;	
	s16				m_cScanline;
	s16				m_iCol;
	u16				m_addrTemp;
	u16				m_addrV;
};

struct PpuCommandHistoryFrame // tag = ppuchf
{
	s64							m_cFrame;	
	u16							m_addrTempBegin;
	u16							m_addrVBegin;
	DynAry<PpuCommandHistory>	m_aryPpuch;
};

#if FF_RECORD_PPU_HISTORY
void ClearCommandHistory(Ppu * pPpu);
void RecordCommandHistory(Ppu * pPpu, u64 tickp, PpuCommand * ppucmd);
#else
inline void ClearCommandHistory(Ppu * pPpu) {}
inline void RecordCommandHistory(Ppu * pPpu, u64 tickp, PpuCommand * ppucmd) {} 
#endif

struct PpuCommandList // tag = ppucl
{
							PpuCommandList()
							:m_iPpucmdExecute(0)
								{ ZeroAB(m_aPpucres, sizeof(m_aPpucres));  }
							~PpuCommandList();

	int						m_iPpucmdExecute;		// index of next command to execute
	DynAry<PpuCommand>		m_aryPpucmd;
		
	PpuCommandResult		m_aPpucres[8];
}; 

void AppendPpuCommand(PpuCommandList * pPpucl, PCMDK pchk, u16 addr, u8 bValue, const PpuTiming & ptim, s8 iPpucres = -1, u16 addrDebug = 0);
void UpdatePpu(Famicom * pFam, const PpuTiming & ptimEnd);

union RGBA // tag = rgba 
{
	u32 m_nBits;
	struct 
	{
		u8 m_r;
		u8 m_g;
		u8 m_b;
		u8 m_a;
	};
};
 
enum HWCOL // HardWare COLor indices
{
	HWCOL_White = 0x30,
	HWCOL_Black = 0x3F,
	HWCOL_Max = 64,
	HWCOL_Min = 0,
};

RGBA RgbaFromHwcol(HWCOL hwcol);

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

union PpuControl	// tag = pctrl
{
	u8	m_nBits;
	struct
	{
		u8	m_iBaseNameTableAddr:2;		//(0==$2000, 1==$2400, 2==$2800, 3==$2C00)
		u8	m_dBVramAddressIncrement:1;	// (0==horizontal:add 1, 1==vertical: add 32)
		u8	m_fHighSpritePatternTable:1;// (0==$0000, 1==$1000; ignored in 8x16 mode)
		u8	m_fHighBgPaternTable:1;		// (0==$0000, 1==$1000)
		u8	m_fUse8x16Sprite:1;			// (0==8x8; 1==8x16)
		u8	m_fMasterSlaveSelect:1;		// (0==read backdrop from ext pins; 1==output color on ext pins)
		u8	m_fGenerateVBlankNMI:1;		// generate an NMI at the start of the vertical blanking interval
	};
};

union PpuMask		// tag = pmask
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

union PpuStatus		// tag = pstatus
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

		u8	m_iTile;		// tile index for top of sprite, bottom half gets the next sprite

		u8	m_palette:2;	// palette (4..7) of sprite
		u8  m_pad:3;		// should always read back as zero
		u8  m_fBgPriority:1;	// (0==in front of bg, 1==behind bg) 
		u8	m_fFlipHoriz:1;
		u8	m_fFlipVert:1;

		u8	m_xLeft;		// Values of $F9..$FF result in parts of the sprite to be past the right edge of the screen. 
							// It is not possible to have a sprite partially visible on the left edge. Instead left clipping through PPUREG_Mask can be used to simulate this effect
	};
};

enum PADDR
{
	PADDR_PatternTable0		= 0x0000,
	PADDR_PatternTable1		= 0x1000,
	// nametables are all addressed, but only two are backed by memory - address bit 10 of CIRAM controls mapping
	PADDR_NameTable0		= 0x2000,
	PADDR_NameTable1		= 0x2400,
	PADDR_NameTable2		= 0x2800,
	PADDR_NameTable3		= 0x2C00,
	PADDR_NameTableMirrors	= 0x3000,
	PADDR_PaletteRam		= 0x3F00,		//bgColor, followed 8 3-color palettes where each color is one byte
	PADDR_PaletteMirrors	= 0x3F20,
	PADDR_VramMax			= 0x4000,
};

struct TileLine // tag = til // one ySubtile slice
{
	u8	m_bLow;
	u8	m_bHigh;
	u8	m_iPalette;
};

struct Ppu
{
					Ppu();
					~Ppu();

	PpuControl		m_pctrl;
	PpuMask			m_pmask;
	PpuStatus		m_pstatus;

	u8 				m_aBCieram[kCBCieramMax];			// 4k of physical VRAM, aka CIRAM (ppu actually only has 2k, some carts have an extra 2k
	u8				m_aBChr[kCBChrMapped];				// 8k current mapped Chr rom
	u8				m_aBPalette[kCBPalette];
	u8 *			m_aPVramMap[kCBVramAddressable / s_cBVramGranularity];	// PPU memory mapping is a lot simpler than the CPU
																			//  memory map, so we'll just use a pointer every 32 bytes

	OAM				m_aOam[64];				// internal OAM memory - sprite data
	OAM				m_aOamLine[8];			// internal sprite memory inaccessible to the program; used to cache the sprites rendered in the current scanline. 
	int				m_iOamSpriteZero;		// which sprite in oamLine corresponds to sprite zero
	u32				m_nShiftBg;
	u32				m_nShiftBgAttrib;		// 

	TileLine		m_aTilLine[8];
	TileLine		m_aTilBackground[32];
	TileLine		m_aTilBgCache[3];
	RGBA			m_aRgbaSpriteLine[8*8];	// rgba values for this line's rastered sprites

	u16				m_addrV;				// address used by PPUDATA $2007 and PPUSCROLL $2005
	u16				m_addrTemp;				// TempAddress used by PPUSCROLL
	u8				m_dXScrollFine;
	u8				m_bOamAddr;				// least significant byte of the OAM address
	u8				m_bReadBuffer;
	bool			m_fIsFirstAddrWrite;	// address register latch 

	Texture *		m_pTexScreen;

	FixAry<PpuCommandHistoryFrame, s_cPpuchf>
					m_aryPpuchf;
};

void StaticInitPpu(Ppu * pPpu, Platform * pPlat);
void InitPpuMemoryMap(Ppu * pPpu, size_t cBChr, NTMIR ntmir);
void InitChrMemory(Ppu * pPpu, u16 addrVram, u8 * pBChr, size_t cBChr);
void DrawChrMemory(Ppu * pPpu, Texture * pTex, bool fUse8x16);
void DrawNameTableMemory(Ppu * pPpu, Texture * pTex);

void DrawScreen(Ppu * pPpu, u64 tickpMin, u64 tickpMax);
