// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include "Common/PerformanceCounter.h"
#endif

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/JitInterface.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/Profiler.h"
#include "Core/PowerPC/JitCommon/JitBase.h"

#if _M_X86
#include "Core/PowerPC/Jit64/Jit.h"
#include "Core/PowerPC/Jit64/Jit64_Tables.h"
#include "Core/PowerPC/Jit64IL/JitIL.h"
#include "Core/PowerPC/Jit64IL/JitIL_Tables.h"
#endif

#if _M_ARM_32
#include "Core/PowerPC/JitArm32/Jit.h"
#include "Core/PowerPC/JitArm32/JitArm_Tables.h"
#endif

#if _M_ARM_64
#include "Core/PowerPC/JitArm64/Jit.h"
#include "Core/PowerPC/JitArm64/JitArm64_Tables.h"
#endif

static bool bFakeVMEM = false;
bool bMMU = false;

namespace JitInterface
{
	void DoState(PointerWrap &p)
	{
		if (jit && p.GetMode() == PointerWrap::MODE_READ)
			jit->GetBlockCache()->Clear();
	}
	CPUCoreBase *InitJitCore(int core)
	{
		bMMU = SConfig::GetInstance().m_LocalCoreStartupParameter.bMMU;
		bFakeVMEM = !bMMU;

		CPUCoreBase *ptr = nullptr;
		switch (core)
		{
		#if _M_X86
		case PowerPC::CORE_JIT64:
			ptr = new Jit64();
			break;
		case PowerPC::CORE_JITIL64:
			ptr = new JitIL();
			break;
		#endif
		#if _M_ARM_32
		case PowerPC::CORE_JITARM:
			ptr = new JitArm();
			break;
		#endif
		#if _M_ARM_64
		case PowerPC::CORE_JITARM64:
			ptr = new JitArm64();
			break;
		#endif
		default:
			PanicAlert("Unrecognizable cpu_core: %d", core);
			jit = nullptr;
			return nullptr;
		}
		jit = static_cast<JitBase*>(ptr);
		jit->Init();
		return ptr;
	}
	void InitTables(int core)
	{
		switch (core)
		{
		#if _M_X86
		case PowerPC::CORE_JIT64:
			Jit64Tables::InitTables();
			break;
		case PowerPC::CORE_JITIL64:
			JitILTables::InitTables();
			break;
		#endif
		#if _M_ARM_32
		case PowerPC::CORE_JITARM:
			JitArmTables::InitTables();
			break;
		#endif
		#if _M_ARM_64
		case PowerPC::CORE_JITARM64:
			JitArm64Tables::InitTables();
			break;
		#endif
		default:
			PanicAlert("Unrecognizable cpu_core: %d", core);
			break;
		}
	}
	CPUCoreBase *GetCore()
	{
		return jit;
	}

	void WriteProfileResults(const std::string& filename)
	{
		// Can't really do this with no jit core available
		if (!jit)
			return;

		std::vector<BlockStat> stats;
		stats.reserve(jit->GetBlockCache()->GetNumBlocks());
		u64 cost_sum = 0;
		u64 timecost_sum = 0;
		u64 countsPerSec;
		QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);
		for (int i = 0; i < jit->GetBlockCache()->GetNumBlocks(); i++)
		{
			const JitBlock *block = jit->GetBlockCache()->GetBlock(i);
			// Rough heuristic.  Mem instructions should cost more.
			u64 cost = block->originalSize * (block->runCount / 4);
			u64 timecost = block->ticCounter;
			// Todo: tweak.
			if (block->runCount >= 1)
				stats.emplace_back(i, cost);
			cost_sum += cost;
			timecost_sum += timecost;
		}

		sort(stats.begin(), stats.end());
		File::IOFile f(filename, "w");
		if (!f)
		{
			PanicAlert("Failed to open %s", filename.c_str());
			return;
		}
		fprintf(f.GetHandle(), "origAddr\tblkName\tcost\ttimeCost\tpercent\ttimePercent\tOvAllinBlkTime(ms)\tblkCodeSize\n");
		for (auto& stat : stats)
		{
			const JitBlock *block = jit->GetBlockCache()->GetBlock(stat.blockNum);
			if (block)
			{
				std::string name = g_symbolDB.GetDescription(block->originalAddress);
				double percent = 100.0 * (double)stat.cost / (double)cost_sum;
				double timePercent = 100.0 * (double)block->ticCounter / (double)timecost_sum;
				fprintf(f.GetHandle(), "%08x\t%s\t%" PRIu64 "\t%" PRIu64 "\t%.2f\t%.2f\t%.2f\t%i\n",
						block->originalAddress, name.c_str(), stat.cost,
						block->ticCounter, percent, timePercent,
						(double)block->ticCounter*1000.0/(double)countsPerSec, block->codeSize);
			}
		}
	}
	bool HandleFault(uintptr_t access_address, SContext* ctx)
	{
		return jit->HandleFault(access_address, ctx);
	}

	void ClearCache()
	{
		if (jit)
			jit->ClearCache();
	}
	void ClearSafe()
	{
		// This clear is "safe" in the sense that it's okay to run from
		// inside a JIT'ed block: it clears the instruction cache, but not
		// the JIT'ed code.
		// TODO: There's probably a better way to handle this situation.
		if (jit)
			jit->GetBlockCache()->Clear();
	}

	void InvalidateICache(u32 address, u32 size, bool forced)
	{
		if (jit)
			jit->GetBlockCache()->InvalidateICache(address, size, forced);
	}

	void CompileExceptionCheck(ExceptionType type)
	{
		if (!jit)
			return;

		std::unordered_set<u32>* exception_addresses = nullptr;

		switch (type)
		{
		case ExceptionType::EXCEPTIONS_FIFO_WRITE:
			exception_addresses = &jit->js.fifoWriteAddresses;
			break;
		case ExceptionType::EXCEPTIONS_PAIRED_QUANTIZE:
			exception_addresses = &jit->js.pairedQuantizeAddresses;
			break;
		}

		if (PC != 0 && (exception_addresses->find(PC)) == (exception_addresses->end()))
		{
			if (type == ExceptionType::EXCEPTIONS_FIFO_WRITE)
			{
				// Check in case the code has been replaced since: do we need to do this?
				int optype = GetOpInfo(PowerPC::HostRead_U32(PC))->type;
				if (optype != OPTYPE_STORE && optype != OPTYPE_STOREFP && (optype != OPTYPE_STOREPS))
					return;
			}
			exception_addresses->insert(PC);

			// Invalidate the JIT block so that it gets recompiled with the external exception check included.
			jit->GetBlockCache()->InvalidateICache(PC, 4, true);
		}
	}

	void Shutdown()
	{
		if (jit)
		{
			jit->Shutdown();
			delete jit;
			jit = nullptr;
		}
	}
}
