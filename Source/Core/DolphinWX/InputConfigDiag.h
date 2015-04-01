// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define SLIDER_TICK_COUNT    100
#define DETECT_WAIT_TIME     2500
#define PREVIEW_UPDATE_TIME  25
#define DEFAULT_HIGH_VALUE   100

// might have to change this setup for Wiimote
#define PROFILES_PATH       "Profiles/"

#include <cstddef>
#include <string>
#include <vector>
#include <wx/button.h>
#include <wx/control.h>
#include <wx/dialog.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/translation.h>

#include "InputCommon/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Device.h"

class InputConfig;
class wxComboBox;
class wxCommandEvent;
class wxEvent;
class wxListBox;
class wxNotebook;
class wxSlider;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;
class wxTimerEvent;
class wxWindow;

class PadSetting
{
protected:
	PadSetting(wxControl* const _control) : wxcontrol(_control) { wxcontrol->SetClientData(this); }

public:
	virtual void UpdateGUI() = 0;
	virtual void UpdateValue() = 0;

	virtual ~PadSetting() {}

	wxControl* const wxcontrol;
};

class PadSettingExtension : public PadSetting
{
public:
	PadSettingExtension(wxWindow* const parent, ControllerEmu::Extension* const ext);
	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::Extension* const extension;
};

class PadSettingSpin : public PadSetting
{
public:
	PadSettingSpin(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const _setting)
		: PadSetting(new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition,
					    wxSize(54, -1), 0, _setting->low, _setting->high, (int)(_setting->value * 100)))
		, setting(_setting) {}

	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::ControlGroup::Setting* const setting;
};

class PadSettingCheckBox : public PadSetting
{
public:
	PadSettingCheckBox(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const setting);
	void UpdateGUI() override;
	void UpdateValue() override;

	ControllerEmu::ControlGroup::Setting* const setting;
};

class GamepadPage;

class ControlDialog : public wxDialog
{
public:
	ControlDialog(GamepadPage* const parent, InputConfig& config, ControllerInterface::ControlReference* const ref);

	wxStaticBoxSizer* CreateControlChooser(GamepadPage* const parent);

	virtual bool Validate() override;

	void DetectControl(wxCommandEvent& event);
	void ClearControl(wxCommandEvent& event);
	void SetDevice(wxCommandEvent& event);

	void UpdateGUI();
	void UpdateListContents();
	void SelectControl(const std::string& name);

	void SetSelectedControl(wxCommandEvent& event);
	void AppendControl(wxCommandEvent& event);

	ControllerInterface::ControlReference* const control_reference;
	InputConfig& m_config;
	wxComboBox*  device_cbox;

	wxTextCtrl* textctrl;
	wxListBox*  control_lbox;
	wxSlider*   range_slider;

private:
	GamepadPage* const m_parent;
	wxStaticText*      m_bound_label;
	wxStaticText*      m_error_label;
	ciface::Core::DeviceQualifier    m_devq;
	bool GetExpressionForSelectedControl(wxString &expr);
};

class ExtensionButton : public wxButton
{
public:
	ExtensionButton(wxWindow* const parent, ControllerEmu::Extension* const ext)
		: wxButton(parent, wxID_ANY, _("Configure"), wxDefaultPosition)
		, extension(ext) {}

	ControllerEmu::Extension* const extension;
};

class ControlButton : public wxButton
{
public:
	ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label = "");

	ControllerInterface::ControlReference* const control_reference;
};

class ControlGroupBox : public wxBoxSizer
{
public:
	ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent, GamepadPage* const eventsink);
	~ControlGroupBox();

	std::vector<PadSetting*> options;

	ControllerEmu::ControlGroup* const control_group;
	wxStaticBitmap*                    static_bitmap;
	std::vector<ControlButton*>        control_buttons;
};

class ControlGroupsSizer : public wxBoxSizer
{
public:
	ControlGroupsSizer(ControllerEmu* const controller, wxWindow* const parent, GamepadPage* const eventsink, std::vector<ControlGroupBox*>* const groups = nullptr);
};

class InputConfigDialog;

class GamepadPage : public wxPanel
{
	friend class InputConfigDialog;
	friend class ControlDialog;

public:
	GamepadPage(wxWindow* parent, InputConfig& config, const unsigned int pad_num, InputConfigDialog* const config_dialog);

	void UpdateGUI();

	void RefreshDevices(wxCommandEvent& event);

	void LoadProfile(wxCommandEvent& event);
	void SaveProfile(wxCommandEvent& event);
	void DeleteProfile(wxCommandEvent& event);

	void ConfigControl(wxEvent& event);
	void ClearControl(wxEvent& event);
	void DetectControl(wxCommandEvent& event);

	void ConfigExtension(wxCommandEvent& event);

	void SetDevice(wxCommandEvent& event);

	void ClearAll(wxCommandEvent& event);
	void LoadDefaults(wxCommandEvent& event);

	void AdjustControlOption(wxCommandEvent& event);
	void AdjustSetting(wxCommandEvent& event);
	void AdjustSettingUI(wxCommandEvent& event);

	void GetProfilePath(std::string& path);

	wxComboBox* profile_cbox;
	wxComboBox* device_cbox;

	std::vector<ControlGroupBox*> control_groups;
	std::vector<ControlButton*>   control_buttons;

protected:

	ControllerEmu* const controller;

private:

	ControlDialog*           m_control_dialog;
	InputConfigDialog* const m_config_dialog;
	InputConfig& m_config;
	bool DetectButton(ControlButton* button);
	bool m_iterate = false;
};

class InputConfigDialog : public wxDialog
{
public:
	InputConfigDialog(wxWindow* const parent, InputConfig& config, const wxString& name, const int tab_num = 0);

	void ClickSave(wxCommandEvent& event);

	void UpdateDeviceComboBox();
	void UpdateProfileComboBox();

	void UpdateControlReferences();
	void UpdateBitmaps(wxTimerEvent&);

private:

	wxNotebook*               m_pad_notebook;
	std::vector<GamepadPage*> m_padpages;
	InputConfig&              m_config;
	wxTimer                   m_update_timer;
};
