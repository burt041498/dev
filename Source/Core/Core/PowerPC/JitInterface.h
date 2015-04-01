// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/ChunkFile.h"
#include "Core/MachineContext.h"
#include "Core/PowerPC/CPUCoreBase.h"

namespace JitInterface
{
	enum class ExceptionType
	{
		EXCEPTIONS_FIFO_WRITE,
		EXCEPTIONS_PAIRED_QUANTIZE
	};

	void DoState(PointerWrap &p);

	CPUCoreBase *InitJitCore(int core);
	void InitTables(int core);
	CPUCoreBase *GetCore();

	// Debugging
	void WriteProfileResults(const std::string& filename);

	// Memory Utilities
	bool HandleFault(uintptr_t access_address, SContext* ctx);

	// Clearing CodeCache
	void ClearCache();

	void ClearSafe();

	// If "forced" is true, a recompile is being requested on code that hasn't been modified.
	void InvalidateICache(u32 address, u32 size, bool forced);

	void CompileExceptionCheck(ExceptionType type);

	void Shutdown();
}
extern bool bMMU;

