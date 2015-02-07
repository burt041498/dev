#include "libretro.h"

#include <string>
#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/State.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/BannerLoader.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeCreator.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"

#ifdef _WIN32
// for MAX_PATH
#include <windows.h>
#endif

#if defined(MAX_PATH) && !defined(PATH_MAX)
#define PATH_MAX MAX_PATH
#endif

#define SAMPLERATE 44100

static Common::Event updateMainFrameEvent;

struct retro_hw_render_callback hw_render;
static retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_environment_t environ_cb;

extern "C"
{
retro_hw_get_proc_address_t libretro_get_proc_address;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   (void)cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

std::string retro_base_dir;
bool retro_base_dir_found;

void retro_init(void)
{
   const char *dir_ptr = NULL;
   struct retro_log_callback log;

   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   retro_base_dir_found = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir_ptr) && dir_ptr)
   {
      retro_base_dir = dir_ptr;
      // Make sure that we don't have any lingering slashes, etc, as they break Windows.
      size_t last = retro_base_dir.find_last_not_of("/\\");
      if (last != std::string::npos)
         last++;

      retro_base_dir = retro_base_dir.substr(0, last);
      retro_base_dir_found = true;
   }

	UICommon::Init();
}

void retro_deinit(void)
{
   Core::Shutdown();
   UICommon::Shutdown();
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "Dolphin";
   info->library_version  = "v4.0-git";
   info->need_fullpath    = true;
   info->valid_extensions = "elf|iso|cso|prx|pbp";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = 60.0f / 1.001f;
   info->timing.sample_rate = SAMPLERATE;

   info->geometry.base_width = 640;
   info->geometry.base_height = 480;
   info->geometry.max_width = 640;
   info->geometry.max_height = 480;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   char *base;
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
   {
      buf[0] = '.';
      buf[1] = '\0';
   }
}

static void context_reset(void)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "Context reset!\n");
}

static void check_variables(void)
{
   struct retro_variable var;

   (void)var;
#if 0
   var.key = "ppsspp_internal_resolution";
   var.value = NULL;
#endif
}


bool retro_load_game(const struct retro_game_info *game)
{
   const char *tmp = NULL;
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "Cross" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "Circle" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,     "Triangle" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "Square" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,    "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,    "Start" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Analog X" },
      { 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Analog Y" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!retro_base_dir_found)
   {
      char _dir[PATH_MAX];
      extract_directory(_dir, game->path, sizeof(_dir));
      retro_base_dir = std::string(_dir);
   }

   retro_base_dir += slash;
   retro_base_dir += "dolphin";
   retro_base_dir += slash;

   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_ERROR, "XRGB8888 is not supported.\n");
      return false;
   }

#ifdef USING_GLES2
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#else
   hw_render.context_type = RETRO_HW_CONTEXT_OPENGL;
#endif
   hw_render.context_reset = context_reset;
   hw_render.bottom_left_origin = true;
   hw_render.depth = true;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   libretro_get_proc_address = hw_render.get_proc_address;

   check_variables();

	if (!BootManager::BootCore(game->path))
      return false;

	PowerPC::Start();
   Core::SetState(Core::CORE_RUN);

   return true;
}

static bool should_reset = false;

void retro_reset(void)
{
   should_reset = true;
}

static bool running = false;

static inline void rarch_sleep(unsigned msec)
{
#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
   sys_timer_usleep(1000 * msec);
#elif defined(PSP)
   sceKernelDelayThread(1000 * msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
   usleep(1000 * msec);
#else
   struct timespec tv = {0};
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}

void retro_run(void)
{
   bool updated = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	   check_variables();
  
   if (input_poll_cb)
      input_poll_cb();

   if (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
      updateMainFrameEvent.Wait();
}

void retro_unload_game(void)
{
   Core::SetState(Core::CORE_PAUSE);
   Core::Stop();
   updateMainFrameEvent.Set(); // Kick the waiting event
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   // TODO: what is a sane maximum size?
   return 96 * 1024 * 1024;
}

bool retro_serialize(void *data, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}
