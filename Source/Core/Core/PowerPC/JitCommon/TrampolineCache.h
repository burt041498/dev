// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/x64Analyzer.h"
#include "Common/x64Emitter.h"

// We need at least this many bytes for backpatching.
const int BACKPATCH_SIZE = 5;

class TrampolineCache : public Gen::X64CodeBlock
{
public:
	void Init(int size);
	void Shutdown();

	const u8* GenerateReadTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u8* exceptionHandler, u8* returnPtr);
	const u8* GenerateWriteTrampoline(const InstructionInfo &info, BitSet32 registersInUse, u8* exceptionHandler, u8* returnPtr, u32 pc);
	void ClearCodeSpace();
};
