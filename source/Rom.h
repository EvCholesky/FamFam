
#include "cpu.h"
#include "Array.h"

struct SFamicom;

enum CONSK // tag = CONSole Kind
{
	CONSK_Nes,
	CONSK_VsSystem,
	CONSK_Playchoice10,
	CONSK_Extended
};

union RomFlags // tag = romf
{
	u16	m_nBits;
	struct 
	{
		u16		m_fMirrorVert:1;			// (0==horiz, CIRAM A10 = PPU A11, 1=vert CIRAM A10 = PPU A10)
		u16		m_fHasBatteryBackedRam:1;
		u16		m_fHasTrainer:1;			// 512byte at $7000-$71FF
		u16		m_fIngnoreMirroring:1;		// ignore mirroring control or above mirroring bit; instead provide four screen vram 
		u16		m_nMapperLow:4;				// lower nibble of mapper number

		u16		m_consk:2;
		u16		m_nFormat:2;				// iff 2 flags bytes 8..15 are NES 2.0 format
		u16		m_nMapperHigh:4;			// upper nibble of mapper number
	};
};

union RomFlagsEx // tag = romfx
{
	u32	m_nBits;
	struct 
	{
		u8	m_nMapperMSB:4;
		u8	m_nSubMapper:4;

		u8	m_sizePrgRomMSB:4;
		u8	m_sizeChrRomMSB:4;

		u8	m_cShiftPrgRam:4;
		u8	m_cShiftPrgNvram:4;

		u8	m_cShiftChrRam:4;
		u8	m_cShiftChrNvram:4;
	};
};



// header for ines/nes2.0 roms
struct RomHeader	// tag = head
{
	u32				m_nCookie;			// should be "NES" followed by EOL
	u8				m_cPagePrgRom;		// number of 16KiB pages of program data
	u8				m_cPageChrRom;		// number of 8KiB pages of chr data
	RomFlags		m_romf;

	RomFlagsEx		m_romfx;
	u8				m_timingPpuCpu;
	u8				m_typePpuHardware;
	u8				m_miscRoms;	
	u8				m_expansionDevice;
};

enum MAPREG1
{
	MAPREG1_Control,	
	MAPREG1_ChrPage0,	
	MAPREG1_ChrPage1,	
	MAPREG1_PrgPage,	
	MAPREG1_Max
};

struct MapperMMC1 // tag = mapr1
{
			MapperMMC1();

	u8		m_cBitShift;				// how many bits have been shifted in via writes to 0x8000..0xFFFFF
	u8		m_bShift;				// how many bits have been shifted in via writes to 0x8000..0xFFFFF
	u32		m_nReg;
	u32		m_nRegPrev;
	u8		m_aBReg[MAPREG1_Max];
	u8		m_aBRegPrev[MAPREG1_Max];
	u8		m_iMemcbWriteMem;
};

void UpdateBanks(Famicom * pFam, MapperMMC1 * pMapr1);



struct Cart
{
					Cart()
					:m_mapperk(MAPPERK_Nil)
					,m_pHead(nullptr)
					,m_aryAddrInstruct(1014)
					,m_pBPrgRom(nullptr)
					,m_pBChrRom(nullptr)
					,m_cBPrgRom(0)
					,m_cBPrgRam(0)
					,m_cBChrRom(0)
					,m_cBChrRam(0)
					,m_addrPrgMappedMin(0)
					,m_addrPrgMappedMax(0)
					,m_fRecomputeAddrInstruct(true)
						{ ; }

	MAPPERK			m_mapperk;
	RomHeader *		m_pHead;
	DynAry<u16>		m_aryAddrInstruct;	// cached starting address for each instruction (for disasembly window)
										// (one entry per debugger line - to allow clipping the offscreen instructions)

	u8 *			m_pBPrgRom;
	u8 *			m_pBChrRom;
	int				m_cBPrgRom;
	int				m_cBPrgRam;
	int				m_cBChrRom;
	int				m_cBChrRam;
	int				m_addrPrgMappedMin;
	int				m_addrPrgMappedMax;
	bool			m_fRecomputeAddrInstruct;
};


void AppendAddrOffset(Ppu * pPpu, DynAry<u16> * pAryAddrInstruct, u16 addrBase, int cB);

bool FTryLoadRomFromFile(const char * pChzFilename, Cart * pCart, Famicom * pFam, FPOW fpow);
bool FTryLoadRom(u8 * pB, u64 cB, Cart * pCart, Famicom * pFam, FPOW fpow);
void CloseRom(Cart * pCart);