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
#include "lib/util.h"
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
#else
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
		dc.SetPen (*wxThePenList->FindOrCreatePen (wxColour (255, 255, 255), 0, wxPENSTYLE_SOLID));
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
	_grid->HideRowLabels ();
	_grid->DisableDragRowSize ();
	_grid->DisableDragColSize ();
	_grid->EnableEditing (false);
	_grid->SetCellHighlightPenWidth (0);
	_grid->SetDefaultRenderer (new NoSelectionStringRenderer);

	set_column_labels ();

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
		cout << "set " << ev.GetRow() << " " << ev.GetCol() << " to 0.\n";
		_grid->SetCellValue (ev.GetRow(), ev.GetCol(), wxT("0"));
	} else {
		cout << "set " << ev.GetRow() << " " << ev.GetCol() << " to 1.\n";
		_grid->SetCellValue (ev.GetRow(), ev.GetCol(), wxT("1"));
	}

	_map = AudioMapping (_map.content_channels ());
	cout << "was: " << _map.dcp_to_content(libdcp::CENTRE).size() << "\n";
	
	for (int i = 0; i < _grid->GetNumberRows(); ++i) {
		for (int j = 1; j < _grid->GetNumberCols(); ++j) {
			if (_grid->GetCellValue (i, j) == wxT ("1")) {
				_map.add (i, static_cast<libdcp::Channel> (j - 1));
			}
		}
	}

	cout << "changed: " << _map.dcp_to_content(libdcp::CENTRE).size() << "\n";
	Changed (_map);
}

void
AudioMappingView::set (AudioMapping map)
{
	_map = map;
	
	if (_grid->GetNumberRows ()) {
		_grid->DeleteRows (0, _grid->GetNumberRows ());
	}

	_grid->InsertRows (0, _map.content_channels ());

	for (int r = 0; r < _map.content_channels(); ++r) {
		for (int c = 1; c < 7; ++c) {
			_grid->SetCellRenderer (r, c, new CheckBoxRenderer);
		}
	}
	
	for (int i = 0; i < _map.content_channels(); ++i) {
		_grid->SetCellValue (i, 0, wxString::Format (wxT("%d"), i + 1));

		list<libdcp::Channel> const d = _map.content_to_dcp (i);
		for (list<libdcp::Channel>::const_iterator j = d.begin(); j != d.end(); ++j) {
			int const c = static_cast<int>(*j) + 1;
			if (c < _grid->GetNumberCols ()) {
				_grid->SetCellValue (i, c, wxT("1"));
			}
		}
	}
}

void
AudioMappingView::set_channels (int c)
{
	c++;

	if (c < _grid->GetNumberCols ()) {
		_grid->DeleteCols (c, _grid->GetNumberCols() - c);
	} else if (c > _grid->GetNumberCols ()) {
		_grid->InsertCols (_grid->GetNumberCols(), c - _grid->GetNumberCols());
		set_column_labels ();
	}

	set (_map);
}

void
AudioMappingView::set_column_labels ()
{
	int const c = _grid->GetNumberCols ();
	
	_grid->SetColLabelValue (0, _("Content channel"));
	
	if (c > 0) {
		_grid->SetColLabelValue (1, _("L"));
	}
	
	if (c > 1) {
		_grid->SetColLabelValue (2, _("R"));
	}
	
	if (c > 2) {
		_grid->SetColLabelValue (3, _("C"));
	}
	
	if (c > 3) {
		_grid->SetColLabelValue (4, _("Lfe"));
	}
	
	if (c > 4) {
		_grid->SetColLabelValue (5, _("Ls"));
	}
	
	if (c > 5) {
		_grid->SetColLabelValue (6, _("Rs"));
	}

	_grid->AutoSize ();
}
