// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QSysInfo>
#include <QUrl>

#include "MainWindow.h"

#include "DolphinQt/Utils/Utils.h"
#include "UICommon/UICommon.h"

static bool IsOsSupported()
{
#ifdef Q_OS_OSX
	return QSysInfo::MacintoshVersion >= QSysInfo::MV_10_9;
#elif defined(Q_OS_WIN)
	return (QSysInfo::WindowsVersion & QSysInfo::WV_NT_based) >= QSysInfo::WV_VISTA;
#else
	return true;
#endif
}

static QString LowestSupportedOsVersion()
{
#ifdef Q_OS_OSX
	return SL("Mac OS X 10.9");
#elif defined(Q_OS_WIN)
	return SL("Windows Vista SP2");
#else
	return SL("Unknown");
#endif
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	app.setAttribute(Qt::AA_UseHighDpiPixmaps);
	// TODO: Add command line options

	UICommon::SetUserDirectory(""); // Auto-detect user folder
	UICommon::CreateDirectories();
	UICommon::Init();

	if (!IsOsSupported())
	{
		QMessageBox::critical(nullptr, QObject::tr("Unsupported OS"),
		                      QObject::tr("Dolphin requires %1 or greater.\n"
		                                  "Please upgrade to %1 or greater to use Dolphin.")
		                      .arg(LowestSupportedOsVersion()));
		return 1;
	}

	g_main_window = new DMainWindow();
	g_main_window->show();

	int retcode = app.exec();
	delete g_main_window;
	UICommon::Shutdown();
	return retcode;
}
