#pragma once
// various macro tables for opcodes and addressing modes 

		// AMOD				Description
#define ADDRESS_MODE_INFO \
	INFO(Implicit,		"Implicit") \
	INFO(Immediate,		"Immediate: val = op8") \
	INFO(Relative,		"Relative: PC = PC + op8") \
	INFO(Accumulator,	"Accumulator: val = A register") \
	INFO(Absolute,		"Absolute: val = op16") \
	INFO(AbsoluteX,		"AbsoluteX: val = op16 + X") \
	INFO(AbsoluteY,		"AbsoluteY: val = op16 + Y") \
	INFO(ZeroPage,		"Zero Page: val = read(op8)") \
	INFO(ZeroPageX,		"Zero Page,X: val = read((operand + X) & 0xFF)") \
	INFO(ZeroPageY,		"Zero Page,Y: val = read((operand + Y) & 0xFF)") \
	INFO(Indirect,		"Indirect: PC = read(op16) | (read(op16 + 1) << 8)") \
	INFO(IndirectX,		"IndirectX: val = read(read((op8 + X) & 0xFF) | read((op8 + X + 1) & 0xFF) << 8") \
	INFO(IndirectY,		"IndirectY: val = read(read(op8) + read((op8 + 1) & 0xFF) << 8 + Y)")



 //			(ADDK, ADMD, 		cB
#define ADDRESS_MODE_RW_INFO \
        INFO(IMP, Implicit, 	1) \
        INFO(IMM, Immediate, 	2) \
        INFO(REL, Relative, 	2) \
        INFO(ACC, Accumulator,	1) \
		INFO(ABS, Absolute,		3) \
		INFO(AXR, AbsoluteX,	3) \
		INFO(AYR, AbsoluteY,	3) \
		INFO(AXW, AbsoluteX,	3) \
		INFO(AYW, AbsoluteY,	3) \
        INFO(ZPG, ZeroPage,		2) \
        INFO(ZPX, ZeroPageX,	2) \
        INFO(ZPY, ZeroPageY,	2) \
		INFO(IND, Indirect,		3) \
		INFO(IXR, IndirectX,	2) \
		INFO(IYR, IndirectY,	2) \
		INFO(IXW, IndirectX,	2) \
		INFO(IYW, IndirectY,	2)



//				(Opcode, Hint)
#define OPCODE_KIND_INFO \
		OPCODE(ADC, Arithmetic, "Add With Carry") \
		OPCODE(AND, Logic, "Logical And") \
		OPCODE(ASL, Arithmetic, "Arithmetic Shift Left") \
		OPCODE(BCC, Branch, "Branch if Carry Clear") \
		OPCODE(BCS, Branch, "Branch if Carry Set") \
		OPCODE(BEQ, Branch, "Branch if Equal") \
		OPCODE(BIT, Flags, "Bit Test") \
		OPCODE(BMI, Branch, "Branch if Minus") \
		OPCODE(BNE, Branch, "Branch if not Equal") \
		OPCODE(BPL, Branch, "Branch if Positive") \
		OPCODE(BRK, Branch, "Force Interrupt") \
		OPCODE(BVC, Branch, "Branch if Overflow Clear") \
		OPCODE(BVS, Branch, "Branch if Overflow Set") \
		OPCODE(CLC, Flags, "Clear Carry Flag") \
		OPCODE(CLD, Flags, "Clear Decimal Mode") \
		OPCODE(CLI, Flags, "Clear Interrupt Disable") \
		OPCODE(CLV, Flags, "Clear Overflow Flag") \
		OPCODE(CMP, Arithmetic, "Compare") \
		OPCODE(CPX, Arithmetic, "Compare X Register") \
		OPCODE(CPY, Arithmetic, "Compare Y Register") \
		OPCODE(DEC, Arithmetic, "Decrement Memory") \
		OPCODE(DEX, Arithmetic, "Decrement X Register") \
		OPCODE(DEY, Arithmetic, "Decrement Y Register") \
		OPCODE(EOR, Logic, "Exclusive OR") \
		OPCODE(INC, Arithmetic, "Increment Memory") \
		OPCODE(INX, Arithmetic, "Increment X Register") \
		OPCODE(INY, Arithmetic, "Increment Y Register") \
		OPCODE(JMP, Branch, "Jump") \
		OPCODE(JSR, Branch, "Jump to Subroutine") \
		OPCODE(LDA, Memory, "Load to Accumulator") \
		OPCODE(LDX, Memory, "Load X Register") \
		OPCODE(LDY, Memory, "Load Y Register") \
		OPCODE(LSR, Arithmetic, "Logical Shift Right") \
		OPCODE(NOP, Nop, "No Operation") \
		OPCODE(ORA, Logic, "Logical Inclusive OR") \
		OPCODE(PHA, Stack, "Push Accumulator") \
		OPCODE(PHP, Stack, "Push Processor Status") \
		OPCODE(PLA, Stack, "Pull Accumulator") \
		OPCODE(PLP, Stack, "Pull Processor Status") \
		OPCODE(ROL, Arithmetic, "Rotate Left") \
		OPCODE(ROR, Arithmetic, "Rotate Right") \
		OPCODE(RTI, Branch, "Return From Interrupt") \
		OPCODE(RTS, Branch, "Return From Subroutine") \
		OPCODE(SBC, Arithmetic, "Subtract With Carry") \
		OPCODE(SEC, Flags, "Set Carry Flag") \
		OPCODE(SED, Flags, "Set Decimal Flag") \
		OPCODE(SEI, Flags, "Set Interrupt Disable") \
		OPCODE(STA, Memory, "Store Accumulator") \
		OPCODE(STX, Memory, "Store X Register") \
		OPCODE(STY, Memory, "Store Y Register") \
		OPCODE(TAX, Transfer, "Transfer Accumulator to X") \
		OPCODE(TAY, Transfer, "Transfer Accumulator to Y") \
		OPCODE(TSX, Transfer, "Transfer Stack Pointer to X") \
		OPCODE(TXA, Transfer, "Transfer X to Accumulator") \
		OPCODE(TXS, Transfer, "Transfer X to Stack Pointer") \
		OPCODE(TYA, Transfer, "Transfer Y to Accumulator") \
		OPCODE(AHX, Illegal, "ILLEGAL: ") \
		OPCODE(ALR, Illegal, "ILLEGAL: ") \
		OPCODE(ANC, Illegal, "ILLEGAL: ") \
		OPCODE(ARR, Illegal, "ILLEGAL: ") \
		OPCODE(AXS, Illegal, "ILLEGAL: X := A & X - Immediate") \
		OPCODE(DCP, Illegal, "ILLEGAL: ") \
		OPCODE(ISC, Illegal, "ILLEGAL: ") \
		OPCODE(KIL, Illegal, "ILLEGAL: Kill Proccess") \
		OPCODE(LAS, Illegal, "ILLEGAL: ") \
		OPCODE(LAX, Illegal, "ILLEGAL: ") \
		OPCODE(RLA, Illegal, "ILLEGAL: ") \
		OPCODE(RRA, Illegal, "ILLEGAL: ") \
		OPCODE(SAX, Illegal, "ILLEGAL: Store A & X") \
		OPCODE(SHX, Illegal, "ILLEGAL: ") \
		OPCODE(SHY, Illegal, "ILLEGAL: ") \
		OPCODE(SLO, Illegal, "ILLEGAL: ") \
		OPCODE(SRE, Illegal, "ILLEGAL: ") \
		OPCODE(TAS, Illegal, "ILLEGAL: ") \
		OPCODE(XAA, Illegal, "ILLEGAL: A := X & Immediate, UNSTABLE!")



#define OPCODE_TABLE \
        OP(00, BRK,IMP, 7)  OP(01, ORA,IXR, 6)  OP(02, KIL,IMP, 1)  OP(03, SLO,IXR, 8)  OP(04, NOP,ZPG, 3)  OP(05, ORA,ZPG, 3)  OP(06, ASL,ZPG, 5)  OP(07, SLO,ZPG, 5) OP(08, PHP,IMP, 3)  OP(09, ORA,IMM, 2)  OP(0A, ASL,IMP, 2)  OP(0B, ANC,IMM, 2)  OP(0C, NOP,ABS, 4)  OP(0D, ORA,ABS, 4)  OP(0E, ASL,ABS, 6)  OP(0F, SLO,ABS, 6) \
        OP(10, BPL,REL,-1)  OP(11, ORA,IYR,-5)  OP(12, KIL,IMP, 1)  OP(13, SLO,IYR, 8)  OP(14, NOP,ZPX, 4)  OP(15, ORA,ZPX, 4)  OP(16, ASL,ZPX, 6)  OP(17, SLO,ZPG, 6) OP(18, CLC,IMP, 2)  OP(19, ORA,AYR,-4)  OP(1A, NOP,IMP, 2)  OP(1B, SLO,AXW, 7)  OP(1C, NOP,AXR,-4)  OP(1D, ORA,AXR,-4)  OP(1E, ASL,AXW, 7)  OP(1F, SLO,AXW, 7) \
        OP(20, JSR,ABS, 2)  OP(21, AND,IXR, 6)  OP(22, KIL,IMP, 1)  OP(23, RLA,IXR, 8)  OP(24, BIT,ZPG, 3)  OP(25, AND,ZPG, 3)  OP(26, ROL,ZPG, 5)  OP(27, RLA,ZPG, 5) OP(28, PLP,IMP, 4)  OP(29, AND,IMM, 2)  OP(2A, ROL,IMP, 2)  OP(2B, ANC,IMM, 2)  OP(2C, BIT,ABS, 4)  OP(2D, AND,ABS, 4)  OP(2E, ROL,ABS, 6)  OP(2F, RLA,ABS, 6) \
        OP(30, BMI,REL,-1)  OP(31, AND,IYR,-5)  OP(32, KIL,IMP, 1)  OP(33, RLA,IYR, 8)  OP(34, NOP,ZPX, 4)  OP(35, AND,ZPX, 4)  OP(36, ROL,ZPX, 6)  OP(37, RLA,ZPG, 6) OP(38, SEC,IMP, 2)  OP(39, AND,AYR,-4)  OP(3A, NOP,IMP, 2)  OP(3B, RLA,AXW, 7)  OP(3C, NOP,AXR,-4)  OP(3D, AND,AXR,-4)  OP(3E, ROL,AXW, 7)  OP(3F, RLA,AYW, 7) \
        OP(40, RTI,IMP, 6)  OP(41, EOR,IXR, 6)  OP(42, KIL,IMP, 1)  OP(43, SRE,IXR, 8)  OP(44, NOP,ZPG, 3)  OP(45, EOR,ZPG, 3)  OP(46, LSR,ZPG, 5)  OP(47, SRE,ZPG, 5) OP(48, PHA,IMP, 3)  OP(49, EOR,IMM, 2)  OP(4A, LSR,IMP, 2)  OP(4B, ALR,IMM, 2)  OP(4C, JMP,ABS, 3)  OP(4D, EOR,ABS, 4)  OP(4E, LSR,ABS, 6)  OP(4F, SRE,ABS, 6) \
        OP(50, BVC,REL,-2)  OP(51, EOR,IYR,-5)  OP(52, KIL,IMP, 1)  OP(53, SRE,IYR, 8)  OP(54, NOP,ZPX, 4)  OP(55, EOR,ZPX, 4)  OP(56, LSR,ZPX, 6)  OP(57, SRE,ZPG, 6) OP(58, CLI,IMP, 2)  OP(59, EOR,AYR,-4)  OP(5A, NOP,IMP, 2)  OP(5B, SRE,AYW, 7)  OP(5C, NOP,AXR,-4)  OP(5D, EOR,AXR,-4)  OP(5E, LSR,AXW, 7)  OP(5F, SRE,AXW, 7) \
        OP(60, RTS,IMP, 6)  OP(61, ADC,IXR, 6)  OP(62, KIL,IMP, 1)  OP(63, RRA,IXR, 8)  OP(64, NOP,ZPG, 3)  OP(65, ADC,ZPG, 3)  OP(66, ROR,ZPG, 5)  OP(67, RRA,ZPG, 5) OP(68, PLA,IMP, 4)  OP(69, ADC,IMM, 2)  OP(6A, ROR,IMP, 2)  OP(6B, ARR,IMM, 2)  OP(6C, JMP,IND, 5)  OP(6D, ADC,ABS, 4)  OP(6E, ROR,ABS, 6)  OP(6F, RRA,ABS, 6) \
        OP(70, BVS,REL,-2)  OP(71, ADC,IYR,-5)  OP(72, KIL,IMP, 1)  OP(73, RRA,IYR, 8)  OP(74, NOP,ZPX, 4)  OP(75, ADC,ZPX, 4)  OP(76, ROR,ZPX, 6)  OP(77, RRA,ZPG, 6) OP(78, SEI,IMP, 2)  OP(79, ADC,AYR,-4)  OP(7A, NOP,IMP, 2)  OP(7B, RRA,AXW, 7)  OP(7C, NOP,AXR,-4)  OP(7D, ADC,AXR,-4)  OP(7E, ROR,AXW, 7)  OP(7F, RRA,AXW, 7) \
        OP(80, NOP,IMM, 2)  OP(81, STA,IXW, 6)  OP(82, NOP,IMM, 2)  OP(83, SAX,IXR, 6)  OP(84, STY,ZPG, 3)  OP(85, STA,ZPG, 3)  OP(86, STX,ZPG, 3)  OP(87, SAX,ZPG, 3) OP(88, DEY,IMP, 2)  OP(89, NOP,IMM, 2)  OP(8A, TXA,IMP, 2)  OP(8B, XAA,IMM, 2)  OP(8C, STY,ABS, 4)  OP(8D, STA,ABS, 4)  OP(8E, STX,ABS, 4)  OP(8F, SAX,ABS, 4) \
        OP(90, BCC,REL,-2)  OP(91, STA,IYW, 6)  OP(92, KIL,IMP, 1)  OP(93, AHX,IYR, 6)  OP(94, STY,ZPX, 4)  OP(95, STA,ZPX, 4)  OP(96, STX,ZPY, 4)  OP(97, SAX,ZPG, 4) OP(98, TYA,IMP, 2)  OP(99, STA,AYW, 5)  OP(9A, TXS,IMP, 2)  OP(9B, TAS,AYW, 5)  OP(9C, SHY,AXW, 5)  OP(9D, STA,AXW, 5)  OP(9E, SHX,AYW, 5)  OP(9F, AHX,AYW, 5) \
        OP(A0, LDY,IMM, 2)  OP(A1, LDA,IXR, 6)  OP(A2, LDX,IMM, 2)  OP(A3, LAX,IXR, 6)  OP(A4, LDY,ZPG, 3)  OP(A5, LDA,ZPG, 3)  OP(A6, LDX,ZPG, 3)  OP(A7, LAX,ZPG, 3) OP(A8, TAY,IMP, 2)  OP(A9, LDA,IMM, 2)  OP(AA, TAX,IMP, 2)  OP(AB, LAX,IMM, 2)  OP(AC, LDY,ABS, 4)  OP(AD, LDA,ABS, 4)  OP(AE, LDX,ABS, 4)  OP(AF, LAX,ABS, 4) \
        OP(B0, BCS,REL,-2)  OP(B1, LDA,IYR,-5)  OP(B2, KIL,IMP, 1)  OP(B3, LAX,IYR,-5)  OP(B4, LDY,ZPX, 4)  OP(B5, LDA,ZPX, 4)  OP(B6, LDX,ZPY, 4)  OP(B7, LAX,ZPG, 4) OP(B8, CLV,IMP, 2)  OP(B9, LDA,AYR,-4)  OP(BA, TSX,IMP, 2)  OP(BB, LAS,AYR,-4)  OP(BC, LDY,AXR,-4)  OP(BD, LDA,AXR,-4)  OP(BE, LDX,AYR,-4)  OP(BF, LAX,AYR,-4) \
        OP(C0, CPY,IMM, 2)  OP(C1, CMP,IXR, 6)  OP(C2, NOP,IMM, 2)  OP(C3, DCP,IXR, 8)  OP(C4, CPY,ZPG, 3)  OP(C5, CMP,ZPG, 3)  OP(C6, DEC,ZPG, 5)  OP(C7, DCP,ZPG, 5) OP(C8, INY,IMP, 2)  OP(C9, CMP,IMM, 2)  OP(CA, DEX,IMP, 2)  OP(CB, AXS,IMM, 2)  OP(CC, CPY,ABS, 4)  OP(CD, CMP,ABS, 4)  OP(CE, DEC,ABS, 4)  OP(CF, DCP,ABS, 6) \
        OP(D0, BNE,REL,-2)  OP(D1, CMP,IYR,-5)  OP(D2, KIL,IMP, 1)  OP(D3, DCP,IYR, 8)  OP(D4, NOP,ZPX, 4)  OP(D5, CMP,ZPX, 4)  OP(D6, DEC,ZPX, 6)  OP(D7, DCP,ZPG, 6) OP(D8, CLD,IMP, 2)  OP(D9, CMP,AYR,-4)  OP(DA, NOP,IMP, 2)  OP(DB, DCP,AYW,-4)  OP(DC, NOP,AYR,-4)  OP(DD, CMP,AXR,-4)  OP(DE, DEC,AXW,-4)  OP(DF, DCP,AXW, 7) \
        OP(E0, CPX,IMM, 2)  OP(E1, SBC,IXR, 6)  OP(E2, NOP,IMM, 2)  OP(E3, ISC,IXR, 8)  OP(E4, CPX,ZPG, 3)  OP(E5, SBC,ZPG, 3)  OP(E6, INC,ZPG, 5)  OP(E7, ISC,ZPG, 5) OP(E8, INX,IMP, 2)  OP(E9, SBC,IMM, 2)  OP(EA, NOP,IMP, 2)  OP(EB, SBC,IMM, 2)  OP(EC, CPX,ABS, 4)  OP(ED, SBC,ABS, 4)  OP(EE, INC,ABS, 4)  OP(EF, ISC,ABS, 6) \
        OP(F0, BEQ,REL,-2)  OP(F1, SBC,IYR,-5)  OP(F2, KIL,IMP, 1)  OP(F3, ISC,IYR, 8)  OP(F4, NOP,ZPX, 4)  OP(F5, SBC,ZPX, 4)  OP(F6, INC,ZPX, 6)  OP(F7, ISC,ZPG, 6) OP(F8, SED,IMP, 2)  OP(F9, SBC,AYR,-4)  OP(FA, NOP,IMP, 2)  OP(FB, ISC,AYW,-4)  OP(FC, NOP,AYR,-4)  OP(FD, SBC,AXR,-4)  OP(FE, INC,AXW,-4)  OP(FF, ISC,AXW, 7) 