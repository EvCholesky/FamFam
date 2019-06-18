#pragma once

#include "Array.h"
#include "Common.h"
#include "Opcodes.h"
#include "PPu.h"

static const int kCBRamPhysical = 2 * 1024; // 0x800

struct Famicom;
struct Platform;
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
					,m_fTriggerNmi(false)
					,m_tickc(0)
						{ ; }

	u8				m_a;
	u8				m_x;
	u8				m_y;
	u8				m_p;				// flags 
	u8				m_sp;				// stack register
	u16				m_pc;				// program counter

	bool			m_fTriggerNmi;		// nmi has been triggered, but not handled
	s64				m_tickc;			// # of cpu cycles elapsed
										// 1 cpu cycle == 3 Ppu cycle
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

typedef u8 (*PFnReadMemCallback)(Famicom * pFam, u16 addr);  
typedef void (*PFnWriteMemCallback)(Famicom * pFam, u16 addr, u8 b);  
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

u8 U8ReadOpenBus(Famicom * pFam, u16 addr);
u8 U8ReadPpuStatus(Famicom * pFam, u16 addr);
u8 U8ReadPpuReg(Famicom * pFam, u16 addr);
u8 U8ReadControllerReg(Famicom * pFam, u16 addr);

void WriteOpenBus(Famicom * pFam, u16 addr, u8 b);
void WritePpuReg(Famicom * pFam, u16 addr, u8 b);
void WriteOamDmaRegister(Famicom * pFam, u16 addr, u8 b);
void WriteControllerLatch(Famicom * pFam, u16 addr, u8 b);


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
					:m_tickp(0)
					,m_pModel(nullptr)
					,m_pCart(nullptr)
						{ ; }

	Cpu  			m_cpu;
	Cpu				m_cpuPrev;

	PpuTiming		m_ptimCpu;		// how far has the cpu simulation processed the ppuClock
	u64				m_tickp;		// how far has the ppu simulated

	Ppu				m_ppu;
	PpuCommandList	m_ppucl;

	Model *			m_pModel;
	Cart *			m_pCart;
	MemoryMap 		m_memmp;
};

// advance one cpu clock cycle
inline void TickCpu(Famicom * pFam)
{
	++pFam->m_cpu.m_tickc;
	AdvancePpuTiming(pFam, pFam->m_cpu.m_tickc, &pFam->m_memmp);
}

inline u8 U8ReadMem(Famicom * pFam, u16 addr)
{
	auto pMemmp = &pFam->m_memmp;
	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];
	if (memdesc.m_iMemcb)
	{
		TickCpu(pFam);
		return (*pMemmp->m_aryMemcb[memdesc.m_iMemcb].m_pFnReadmem)(pFam, addr);
	}

	u8 * pB = pMemmp->m_mpAddrPB[addr];

	u8 b = *pB; 
	pMemmp->m_bPrevBus = b;
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
	auto pMemmp = &pFam->m_memmp;
	const MemoryDescriptor & memdesc = pMemmp->m_mpAddrMemdesc[addr];
	if (memdesc.m_iMemcb)
	{
		(*pMemmp->m_aryMemcb[memdesc.m_iMemcb].m_pFnWritemem)(pFam, addr, b);
		TickCpu(pFam);
		return;
	}

	pMemmp->m_bPrevBus = b;

	u8 bDummy;
	u8 * pB = ((memdesc.m_fmem & FMEM_ReadOnly) == 0) ? pMemmp->m_mpAddrPB[addr] : &bDummy;
	*pB = b;
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
	OPCAT_Illegal,	// NOTE: this category is not the way to check for unofficial instructions, 
					//  some legal operands have unofficial addressing modes
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
	bool			m_fIsOfficial;
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

void SetPowerUpPreLoad(Famicom * pFam, u16 fpow);
void SetPowerUpState(Famicom * pFam, u16 fpow);
void SetPpuPowerUpState(Famicom * pFam, u16 fpow);

void StaticInitFamicom(Famicom * pFam, Platform * pPlat);
void ExecuteFamicomFrame(Famicom * pFam);
bool FTryAllLogTests();

