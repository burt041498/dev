// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <wx/defs.h>
#include <wx/grid.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"

class wxWindow;

class CDSPRegTable : public wxGridTableBase
{
private:
	u64 m_CachedCounter;
	u16 m_CachedRegs[32];
	bool m_CachedRegHasChanged[32];

	DECLARE_NO_COPY_CLASS(CDSPRegTable);

public:
	CDSPRegTable()
	{
		memset(m_CachedRegs, 0, sizeof(m_CachedRegs));
		memset(m_CachedRegHasChanged, 0, sizeof(m_CachedRegHasChanged));
	}

	int GetNumberCols() override { return 2; }
	int GetNumberRows() override { return 32; }
	bool IsEmptyCell(int row, int col) override { return false; }
	wxString GetValue(int row, int col) override;
	void SetValue(int row, int col, const wxString &) override;
	wxGridCellAttr *GetAttr(int, int, wxGridCellAttr::wxAttrKind) override;
	void UpdateCachedRegs();
};

class DSPRegisterView : public wxGrid
{
public:
	DSPRegisterView(wxWindow* parent, wxWindowID id = wxID_ANY);
	void Update() override;

private:
	// Owned by wx. Deleted implicitly upon destruction.
	CDSPRegTable* m_register_table;
};
