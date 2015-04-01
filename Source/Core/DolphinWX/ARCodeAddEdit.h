// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

class wxSpinButton;
class wxSpinEvent;
class wxTextCtrl;
class wxWindow;

namespace ActionReplay { struct ARCode; }

class CARCodeAddEdit : public wxDialog
{
public:
	CARCodeAddEdit(int _selection, std::vector<ActionReplay::ARCode>* _arCodes,
		wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = _("Edit ActionReplay Code"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);

private:
	wxTextCtrl* EditCheatName;
	wxSpinButton* EntrySelection;
	wxTextCtrl* EditCheatCode;

	std::vector<ActionReplay::ARCode>* arCodes;

	void SaveCheatData(wxCommandEvent& event);
	void ChangeEntry(wxSpinEvent& event);
	void UpdateTextCtrl(ActionReplay::ARCode arCode);

	int selection;
};
