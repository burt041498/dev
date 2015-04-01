// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ArmEmitter.h"
#include "Common/MemoryUtil.h"

#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/GPFifo.h"
#include "Core/HW/Memmap.h"

#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitAsm.h"
#include "Core/PowerPC/JitCommon/JitCache.h"


using namespace ArmGen;

//TODO - make an option
//#if _DEBUG
//	bool enableDebug = false;
//#else
//	bool enableDebug = false;
//#endif

JitArmAsmRoutineManager asm_routines;

static void WriteDual8(u32 val1, u32 val2, u32 addr)
{
	PowerPC::Write_U16(((u16)(u8)val1 << 8) | (u16)(u8)val2, addr);
}

static void WriteDual16(u32 val1, u32 val2, u32 addr)
{
	PowerPC::Write_U32(((u32)(u16)val1 << 16) | (u32)(u16)val2, addr);
}

static void WriteDual32(u32 val1, u32 val2, u32 addr)
{
	PowerPC::Write_U64(((u64)val1 << 32) | (u64)val2, addr);
}

void JitArmAsmRoutineManager::Generate()
{
	enterCode = GetCodePtr();
	PUSH(9, R4, R5, R6, R7, R8, R9, R10, R11, _LR);
	// Take care to 8-byte align stack for function calls.
	// We are misaligned here because of an odd number of args for PUSH.
	// It's not like x86 where you need to account for an extra 4 bytes
	// consumed by CALL.
	SUB(_SP, _SP, 4);

	MOVI2R(R9, (u32)&PowerPC::ppcState.spr[0]);
	MOVI2R(R8, (u32)Memory::physical_base);

	FixupBranch skipToRealDispatcher = B();
	dispatcher = GetCodePtr();
		printf("Dispatcher is %p\n", dispatcher);

		// Downcount Check
		// The result of slice decrementation should be in flags if somebody jumped here
		// IMPORTANT - We jump on negative, not carry!!!
		FixupBranch bail = B_CC(CC_MI);

		FixupBranch dbg_exit;
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		{
			MOVI2R(R0, (u32)PowerPC::GetStatePtr());
			LDR(R0, R0);
			TST(R0, PowerPC::CPU_STEPPING);
			FixupBranch not_stepping = B_CC(CC_EQ);
			// XXX: Check for breakpoints
			dbg_exit = B();
			SetJumpTarget(not_stepping);
		}

		SetJumpTarget(skipToRealDispatcher);
		dispatcherNoCheck = GetCodePtr();

		// This block of code gets the address of the compiled block of code
		// It runs though to the compiling portion if it isn't found
			LDR(R12, R9, PPCSTATE_OFF(pc));// Load the current PC into R12

			Operand2 iCacheMask = Operand2(0xE, 2); // JIT_ICACHE_MASK
			BIC(R12, R12, iCacheMask); // R12 contains PC & JIT_ICACHE_MASK here.

			MOVI2R(R14, (u32)jit->GetBlockCache()->iCache.data());

			LDR(R12, R14, R12); // R12 contains iCache[PC & JIT_ICACHE_MASK] here
			// R12 Confirmed this is the correct iCache Location loaded.
			TST(R12, 0x80); // Test  to see if it is a JIT block.

			FixupBranch no_block = B_CC(CC_NEQ);
				// Success, it is our Jitblock.
				MOVI2R(R14, (u32)jit->GetBlockCache()->GetCodePointers());
				// LDR R14 right here to get CodePointers()[0] pointer.
				LSL(R12, R12, 2); // Multiply by four because address locations are u32 in size
				LDR(R14, R14, R12); // Load the block address in to R14

				B(R14);
				// No need to jump anywhere after here, the block will go back to dispatcher start
			SetJumpTarget(no_block);

		// If we get to this point, that means that we don't have the block cached to execute
		// So call ArmJit to compile the block and then execute it.
		MOVI2R(R14, (u32)&Jit);
		BL(R14);

		B(dispatcherNoCheck);

		SetJumpTarget(bail);
		doTiming = GetCodePtr();
		// XXX: In JIT64, Advance() gets called /after/ the exception checking
		// once it jumps back to the start of outerLoop
		QuickCallFunction(R14, (void*)&CoreTiming::Advance);

		// Does exception checking
			LDR(R0, R9, PPCSTATE_OFF(pc));
			STR(R0, R9, PPCSTATE_OFF(npc));
				QuickCallFunction(R14, (void*)&PowerPC::CheckExceptions);
			LDR(R0, R9, PPCSTATE_OFF(npc));
			STR(R0, R9, PPCSTATE_OFF(pc));
		// Check the state pointer to see if we are exiting
		// Gets checked on every exception check
			MOVI2R(R0, (u32)PowerPC::GetStatePtr());
			MVN(R1, 0);
			LDR(R0, R0);
			TST(R0, R1);
			FixupBranch Exit = B_CC(CC_NEQ);

	B(dispatcher);

	SetJumpTarget(Exit);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
		SetJumpTarget(dbg_exit);

	ADD(_SP, _SP, 4);

	POP(9, R4, R5, R6, R7, R8, R9, R10, R11, _PC);  // Returns

	GenerateCommon();

	FlushIcache();
}

void JitArmAsmRoutineManager::GenerateCommon()
{
	// R14 is LR
	// R12 is scratch
	// R11 is scale
	// R10 is the address
	Operand2 mask(3, 1); // ~(Memory::MEMVIEW32_MASK)
	Operand2 arghmask(3, 3); // 0x0C000000
	NEONXEmitter nemit(this);

	const u8* loadPairedIllegal = GetCodePtr();
	BKPT(0x10);

	const u8* loadPairedFloatTwo = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		nemit.VLD1(I_32, D0, R10);
		nemit.VREV32(I_8, D0, D0);

		MOV(_PC, _LR);
	}
	const u8* loadPairedFloatOne = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		nemit.VLD1(I_32, D0, R10);
		nemit.VREV32(I_8, D0, D0);

		MOV(_PC, _LR);
	}
	const u8* loadPairedU8Two = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRB(R12, R10);
		VMOV(S0, R12);

		LDRB(R12, R10, 1);
		VMOV(S1, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT);
		VCVT(S1, S1, TO_FLOAT);

		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedU8One = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRB(R12, R10);
		VMOV(S0, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT);

		VMUL(S0, S0, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedS8Two = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRSB(R12, R10);
		VMOV(S0, R12);

		LDRSB(R12, R10, 1);
		VMOV(S1, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT | IS_SIGNED);
		VCVT(S1, S1, TO_FLOAT | IS_SIGNED);

		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedS8One = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRSB(R12, R10);
		VMOV(S0, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT | IS_SIGNED);

		VMUL(S0, S0, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedU16Two = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRH(R12, R10);
		REV16(R12, R12);
		VMOV(S0, R12);

		LDRH(R12, R10, 2);
		REV16(R12, R12);
		VMOV(S1, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT);
		VCVT(S1, S1, TO_FLOAT);

		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedU16One = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRH(R12, R10);
		REV16(R12, R12);
		VMOV(S0, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT);

		VMUL(S0, S0, S2);
		MOV(_PC, _LR);
	}
	const u8* loadPairedS16Two = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRH(R12, R10);
		REV16(R12, R12);
		SXTH(R12, R12);
		VMOV(S0, R12);

		LDRH(R12, R10, 2);
		REV16(R12, R12);
		SXTH(R12, R12);
		VMOV(S1, R12);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		VCVT(S0, S0, TO_FLOAT | IS_SIGNED);
		VCVT(S1, S1, TO_FLOAT | IS_SIGNED);

		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		MOV(_PC, _LR);
	}
	const u8* loadPairedS16One = GetCodePtr();
	{
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		LDRH(R12, R10);

		MOVI2R(R10, (u32)&m_dequantizeTableS);
		ADD(R10, R10, R11);
		VLDR(S2, R10, 0);

		REV16(R12, R12);
		SXTH(R12, R12);
		VMOV(S0, R12);
		VCVT(S0, S0, TO_FLOAT | IS_SIGNED);

		VMUL(S0, S0, S2);
		MOV(_PC, _LR);
	}

	pairedLoadQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(16 * sizeof(u8*));

	pairedLoadQuantized[0] = loadPairedFloatTwo;
	pairedLoadQuantized[1] = loadPairedIllegal;
	pairedLoadQuantized[2] = loadPairedIllegal;
	pairedLoadQuantized[3] = loadPairedIllegal;
	pairedLoadQuantized[4] = loadPairedU8Two;
	pairedLoadQuantized[5] = loadPairedU16Two;
	pairedLoadQuantized[6] = loadPairedS8Two;
	pairedLoadQuantized[7] = loadPairedS16Two;

	pairedLoadQuantized[8] = loadPairedFloatOne;
	pairedLoadQuantized[9] = loadPairedIllegal;
	pairedLoadQuantized[10] = loadPairedIllegal;
	pairedLoadQuantized[11] = loadPairedIllegal;
	pairedLoadQuantized[12] = loadPairedU8One;
	pairedLoadQuantized[13] = loadPairedU16One;
	pairedLoadQuantized[14] = loadPairedS8One;
	pairedLoadQuantized[15] = loadPairedS16One;

	// Stores
	const u8* storePairedIllegal = GetCodePtr();
		BKPT(0x21);
	const u8* storePairedFloat = GetCodePtr();
	{
		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		nemit.VREV32(I_8, D0, D0);
		nemit.VST1(I_32, D0, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		VMOV(R1, S1);
		MOV(R2, R10);
		MOVI2R(R12, (u32)&WriteDual32);
		BL(R12);
		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storePairedU8 = GetCodePtr();
	{
		// R10 is the addr
		// R11 is the scale
		// R12 is scratch
		// S0, S1 is the values
		PUSH(5, R0, R1, R2, R3, _LR);

		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO);
		VCVT(S1, S1, TO_INT | ROUND_TO_ZERO);

		VMOV(R0, S0);
		VMOV(R1, S1);
		MOV(R2, R10);
		MOVI2R(R12, (u32)&WriteDual8);
		BL(R12);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storePairedS8 = GetCodePtr();
	{
		// R10 is the addr
		// R11 is the scale
		// R12 is scratch
		// S0, S1 is the values
		PUSH(5, R0, R1, R2, R3, _LR);

		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO | IS_SIGNED);
		VCVT(S1, S1, TO_INT | ROUND_TO_ZERO | IS_SIGNED);

		VMOV(R0, S0);
		VMOV(R1, S1);
		MOV(R2, R10);
		MOVI2R(R12, (u32)&WriteDual8);
		BL(R12);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storePairedU16 = GetCodePtr();
	{
		PUSH(5, R0, R1, R2, R3, _LR);

		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO);
		VCVT(S1, S1, TO_INT | ROUND_TO_ZERO);

		VMOV(R0, S0);
		VMOV(R1, S1);
		MOV(R2, R10);
		MOVI2R(R12, (u32)&WriteDual16);
		BL(R12);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storePairedS16 = GetCodePtr();
	{
		PUSH(5, R0, R1, R2, R3, _LR);

		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);
		VMUL(S1, S1, S2);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO | IS_SIGNED);
		VCVT(S1, S1, TO_INT | ROUND_TO_ZERO | IS_SIGNED);

		VMOV(R0, S0);
		VMOV(R1, S1);
		MOV(R2, R10);
		MOVI2R(R12, (u32)&WriteDual16);
		BL(R12);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storeSingleIllegal = GetCodePtr();
	BKPT(0x27);
	const u8* storeSingleFloat = GetCodePtr();
	{
		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		VMOV(R12, S0);
		REV(R12, R12);
		STR(R12, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		MOV(R1, R10);
		MOVI2R(R10, (u32)&PowerPC::Write_U32);
		BL(R10);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storeSingleU8 = GetCodePtr();  // Used by MKWii
	{
		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);

		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO);
		VMOV(R12, S0);
		STRB(R12, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		MOV(R1, R10);
		MOVI2R(R10, (u32)&PowerPC::Write_U8);
		BL(R10);
		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storeSingleS8 = GetCodePtr();
	{
		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);

		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO | IS_SIGNED);
		VMOV(R12, S0);
		STRB(R12, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		MOV(R1, R10);
		MOVI2R(R10, (u32)&Memory::Write_U8);
		BL(R10);
		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storeSingleU16 = GetCodePtr();  // Used by MKWii
	{
		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);

		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO);
		VMOV(R12, S0);
		REV16(R12, R12);
		STRH(R12, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		MOV(R1, R10);
		MOVI2R(R10, (u32)&PowerPC::Write_U16);
		BL(R10);

		POP(5, R0, R1, R2, R3, _PC);
	}
	const u8* storeSingleS16 = GetCodePtr();
	{
		MOVI2R(R12, (u32)&m_quantizeTableS);
		ADD(R12, R12, R11);
		VLDR(S2, R12, 0);
		VMUL(S0, S0, S2);

		TST(R10, arghmask);
		FixupBranch argh = B_CC(CC_NEQ);
		BIC(R10, R10, mask);
		ADD(R10, R10, R8);

		VCVT(S0, S0, TO_INT | ROUND_TO_ZERO | IS_SIGNED);
		VMOV(R12, S0);
		REV16(R12, R12);
		STRH(R12, R10);
		MOV(_PC, _LR);

		SetJumpTarget(argh);

		PUSH(5, R0, R1, R2, R3, _LR);
		VMOV(R0, S0);
		MOV(R1, R10);
		MOVI2R(R10, (u32)&Memory::Write_U16);
		BL(R10);

		POP(5, R0, R1, R2, R3, _PC);
	}

	pairedStoreQuantized = reinterpret_cast<const u8**>(const_cast<u8*>(AlignCode16()));
	ReserveCodeSpace(16 * sizeof(u8*));

	pairedStoreQuantized[0] = storePairedFloat;
	pairedStoreQuantized[1] = storePairedIllegal;
	pairedStoreQuantized[2] = storePairedIllegal;
	pairedStoreQuantized[3] = storePairedIllegal;
	pairedStoreQuantized[4] = storePairedU8;
	pairedStoreQuantized[5] = storePairedU16;
	pairedStoreQuantized[6] = storePairedS8;
	pairedStoreQuantized[7] = storePairedS16;

	pairedStoreQuantized[8] = storeSingleFloat;
	pairedStoreQuantized[9] = storeSingleIllegal;
	pairedStoreQuantized[10] = storeSingleIllegal;
	pairedStoreQuantized[11] = storeSingleIllegal;
	pairedStoreQuantized[12] = storeSingleU8;
	pairedStoreQuantized[13] = storeSingleU16;
	pairedStoreQuantized[14] = storeSingleS8;
	pairedStoreQuantized[15] = storeSingleS16;

	m_increment_profile_counter = AlignCode16();

	nemit.VLD1(I_64, D0, R0); // Start
	ADD(R0, R0, 8);
	nemit.VLD1(I_64, D1, R0); // End
	ADD(R0, R0, 8);
	nemit.VLD1(I_64, D2, R0); // Counter
	nemit.VSUB(I_64, D1, D1, D0);
	nemit.VADD(I_64, D2, D2, D1);
	nemit.VST1(I_64, D2, R0);
	MOV(_PC, _LR);
}
