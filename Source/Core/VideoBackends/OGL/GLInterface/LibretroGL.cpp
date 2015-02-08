// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "VideoBackends/OGL/GLInterface/LibretroGL.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

#include "libretro.h"

extern struct retro_hw_render_callback hw_render;
extern retro_video_refresh_t video_cb;

void cInterfaceLibretroGL::SwapInterval(int Interval)
{
}

void* cInterfaceLibretroGL::GetFuncAddress(const std::string& name)
{
   return (void*)hw_render.get_proc_address(name.c_str());
}

void cInterfaceLibretroGL::Swap()
{
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, 640, 480, 0);
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceLibretroGL::Create(void *window_handle)
{
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
