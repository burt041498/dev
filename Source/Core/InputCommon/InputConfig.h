// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Thread.h"

#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

class InputConfig
{
public:
	InputConfig(const char* const _ini_name, const char* const _gui_name,
		const char* const _profile_name)
		: ini_name(_ini_name), gui_name(_gui_name), profile_name(_profile_name) {}

	bool LoadConfig(bool isGC);
	void SaveConfig();

	std::vector<ControllerEmu*>  controllers;

	std::recursive_mutex controls_lock; // for changing any control references

	const char* const ini_name;
	const char* const gui_name;
	const char* const profile_name;
};
