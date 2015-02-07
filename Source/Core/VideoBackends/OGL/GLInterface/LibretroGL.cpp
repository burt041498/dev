// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "VideoBackends/OGL/GLInterface/LibretroGL.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

void cInterfaceLibretroGL::SwapInterval(int Interval)
{
}

void* cInterfaceLibretroGL::GetFuncAddress(const std::string& name)
{
   /* call proc address function here. */
}

void cInterfaceLibretroGL::Swap()
{
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceLibretroGL::Create(void *window_handle)
{
   /* only set framebuffer here. */
	return true;
}

bool cInterfaceLibretroGL::MakeCurrent()
{
   return true;
}

bool cInterfaceLibretroGL::ClearCurrent()
{
   return true;
}


// Close backend
void cInterfaceLibretroGL::Shutdown()
{
}

void cInterfaceLibretroGL::SetMode(u32 mode) {  }

void cInterfaceLibretroGL::Update() { }

bool cInterfaceLibretroGL::PeekMessages() { return false; }
