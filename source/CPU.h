#pragma once

#include "Array.h"
#include "Common.h"
#include "Opcodes.h"
#include "PPu.h"

static const int kCBRamPhysical = 2 * 1024; // 0x800

struct Famicom;
struct Ppu;
struct Cart;

enum FCPU : u8
{
	FCPU_None				= 0x0,
	FCPU_Carry				= 0x1,
	FCPU_Zero				= 0x2,
	FCPU_InterruptDisable	= 0x4,
	FCPU_DecimalMode		= 0x8,
	FCPU_Break				= 0x10,
	FCPU_Unused				= 0x20,
	FCPU_Overflow			= 0x40,
	FCPU_Negative			= 0x80,
};

struct Cpu // tag = cpu
{
					Cpu()
					:m_a(0)
					,m_x(0)
					,m_y(0)
					,m_p(FCPU_None)
					,m_sp(0)
					,m_pc(0)
					,m_cCycleCpu(0)
						{ ; }

	u8				m_a;
	u8				m_x;
	u8				m_y;
	u8				m_p;				// flags 
	u8				m_sp;				// stack register
	u16				m_pc;				// program counter

	s64				m_cCycleCpu;		// count of cycles elapsed this frame 		
										// 1 cpu cycle == 3 Ppu cycle
};

struct PpuTiming // tag = ptim
{
					PpuTiming()
					:m_cPclockScanline(0)
					,m_cScanline(0)
					,m_cFrame(0)
					,m_cCycleCpuPrev(0)
						{ ; }

	u16				m_cPclockScanline;	// Ppu cycles since the start of the scanline
	u16				m_cScanline;		// Scanlines since the start of the frame
	int				m_cFrame;			// Total frames since power up
	s64				m_cCycleCpuPrev;	// count of cycles elapsed since the last ppu update
};

enum MEMSP : u16 // MEMory SPan
{
	MEMSP_ZeroPageMin	= 0x0000,
	MEMSP_ZeroPageMax	= 0x0100,
	MEMSP_StackMin		= 0x0100,
	MEMSP_StackMax		= 0x0200,
	MEMSP_RamMin		= 0x0200,
	MEMSP_RamMax		= 0x0800,
	MEMSP_RamMirrorMin	= 0x0800,
	MEMSP_RamMirrorMax	= 0x2000,

	MEMSP_PpuRegisterMin = 0x2000,
	MEMSP_PpuRegisterMax = 0x2008,
	MEMSP_PpuMirrorsMin = 0x2008,
	MEMSP_PpuMirrorsMax = 0x4000,
	MEMSP_IoRegisterMin = 0x4000,
	MEMSP_IoRegisterMax = 0x4020,

	MEMSP_ExpansionMin	= 0x4020,
	MEMSP_ExpansionMax	= 0x6000,

	MEMSP_SramMin		= 0x6000,	// cartridge ram
	MEMSP_SramMax 		= 0x8000,

	MEMSP_PRGRomLowMin	= 0x8000,
	MEMSP_PRGRomLowMax	= 0xC000,
	MEMSP_PRGRomHighMin	= 0xC000,
	MEMSP_PRGRomHighMax	= 0x1000,
};

enum MAPPERK
{
	MAPPERK_NROM,
	MAPPERK_MMC1,
	MAPPERK_UxROM,
	MAPPERK_InesMapper3,
	MAPPERK_MMC3,

	MAPPERK_Max,
	MAPPERK_Min = 0,
	MAPPERK_Nil = -1,
};

static const u16 kAddrIrq = 0xFFFE; 
static const u16 kAddrReset = 0xFFFC; 
static const u16 kAddrNmi = 0xFFFA; 

static const int kCBIndirect = 1024; // one indirect per kb
static const int kCBAddressable = 64 * 1024;

struct MemoryMap;
void ClearMemmp(MemoryMap * pMemmp);

enum FMEM : u8
{
	FMEM_None			= 0x0,
	FMEM_Mapped			= 0x1,
	FMEM_ReadOnly		= 0x2,
	FMEM_BreakOnRead	= 0x4,
	FMEM_BreakOnWrite	= 0x8,
};

struct MemoryDescriptor //  tag = memdesc
{
				MemoryDescriptor()
				:m_fmem(FMEM_None)
				,m_iMemcb(0)
					{ ; }

				MemoryDescriptor(FMEM fmem, u8 iMemcb = 0)
				:m_fmem(fmem)
				,m_iMemcb(iMemcb)
					{ ; }

	u8		m_fmem;
	u8		m_iMemcb;	// index into the memory callback table, 0 == no cb
};

typedef u8 (*PFnReadMemCallback)(MemoryMap * pMemmp, u16 addr);  
typedef void (*PFnWriteMemCallback)(MemoryMap * pMemmp, u16 addr, u8 b);  
struct MemoryCallback // tag = memcb
{
	PFnReadMemCallback		m_pFnReadmem;
	PFnWriteMemCallback		m_pFnWritemem;
};
 
struct AddressSpan // tag = addrsp
{
	u16		m_addrMin;
	u16		m_addrMax;
};

struct MemoryMap // tag = memmp
{
								MemoryMap();
								~MemoryMap()
									{ ClearMemmp(this); }

	u8							m_aBRaw[kCBAddressable];				// 64k of raw memory
	u8 *						m_mpAddrPB[kCBAddressable];
	MemoryDescriptor			m_mpAddrMemdesc[kCBAddressable];
	FixAry<MemoryCallback, 128>	m_aryMemcb;
	u8							m_bPrevBus;
	u8							m_bPrevBusPpu;
};

AddressSpan AddrspMapMemory(MemoryMap * pMemmp, u32 addrMin, u32 addrMax, const MemoryDescriptor * pMemdesc = nullptr, int cMemdesc = 0);
AddressSpan AddrspMarkUnmapped(MemoryMap * pMemmp, u32 addrMin, u32 addrMax);
void MapMirrored(MemoryMap * pMemmp, AddressSpan addrspBase, u32 addrMin, u32 addrMax, const MemoryDescriptor * aMemdec = nullptr, int cMemdesc = 0);
u8 IMemcbAllocate(MemoryMap * pMemmp, PFnReadMemCallback pFnRead, PFnWriteMemCallback pFnWrite); 

void TestMemoryMap(MemoryMap * pMemmp);
void VerifyPrgRom(MemoryMap * pMemmp, u8 * pBPrgRom, u32 addrMin, u32 addrMax);

u8 U8PeekMem(MemoryMap * pMemmp, u16 addr); // return an address value without the machine emulating a memory read
u8 U8ReadMem(Famicom * pFam, u16 addr);
u16 U16ReadMem(Famicom * pFam, u16 addr);

void PokeMemU8(MemoryMap * pMemmp, u16 addr, u8 b);
void WriteMemU8(Famicom * pFam, u16 addr, u8 b);
void WriteMemU16(Famicom * pFam, u16 addr, u16 n);

u8 U8PeekWriteOnlyPpuReg(MemoryMap * pMemmp, u16 addr);
u8 U8PeekPpuStatus(MemoryMap * pMemmp, u16 addr);
u8 U8PeekPpuReg(MemoryMap * pMemmp, u16 addr);
void PokePpuReg(MemoryMap * pMemmp, u16 addr, u8 b);

void AdvancePpuTiming(PpuTiming * pPtim, s64 cCycleCpu, MemoryMap * pMemmp);

enum MODELK
{
	MODELK_NTSC,
	MODELK_PAL
};

struct Model // tag = model
{
};

struct Famicom // tag = fam
{
					Famicom()
					:m_pModel(nullptr)
					,m_pCart(nullptr)
						{ ; }

	Cpu  			m_cpu;
	Cpu				m_cpuPrev;
	PpuTiming		m_ptimCpu;
	Ppu				m_ppu;
	Model *			m_pModel;
	Cart *			m_pCart;
	MemoryMap 		m_memmp;
};

// advance one cpu clock cycle
inline void TickCpu(Famicom * pFam)
{
	++pFam->m_cpu.m_cCycleCpu;
	AdvancePpuTiming(&pFam->m_ptimCpu, pFam->m_cpu.m_cCycleCpu, &pFam->m_memmp);
}

inline u8 U8ReadMem(Famicom * pFam, u16 addr)
{
	u8 b = U8PeekMem(&pFam->m_memmp, addr);
	pFam->m_memmp.m_bPrevBus = b;
	TickCpu(pFam);
	return b;
}

inline u16 U16ReadMem(Famicom * pFam, u16 addr)
{
	u16 n = U8ReadMem(pFam, addr) | (u16(U8ReadMem(pFam, addr+1)) << 8);
	return n;
}

inline u16 U16PeekMem(MemoryMap * pMemmp, u16 addr)
{
	u16 n = U8PeekMem(pMemmp, addr) | (u16(U8PeekMem(pMemmp, addr+1)) << 8);
	return n;
}

inline void WriteMemU8(Famicom * pFam, u16 addr, u8 b)
{
	PokeMemU8(&pFam->m_memmp, addr, b);
	TickCpu(pFam);
}

inline void WriteMemU16(Famicom * pFam, u16 addr, u16 n)
{
	WriteMemU8(pFam, addr, u8(n));
	WriteMemU8(pFam, addr+1, u8(n >> 8));
}

#define INFO(AM, DESC) AMOD_##AM,
enum AMOD // Addressing  MODe
{
	ADDRESS_MODE_INFO
	AMOD_Max,
	AMOD_Min = 0,
	AMOD_Nil = -1,
};
#undef INFO

#define INFO(NAME, MODE, CB) AMRW_##NAME,
enum AMRW	// Addressing Mode with Read/Write
{
	ADDRESS_MODE_RW_INFO
	AMRW_Max,
	AMRW_Min = 0,
	AMRW_Nil = -1,
};
#undef INFO

struct AddrModeInfo // tag = amodinfo
{
	AMOD			m_amod;
	const char *	m_pChzDesc;
};

struct AddrModeRwInfo // tag = amrwinfo
{
	AMRW		m_amrw;
	AMOD		m_amod;
	int			m_cB;
};

const AddrModeInfo * PAmodinfoFromAmod(AMOD amod);
const AddrModeRwInfo * PAmrwinfoFromAmrw(AMRW amrw);

enum OPCAT
{
	OPCAT_Arithmetic,
	OPCAT_Branch,
	OPCAT_Flags,
	OPCAT_Illegal,
	OPCAT_Logic,
	OPCAT_Nop,
	OPCAT_Memory,
	OPCAT_Stack,
	OPCAT_Transfer,

	OPCAT_Max,
	OPCAT_Min = 0,
	OPCAT_Nil = -1,
};

#define OPCODE(NAME, OPC, HINT) OPK_##NAME,
enum OPK
{
	OPCODE_KIND_INFO
	OPK_Max,
	OPK_Min			= 0,
	OPK_Nil			= -1,

	OPK_LegalMin	= OPK_Min,
	OPK_LegalMax	= OPK_AHX,
	OPK_IllegalMin	= OPK_AHX,
	OPK_IllegalMax	= OPK_Max
};
#undef OPCODE

struct OpkInfo // tag = opkinfo
{
	const char *	m_pChzName;
	const char *	m_pChzHint;
	OPCAT			m_opcat;
};

struct OpInfo // tag = opinfo
{
	const char *	m_pChzMnemonic;
	OPK				m_opk;
	AMRW			m_amrw;

	u8				m_nOpcode;
	s8				m_cCycle;
	bool			m_fHasBoundaryCycle;
};

const OpkInfo * POpkinfoFromOPK(OPK opk);
const OpInfo * POpinfoFromOpcode(u8 nOpcode);

extern Famicom g_fam;

void DumpAsm(Famicom * pFam, u16 addrMin, int cB);

enum FPOW // Flags for POWer up
{
	FPOW_None		= 0x0,
	FPOW_LogTest	= 0x1,
};

void SetPowerUpState(Famicom * pFam, u16 fpow);
void SetPpuPowerUpState(Famicom * pFam, u16 fpow);

bool FTryAllLogTests();

