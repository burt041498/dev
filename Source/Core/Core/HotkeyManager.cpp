// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/HotkeyManager.h"

const std::string hotkey_labels[] =
{
	(""), // Open
	(""), // Change Disc
	(""), // Refresh List

	_trans("Toggle Pause"),
	_trans("Stop"),
	_trans("Reset"),
	_trans("Frame Advance"),

	_trans("Start Recording"),
	_trans("Play Recording"),
	_trans("Export Recording"),
	_trans("Read-only mode"),

	_trans("Toggle Fullscreen"),
	_trans("Take Screenshot"),
	(""), // Exit

	_trans("Connect Wiimote 1"),
	_trans("Connect Wiimote 2"),
	_trans("Connect Wiimote 3"),
	_trans("Connect Wiimote 4"),
	_trans("Connect Balance Board"),

	_trans("Volume Down"),
	_trans("Volume Up"),
	_trans("Volume Toggle Mute"),

	_trans("Toggle IR"),
	_trans("Toggle Aspect Ratio"),
	_trans("Toggle EFB Copies"),
	_trans("Toggle Fog"),
	_trans("Toggle Frame limit"),
	_trans("Decrease Frame limit"),
	_trans("Increase Frame limit"),

	_trans("Freelook Decrease Speed"),
	_trans("Freelook Increase Speed"),
	_trans("Freelook Reset Speed"),
	_trans("Freelook Move Up"),
	_trans("Freelook Move Down"),
	_trans("Freelook Move Left"),
	_trans("Freelook Move Right"),
	_trans("Freelook Zoom In"),
	_trans("Freelook Zoom Out"),
	_trans("Freelook Reset"),

	_trans("Decrease Depth"),
	_trans("Increase Depth"),
	_trans("Decrease Convergence"),
	_trans("Increase Convergence"),

	_trans("Load State Slot 1"),
	_trans("Load State Slot 2"),
	_trans("Load State Slot 3"),
	_trans("Load State Slot 4"),
	_trans("Load State Slot 5"),
	_trans("Load State Slot 6"),
	_trans("Load State Slot 7"),
	_trans("Load State Slot 8"),
	_trans("Load State Slot 9"),
	_trans("Load State Slot 10"),

	_trans("Save State Slot 1"),
	_trans("Save State Slot 2"),
	_trans("Save State Slot 3"),
	_trans("Save State Slot 4"),
	_trans("Save State Slot 5"),
	_trans("Save State Slot 6"),
	_trans("Save State Slot 7"),
	_trans("Save State Slot 8"),
	_trans("Save State Slot 9"),
	_trans("Save State Slot 10"),

	_trans("Select State Slot 1"),
	_trans("Select State Slot 2"),
	_trans("Select State Slot 3"),
	_trans("Select State Slot 4"),
	_trans("Select State Slot 5"),
	_trans("Select State Slot 6"),
	_trans("Select State Slot 7"),
	_trans("Select State Slot 8"),
	_trans("Select State Slot 9"),
	_trans("Select State Slot 10"),

	_trans("Save to selected slot"),
	_trans("Load from selected slot"),

	_trans("Load State Last 1"),
	_trans("Load State Last 2"),
	_trans("Load State Last 3"),
	_trans("Load State Last 4"),
	_trans("Load State Last 5"),
	_trans("Load State Last 6"),
	_trans("Load State Last 7"),
	_trans("Load State Last 8"),

	_trans("Save Oldest State"),
	_trans("Undo Load State"),
	_trans("Undo Save State"),
	_trans("Save State"),
	_trans("Load State"),
};

const int num_hotkeys = (sizeof(hotkey_labels) / sizeof(hotkey_labels[0]));

namespace HotkeyManagerEmu
{

static u32 s_hotkeyDown[3];
static HotkeyStatus s_hotkey;
static bool s_enabled;

static InputConfig s_config("Hotkeys", _trans("Hotkeys"), "Hotkeys");

InputConfig* GetConfig()
{
	return &s_config;
}

void GetStatus()
{
	s_hotkey.err = PAD_ERR_NONE;

	// get input
	((HotkeyManager*)s_config.controllers[0])->GetInput(&s_hotkey);
}

bool IsEnabled()
{
	return s_enabled;
}

void Enable(bool enable_toggle)
{
	s_enabled = enable_toggle;
}

bool IsPressed(int Id, bool held)
{
	unsigned int set = Id / 32;
	unsigned int setKey = Id % 32;
	if (s_hotkey.button[set] & (1 << setKey))
	{
		s_hotkeyDown[set] |= (1 << setKey);
		if (held)
			return true;
	}
	else
	{
		bool pressed = !!(s_hotkeyDown[set] & (1 << setKey));
		s_hotkeyDown[set] &= ~(1 << setKey);
		if (pressed)
			return true;
	}

	return false;
}

void Initialize(void* const hwnd)
{
	if (s_config.controllers.empty())
		s_config.controllers.push_back(new HotkeyManager());

	g_controller_interface.Initialize(hwnd);

	// load the saved controller config
	s_config.LoadConfig(true);

	for (unsigned int i = 0; i < 3; ++i)
		s_hotkeyDown[i] = 0;

	s_enabled = true;
}

void LoadConfig()
{
	s_config.LoadConfig(true);
}

void Shutdown()
{
	std::vector<ControllerEmu*>::const_iterator
		i = s_config.controllers.begin(),
		e = s_config.controllers.end();
	for (; i != e; ++i)
		delete *i;
	s_config.controllers.clear();

	g_controller_interface.Shutdown();
}

}

HotkeyManager::HotkeyManager()
{
	for (int set = 0; set < 3; set++)
	{
		// buttons
		if ((set * 32) < num_hotkeys)
			groups.emplace_back(m_keys[set] = new Buttons(_trans("Keys")));

		for (int key = 0; key < 32; key++)
		{
			if ((set * 32 + key) < num_hotkeys && hotkey_labels[set * 32 + key].length() != 0)
			{
				m_keys[set]->controls.emplace_back(new ControlGroup::Input(hotkey_labels[set * 32 + key]));
			}
		}
	}

	groups.emplace_back(m_options = new ControlGroup(_trans("Options")));
	m_options->settings.emplace_back(new ControlGroup::BackgroundInputSetting(_trans("Background Input")));
	m_options->settings.emplace_back(new ControlGroup::IterateUI(_trans("Iterative Input")));
}

HotkeyManager::~HotkeyManager()
{
}

std::string HotkeyManager::GetName() const
{
	return std::string("Hotkeys") + char('1' + 0);
}

void HotkeyManager::GetInput(HotkeyStatus* const kb)
{
	for (int set = 0; set < 3; set++)
	{
		std::vector<u32> bitmasks;
		for (int key = 0; key < 32; key++)
		{
			if ((set * 32 + key) < num_hotkeys && hotkey_labels[set * 32 + key].length() != 0)
				bitmasks.push_back(1 << key);
		}

		if ((set * 32) < num_hotkeys)
		{
			kb->button[set] = 0;
			m_keys[set]->GetState(&kb->button[set], bitmasks.data());
		}
	}
}

void HotkeyManager::LoadDefaults(const ControllerInterface& ciface)
{
	for (int set = 0; set < 3; set++)
	{
		for (unsigned int key = 0; key < (m_keys[set])->controls.size(); key++)
		{
			(m_keys[set])->controls[key]->control_ref->expression = "";
		}
	}
}
