#pragma once

#include "Common.h"
#include "Opcodes.h"

static const int kCBRamPhysical = 2 * 1024; // 0x800

struct Ppu;
struct Cart;

enum FCPU : u8
{
	FCPU_Carry				= 0X1,
	FCPU_Zero				= 0X2,
	FCPU_InterruptDisable	= 0X4,
	FCPU_DecimalMode		= 0X8,
	FCPU_Break				= 0X10,
	FCPU_Overflow			= 0X40,
	FCPU_Negative			= 0X80,
};

struct Cpu // tag = cpu
{
			Cpu();

	u8		m_a;
	u8		m_x;
	u8		m_y;
	FCPU	m_p;	// flags 
	u8		m_sp;	// stack register
	u16		m_pc;	// program counter

	u64		m_cCpuCycles; 		// 1 cpu cycle == 3 Ppu cycle
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

	MEMSP_IoRegisterMin = 0x2000,
	MEMSP_IoRegisterMax = 0x2008,
	MEMSP_IoMirrorsMin 	= 0x2008,
	MEMSP_IoUpperMin 	= 0x4000,
	MEMSP_IoUpperMax 	= 0x4020,

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
struct AddressSpan // tab = addrsp
{
	u16		m_addrMin;
	u16		m_addrMax;
};

struct MemoryMap // tag = memmp
{
					MemoryMap();
					~MemoryMap()
						{ ClearMemmp(this); }

	u8 				m_pBRaw[kCBAddressable];				// 64k of raw memory
	u8 *			m_mpAddrPB[kCBAddressable];
	FMEM 			m_mpAddrFmem[kCBAddressable];
	u8				m_bPrevBus;
};

AddressSpan AddrspMapMemory(MemoryMap * pMemmp, u32 addrMin, u32 addrMax, u8 fmem = FMEM_None);
AddressSpan AddrspMarkUnmapped(MemoryMap * pMemmp, u32 addrMin, u32 addrMax);
void MapMirrored(MemoryMap * pMemmp, AddressSpan addrspBase, u32 addrMin, u32 addrMax, u8 fmem = FMEM_None);

void TestMemoryMap(MemoryMap * pMemmp);
void VerifyPrgRom(MemoryMap * pMemmp, u8 * pBPrgRom, u32 addrMin, u32 addrMax);

u8 U8PeekMem(MemoryMap * pMemmp, u16 addr); // return an address value without the machine emulating a memory read
u8 U8ReadMem(MemoryMap * pMemmp, u16 addr);
u16 U16ReadMem(MemoryMap * pMemmp, u16 addr);

void WriteMemU8(MemoryMap * pMemmp, u16 addr, u8 b);
void WriteMemU16(MemoryMap * pMemmp, u16 addr, u16 n);

inline u8 U8ReadMem(MemoryMap * pMemmp, u16 addr)
{
	u8 b = U8PeekMem(pMemmp, addr);
	pMemmp->m_bPrevBus = b;
	return b;
}

inline u16 U16ReadMem(MemoryMap * pMemmp, u16 addr)
{
	u16 n = U8ReadMem(pMemmp, addr) | (u16(U8ReadMem(pMemmp, addr+1)) << 8);
	return n;
}

inline void WriteMemU16(MemoryMap * pMemmp, u16 addr, u16 n)
{
	WriteMemU8(pMemmp, addr, u8(n));
	WriteMemU8(pMemmp, addr+1, u8(n >> 8));
}

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
				:m_pCpu(nullptr)
				,m_pPpu(nullptr)
				,m_pModel(nullptr)
					{ ; }

	Cpu * 		m_pCpu;
	Ppu * 		m_pPpu;
	Model * 	m_pModel;
	Cart *		m_pCart;
	MemoryMap 	m_memmp;
};



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
void SetPowerUpState(Famicom * pFam);
void SetResetState(Famicom * pFam);

