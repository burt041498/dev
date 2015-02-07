// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "VideoBackends/OGL/GLInterfaceBase.h"

class cInterfaceLibretroGL : public cInterfaceBase
{
public:
	void SwapInterval(int Interval) override;
	void Swap() override;
	void* GetFuncAddress(const std::string& name) override;
	bool Create(void *window_handle);
	bool MakeCurrent() override;
	bool ClearCurrent();
	void Shutdown();
   void SetMode(u32 mode);
   void Update();
   bool PeekMessages();
};
