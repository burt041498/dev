// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "VideoBackends/OGL/GLInterface/LibretroGL.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

#include "libretro.h"

struct retro_hw_render_callback hw_render;
extern retro_video_refresh_t video_cb;
extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;
retro_hw_get_proc_address_t libretro_get_proc_address;

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

static void context_reset(void)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Context reset!\n");
}

// Create rendering window.
// Call browser: Core.cpp:EmuThread() > main.cpp:Video_Initialize()
bool cInterfaceLibretroGL::Create(void *window_handle)
{
#ifdef USING_GLES3
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES3;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
   hw_render.context_reset = context_reset;
   hw_render.bottom_left_origin = true;
   hw_render.depth = true;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   libretro_get_proc_address = hw_render.get_proc_address;

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
