// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>

#include "Common/Logging/ConsoleListener.h"

ConsoleListener::ConsoleListener()
{
	m_use_color = !!isatty(fileno(stdout));
}

ConsoleListener::~ConsoleListener()
{
	fflush(nullptr);
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char *text)
{
	char color_attr[16] = "";
	char reset_attr[16] = "";

	if (m_use_color)
	{
		strcpy(reset_attr, "\x1b[0m");
		switch (level)
		{
		case LogTypes::LOG_LEVELS::LNOTICE:
			// light green
			strcpy(color_attr, "\x1b[92m");
			break;
		case LogTypes::LOG_LEVELS::LERROR:
			// light red
			strcpy(color_attr, "\x1b[91m");
			break;
		case LogTypes::LOG_LEVELS::LWARNING:
			// light yellow
			strcpy(color_attr, "\x1b[93m");
			break;
		default:
			break;
		}
	}
	fprintf(stderr, "%s%s%s", color_attr, text, reset_attr);
}
