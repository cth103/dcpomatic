/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/wx/audio_mapping_view.cc
 *  @brief AudioMappingView class and helpers.
 */

#include "lib/audio_mapping.h"
#include "lib/util.h"
#include "audio_mapping_view.h"
#include "wx_util.h"
#include "audio_gain_dialog.h"
#include <dcp/raw_convert.h>
#include <dcp/types.h>
#include <wx/wx.h>
#include <wx/renderer.h>
#include <wx/grid.h>
#include <wx/graphics.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::list;
using std::string;
using std::min;
using std::max;
using std::vector;
using std::pair;
using std::make_pair;
using boost::shared_ptr;
using dcp::raw_convert;

#define INDICATOR_SIZE 16
#define LEFT_WIDTH 48

enum {
	ID_off = 1,
	ID_full = 2,
	ID_minus6dB = 3,
	ID_edit = 4
};

class NoSelectionStringRenderer : public wxGridCellStringRenderer
{
public:
	void Draw (wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool)
	{
		wxGridCellStringRenderer::Draw (grid, attr, dc, rect, row, col, false);
	}
};

/** @class ValueRenderer
 *  @brief wxGridCellRenderer for a gain value.
 */
class ValueRenderer : public wxGridCellRenderer
{
public:

	void Draw (wxGrid& grid, wxGridCellAttr &, wxDC& dc, const wxRect& rect, int row, int col, bool)
	{
		dc.SetPen (*wxThePenList->FindOrCreatePen (wxColour (255, 255, 255), 1, wxPENSTYLE_SOLID));
		dc.SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (255, 255, 255), wxBRUSHSTYLE_SOLID));
		dc.DrawRectangle (rect);

		int const xo = (rect.GetWidth() - INDICATOR_SIZE) / 2;
		int const yo = (rect.GetHeight() - INDICATOR_SIZE) / 2;

		dc.SetPen (*wxThePenList->FindOrCreatePen (wxColour (0, 0, 0), 1, wxPENSTYLE_SOLID));
		dc.SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (255, 255, 255), wxBRUSHSTYLE_SOLID));
		dc.DrawRectangle (wxRect (rect.GetLeft() + xo, rect.GetTop() + yo, INDICATOR_SIZE, INDICATOR_SIZE));

		float const value = raw_convert<float> (wx_to_std (grid.GetCellValue (row, col)));
		float const value_dB = 20 * log10 (value);
		int const range = 18;
		int height = 0;
		if (value_dB > -range) {
			height = INDICATOR_SIZE * (1 + value_dB / range);
		}

		height = max (0, height);

		if (value > 0) {
			/* Make sure we get a little bit of the marker if there is any gain */
			height = max (3, height);
		}

		dc.SetBrush (*wxTheBrushList->FindOrCreateBrush (wxColour (0, 255, 0), wxBRUSHSTYLE_SOLID));
		dc.DrawRectangle (wxRect (rect.GetLeft() + xo, rect.GetTop() + yo + INDICATOR_SIZE - height, INDICATOR_SIZE, height));
	}

	wxSize GetBestSize (wxGrid &, wxGridCellAttr &, wxDC &, int, int)
	{
		return wxSize (INDICATOR_SIZE + 4, INDICATOR_SIZE + 4);
	}

	wxGridCellRenderer* Clone () const
	{
		return new ValueRenderer;
	}
};


AudioMappingView::AudioMappingView (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
	, _menu_row (0)
	, _menu_column (1)
	, _last_tooltip_row (0)
	, _last_tooltip_column (0)
{
	_left_labels = new wxPanel (this, wxID_ANY);
	_left_labels->Bind (wxEVT_PAINT, boost::bind (&AudioMappingView::paint_left_labels, this));
	_top_labels = new wxPanel (this, wxID_ANY);
	_top_labels->Bind (wxEVT_PAINT, boost::bind (&AudioMappingView::paint_top_labels, this));

	_grid = new wxGrid (this, wxID_ANY);

	_grid->CreateGrid (0, MAX_DCP_AUDIO_CHANNELS + 1);
	_grid->HideRowLabels ();
	_grid->DisableDragRowSize ();
	_grid->DisableDragColSize ();
	_grid->EnableEditing (false);
	_grid->SetCellHighlightPenWidth (0);
	_grid->SetDefaultRenderer (new NoSelectionStringRenderer);
	_grid->AutoSize ();

	wxSizer* vertical_sizer = new wxBoxSizer (wxVERTICAL);
	vertical_sizer->Add (_top_labels);
	wxSizer* horizontal_sizer = new wxBoxSizer (wxHORIZONTAL);
	horizontal_sizer->Add (_left_labels);
	horizontal_sizer->Add (_grid, 1, wxEXPAND | wxALL);
	vertical_sizer->Add (horizontal_sizer);
	SetSizerAndFit (vertical_sizer);

	Bind (wxEVT_GRID_CELL_LEFT_CLICK, boost::bind (&AudioMappingView::left_click, this, _1));
	Bind (wxEVT_GRID_CELL_RIGHT_CLICK, boost::bind (&AudioMappingView::right_click, this, _1));
	_grid->GetGridWindow()->Bind (wxEVT_MOTION, boost::bind (&AudioMappingView::mouse_moved_grid, this, _1));
	Bind (wxEVT_SIZE, boost::bind (&AudioMappingView::sized, this, _1));

	_menu = new wxMenu;
	_menu->Append (ID_off, _("Off"));
	_menu->Append (ID_full, _("Full"));
	_menu->Append (ID_minus6dB, _("-6dB"));
	_menu->Append (ID_edit, _("Edit..."));

	Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&AudioMappingView::off, this), ID_off);
	Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&AudioMappingView::full, this), ID_full);
	Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&AudioMappingView::minus6dB, this), ID_minus6dB);
	Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&AudioMappingView::edit, this), ID_edit);

	_left_labels->Bind (wxEVT_MOTION, bind (&AudioMappingView::mouse_moved_left_labels, this, _1));
}

/** Called when any gain value has changed */
void
AudioMappingView::map_values_changed ()
{
	update_cells ();
	Changed (_map);
	_last_tooltip_column = -1;
}

void
AudioMappingView::left_click (wxGridEvent& ev)
{
	if (ev.GetCol() == 0) {
		return;
	}

	int const d = ev.GetCol() - 1;

	if (_map.get (ev.GetRow(), d) > 0) {
		_map.set (ev.GetRow(), d, 0);
	} else {
		_map.set (ev.GetRow(), d, 1);
	}

	map_values_changed ();
}

void
AudioMappingView::right_click (wxGridEvent& ev)
{
	if (ev.GetCol() == 0) {
		return;
	}

	_menu_row = ev.GetRow ();
	_menu_column = ev.GetCol ();
	PopupMenu (_menu, ev.GetPosition ());
}

void
AudioMappingView::off ()
{
	_map.set (_menu_row, _menu_column - 1, 0);
	map_values_changed ();
}

void
AudioMappingView::full ()
{
	_map.set (_menu_row, _menu_column - 1, 1);
	map_values_changed ();
}

void
AudioMappingView::minus6dB ()
{
	_map.set (_menu_row, _menu_column - 1, pow (10, -6.0 / 20));
	map_values_changed ();
}

void
AudioMappingView::edit ()
{
	int const d = _menu_column - 1;

	AudioGainDialog* dialog = new AudioGainDialog (this, _menu_row, _menu_column - 1, _map.get (_menu_row, d));
	if (dialog->ShowModal () == wxID_OK) {
		_map.set (_menu_row, d, dialog->value ());
		map_values_changed ();
	}

	dialog->Destroy ();
}

void
AudioMappingView::set (AudioMapping map)
{
	_map = map;
	update_cells ();
}

void
AudioMappingView::set_input_channels (vector<string> const & names)
{
	for (int i = 0; i < _grid->GetNumberRows(); ++i) {
		_grid->SetCellValue (i, 0, std_to_wx (names[i]));
	}
}

void
AudioMappingView::set_output_channels (vector<string> const & names)
{
	int const o = names.size() + 1;
	if (o < _grid->GetNumberCols ()) {
		_grid->DeleteCols (o, _grid->GetNumberCols() - o);
	} else if (o > _grid->GetNumberCols ()) {
		_grid->InsertCols (_grid->GetNumberCols(), o - _grid->GetNumberCols());
	}

	_grid->SetColLabelValue (0, wxT (""));

	for (size_t i = 0; i < names.size(); ++i) {
		_grid->SetColLabelValue (i + 1, std_to_wx (names[i]));
	}

	update_cells ();
	setup_sizes ();
}

void
AudioMappingView::update_cells ()
{
	vector<string> row_names;
	for (int i = 0; i < _grid->GetNumberRows (); ++i) {
		row_names.push_back (wx_to_std (_grid->GetCellValue (i, 0)));
	}

	if (_grid->GetNumberRows ()) {
		_grid->DeleteRows (0, _grid->GetNumberRows ());
	}

	_grid->InsertRows (0, _map.input_channels ());

	for (int i = 0; i < _map.input_channels(); ++i) {
		for (int j = 0; j < _map.output_channels(); ++j) {
			_grid->SetCellRenderer (i, j + 1, new ValueRenderer);
		}
	}

	for (int i = 0; i < _map.input_channels(); ++i) {
		if (i < int (row_names.size ())) {
			_grid->SetCellValue (i, 0, std_to_wx (row_names[i]));
		}
		for (int j = 1; j < _grid->GetNumberCols(); ++j) {
			_grid->SetCellValue (i, j, std_to_wx (raw_convert<string> (_map.get (i, j - 1))));
		}
	}

	_grid->AutoSize ();
}

void
AudioMappingView::mouse_moved_grid (wxMouseEvent& ev)
{
	int xx;
	int yy;
	_grid->CalcUnscrolledPosition (ev.GetX(), ev.GetY(), &xx, &yy);

	int const row = _grid->YToRow (yy);
	int const column = _grid->XToCol (xx);

	if (row < 0 || column < 1) {
		_grid->GetGridWindow()->SetToolTip ("");
		_last_tooltip_row = row;
		_last_tooltip_column = column;
	}

	if (row != _last_tooltip_row || column != _last_tooltip_column) {

		wxString s;
		float const gain = _map.get (row, column - 1);
		if (gain == 0) {
			s = wxString::Format (_("No audio will be passed from content channel %d to DCP channel %d."), row + 1, column);
		} else if (gain == 1) {
			s = wxString::Format (_("Audio will be passed from content channel %d to DCP channel %d unaltered."), row + 1, column);
		} else {
			float const dB = 20 * log10 (gain);
			s = wxString::Format (_("Audio will be passed from content channel %d to DCP channel %d with gain %.1fdB."), row + 1, column, dB);
		}

		_grid->GetGridWindow()->SetToolTip (s + " " + _("Right click to change gain."));
		_last_tooltip_row = row;
		_last_tooltip_column = column;
	}

        ev.Skip ();
}

void
AudioMappingView::sized (wxSizeEvent& ev)
{
	setup_sizes ();
	ev.Skip ();
}

void
AudioMappingView::setup_sizes ()
{
	int const top_height = 24;

	_grid->AutoSize ();
	_left_labels->SetMinSize (wxSize (LEFT_WIDTH, _grid->GetSize().GetHeight()));
	_top_labels->SetMinSize (wxSize (_grid->GetSize().GetWidth() + LEFT_WIDTH, top_height));
	/* Try to make the _top_labels 'actua' size respect the minimum we just set */
	_top_labels->Fit ();
	_left_labels->Refresh ();
	_top_labels->Refresh ();
}

void
AudioMappingView::paint_left_labels ()
{
	wxPaintDC dc (_left_labels);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	wxSize const size = dc.GetSize();
	int const half = size.GetWidth() / 2;

	gc->SetPen (wxPen (wxColour (0, 0, 0)));
	gc->SetAntialiasMode (wxANTIALIAS_DEFAULT);

	wxGraphicsPath lines = gc->CreatePath();

	vector<pair<int, int> >::const_iterator i = _input_group_positions.begin();
	if (i != _input_group_positions.end()) {
		lines.MoveToPoint (half, i->first);
		lines.AddLineToPoint (size.GetWidth(), i->first);
	}

	vector<Group>::const_iterator j = _input_groups.begin();
	while (i != _input_group_positions.end() && j != _input_groups.end()) {

		dc.SetClippingRegion (0, i->first + 2, size.GetWidth(), i->second - 4);

		dc.SetFont (*wxSWISS_FONT);
		wxCoord label_width;
		wxCoord label_height;
		dc.GetTextExtent (std_to_wx (j->name), &label_width, &label_height);

		dc.DrawRotatedText (
			j->name,
			half + (half - label_height) / 2,
			min (i->second, (i->second + i->first + label_width) / 2),
			90
			);

		dc.DestroyClippingRegion ();

		lines.MoveToPoint (half, i->second);
		lines.AddLineToPoint (size.GetWidth(), i->second);

		gc->StrokePath (lines);

		++i;
		++j;
	}

	/* Overall label */
	dc.SetFont (wxSWISS_FONT->Bold());
	wxCoord overall_label_width;
	wxCoord overall_label_height;
	dc.GetTextExtent (_("Content"), &overall_label_width, &overall_label_height);
	dc.DrawRotatedText (
		_("Content"),
		(half - overall_label_height) / 2,
		min (size.GetHeight(), (size.GetHeight() + _grid->GetColLabelSize() + overall_label_width) / 2),
		90
		);

	delete gc;
}

void
AudioMappingView::paint_top_labels ()
{
	wxPaintDC dc (_top_labels);
	if (_grid->GetNumberCols() == 0) {
		return;
	}

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	wxSize const size = dc.GetSize();

	gc->SetAntialiasMode (wxANTIALIAS_DEFAULT);

	dc.SetFont (wxSWISS_FONT->Bold());
	wxCoord label_width;
	wxCoord label_height;
	dc.GetTextExtent (_("DCP"), &label_width, &label_height);

	dc.DrawText (_("DCP"), (size.GetWidth() + _grid->GetColSize(0) + LEFT_WIDTH - label_width) / 2, (size.GetHeight() - label_height) / 2);

	gc->SetPen (wxPen (wxColour (0, 0, 0)));
	wxGraphicsPath lines = gc->CreatePath();
	lines.MoveToPoint (LEFT_WIDTH + _grid->GetColSize(0) - 1, 0);
	lines.AddLineToPoint (LEFT_WIDTH + _grid->GetColSize(0) - 1, size.GetHeight());
	lines.MoveToPoint (size.GetWidth() - 1, 0);
	lines.AddLineToPoint (size.GetWidth() - 1, size.GetHeight());
	gc->StrokePath (lines);

	delete gc;
}

void
AudioMappingView::set_input_groups (vector<Group> const & groups)
{
	_input_groups = groups;
	_input_group_positions.clear ();

	int ypos = _grid->GetColLabelSize() - 1;
	BOOST_FOREACH (Group const & i, _input_groups) {
		int const old_ypos = ypos;
		ypos += (i.to - i.from + 1) * _grid->GetRowSize(0);
		_input_group_positions.push_back (make_pair (old_ypos, ypos));
	}
}

void
AudioMappingView::mouse_moved_left_labels (wxMouseEvent& event)
{
	bool done = false;
	for (size_t i = 0; i < _input_group_positions.size(); ++i) {
		if (_input_group_positions[i].first <= event.GetY() && event.GetY() < _input_group_positions[i].second) {
			_left_labels->SetToolTip (_input_groups[i].name);
			done = true;
		}
	}

	if (!done) {
		_left_labels->SetToolTip ("");
	}
}
