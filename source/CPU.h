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

struct IMapper // tag = mapr
{
	//virtual void OnEnable();
};

struct SMapperNrom : public IMapper // tag = mapr0
{
};

struct SMapperMMC1 : public IMapper // tag = mapr1
{
};

enum FMEM : u8
{
	FMEM_Mapped			= 0x1,
	FMEM_NotifyPPU		= 0x2,
	FMEM_BreakOnRead	= 0x4,
	FMEM_BreakOnWrite	= 0x8,
};

// remapping and flags for every mapped byte
// could be spans?
union SMemoryData // memd
{
	u64			m_nBits;
	struct 
	{
		u16		m_nBase;;
		u16		m_iMemcb;
		u8		m_redirectMask;
		FMEM	m_fmem;
	};
};

IMapper * PMapperFromMapperk(MAPPERK mapperk);

static const u16 kAddrIrq = 0xFFFE; 
static const u16 kAddrReset = 0xFFFC; 
static const u16 kAddrNmi = 0xFFFA; 

struct SMemory // tag = mem
{
};


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
				,m_pMapper(nullptr)
					{ ; }

	Cpu * 		m_pCpu;
	Ppu * 		m_pPpu;
	Model * 	m_pModel;
	Cart *		m_pCart;
	IMapper *	m_pMapper;
};



enum AMOD // Addressing  MODe
{
	AMOD_Implicit, 		// No destination operand, the result is implicit
	AMOD_Immediate, 	// Uses the 8-bit operand itself as the value for the operation, rather than fetching a value from a memory address.
	AMOD_Relative, 		// Branch instructions (e.g. BEQ, BCS) have a relative addressing mode that specifies an 8-bit signed offset relative to the current PC.
	AMOD_Accumulator,	// 
	AMOD_Absolute,		//
	AMOD_AbsoluteX,		// val = READ(arg + X)
	AMOD_AbsoluteY,		// val = READ(arg + Y)
	AMOD_ZeroPage,		// val = Read((arg) & 0xFF)
	AMOD_ZeroPageX,		// val = READ((arg + X) & 0xFF) 
	AMOD_ZeroPageY,		// val = READ((arg + Y) & 0xFF) 
	AMOD_Indirect,		// val = READ(arg16)
	AMOD_IndirectX,		// val = READ(READ((arg + X) & 0xFF) + READ((arg + X + 1) & 0xFF) << 8)
	AMOD_IndirectY,		// val = READ(READ(arg) + READ((arg + 1) & 0xFF) << 8 + Y)
};


#define ADDRMODE(NAME, MODE, CB) AMRW_##NAME,
enum AMRW	// Addressing Mode with Read/Write
{
	ADDRESSING_MODE_INFO
	AMRW_Max,
	AMRW_Min = 0,
	AMRW_Nil = -1,
};
#undef ADDRMODE

struct AddrModeInfo // tag = amodinfo
{
	AMRW		m_amrw;
	AMOD		m_amod;
	int			m_cB;
};

const AddrModeInfo * PAmodinfoFromAmrw(AMRW amrw);

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

u8 U8ReadAddress(Famicom * pFam, u16 addr);
u16 U16ReadAddress(Famicom * pFam, u16 addr);