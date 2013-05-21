/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <wx/wx.h>
#include <wx/renderer.h>
#include <wx/grid.h>
#include <libdcp/types.h>
#include "lib/audio_mapping.h"
#include "audio_mapping_view.h"
#include "wx_util.h"

using std::cout;
using std::list;
using boost::shared_ptr;

/* This could go away with wxWidgets 2.9, which has an API call
   to find these values.
*/

#ifdef __WXMSW__
#define CHECKBOX_WIDTH 16
#define CHECKBOX_HEIGHT 16
#endif

#ifdef __WXGTK__
#define CHECKBOX_WIDTH 20
#define CHECKBOX_HEIGHT 20
#endif


class NoSelectionStringRenderer : public wxGridCellStringRenderer
{
public:
	void Draw (wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool)
	{
		wxGridCellStringRenderer::Draw (grid, attr, dc, rect, row, col, false);
	}
};

class CheckBoxRenderer : public wxGridCellRenderer
{
public:

	void Draw (wxGrid& grid, wxGridCellAttr &, wxDC& dc, const wxRect& rect, int row, int col, bool)
	{
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 9
		dc.SetPen (*wxThePenList->FindOrCreatePen (wxColour (255, 255, 255), 0, wxPENSTYLE_SOLID));
#else		
		dc.SetPen (*wxThePenList->FindOrCreatePen (wxColour (255, 255, 255), 0, wxSOLID));
#endif		
		dc.DrawRectangle (rect);
		
		wxRendererNative::Get().DrawCheckBox (
			&grid,
			dc, rect,
			grid.GetCellValue (row, col) == wxT("1") ? static_cast<int>(wxCONTROL_CHECKED) : 0
			);
	}

	wxSize GetBestSize (wxGrid &, wxGridCellAttr &, wxDC &, int, int)
	{
		return wxSize (CHECKBOX_WIDTH + 4, CHECKBOX_HEIGHT + 4);
	}
	
	wxGridCellRenderer* Clone () const
	{
		return new CheckBoxRenderer;
	}
};


AudioMappingView::AudioMappingView (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	_grid = new wxGrid (this, wxID_ANY);

	_grid->CreateGrid (0, 7);
#if wxMINOR_VERSION == 9
	_grid->HideRowLabels ();
#else
	_grid->SetRowLabelSize (0);
#endif	
	_grid->DisableDragRowSize ();
	_grid->DisableDragColSize ();
	_grid->EnableEditing (false);
	_grid->SetCellHighlightPenWidth (0);
	_grid->SetDefaultRenderer (new NoSelectionStringRenderer);

	_grid->SetColLabelValue (0, _("Content channel"));
	_grid->SetColLabelValue (1, _("L"));
	_grid->SetColLabelValue (2, _("R"));
	_grid->SetColLabelValue (3, _("C"));
	_grid->SetColLabelValue (4, _("Lfe"));
	_grid->SetColLabelValue (5, _("Ls"));
	_grid->SetColLabelValue (6, _("Rs"));

	_grid->AutoSize ();

	_sizer = new wxBoxSizer (wxVERTICAL);
	_sizer->Add (_grid, 1, wxEXPAND | wxALL);
	SetSizerAndFit (_sizer);

	Connect (wxID_ANY, wxEVT_GRID_CELL_LEFT_CLICK, wxGridEventHandler (AudioMappingView::left_click), 0, this);
}

void
AudioMappingView::left_click (wxGridEvent& ev)
{
	if (ev.GetCol() == 0) {
		return;
	}
	
	if (_grid->GetCellValue (ev.GetRow(), ev.GetCol()) == wxT("1")) {
		_grid->SetCellValue (ev.GetRow(), ev.GetCol(), wxT("0"));
	} else {
		_grid->SetCellValue (ev.GetRow(), ev.GetCol(), wxT("1"));
	}

	AudioMapping mapping;
	for (int i = 0; i < _grid->GetNumberRows(); ++i) {
		for (int j = 1; j < _grid->GetNumberCols(); ++j) {
			if (_grid->GetCellValue (i, j) == wxT ("1")) {
				mapping.add (i, static_cast<libdcp::Channel> (j - 1));
			}
		}
	}

	Changed (mapping);
}

void
AudioMappingView::set_mapping (AudioMapping map)
{
	if (_grid->GetNumberRows ()) {
		_grid->DeleteRows (0, _grid->GetNumberRows ());
	}

	list<int> content_channels = map.content_channels ();
	_grid->InsertRows (0, content_channels.size ());

	for (size_t r = 0; r < content_channels.size(); ++r) {
		for (int c = 1; c < 7; ++c) {
			_grid->SetCellRenderer (r, c, new CheckBoxRenderer);
		}
	}
	
	int n = 0;
	for (list<int>::iterator i = content_channels.begin(); i != content_channels.end(); ++i) {
		_grid->SetCellValue (n, 0, wxString::Format (wxT("%d"), *i + 1));

		list<libdcp::Channel> const d = map.content_to_dcp (*i);
		for (list<libdcp::Channel>::const_iterator j = d.begin(); j != d.end(); ++j) {
			_grid->SetCellValue (n, static_cast<int> (*j) + 1, wxT("1"));
		}
		++n;
	}
}

