/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "audio_mapping_view.h"
#include "wx_util.h"
#include "audio_gain_dialog.h"
#include "lib/audio_mapping.h"
#include "lib/util.h"
#include <dcp/locale_convert.h>
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
using boost::optional;
using dcp::locale_convert;

#define INDICATOR_SIZE 20
#define GRID_SPACING 32
#define LEFT_WIDTH (GRID_SPACING * 3)
#define TOP_HEIGHT (GRID_SPACING * 2)

enum {
	ID_off = 1,
	ID_full = 2,
	ID_minus6dB = 3,
	ID_edit = 4
};

AudioMappingView::AudioMappingView (wxWindow* parent)
	: wxScrolledWindow (parent, wxID_ANY)
	, _menu_input (0)
	, _menu_output (1)
{
	_menu = new wxMenu;
	_menu->Append (ID_off, _("Off"));
	_menu->Append (ID_full, _("Full"));
	_menu->Append (ID_minus6dB, _("-6dB"));
	_menu->Append (ID_edit, _("Edit..."));

	Bind (wxEVT_PAINT, boost::bind(&AudioMappingView::paint, this));
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::off, this), ID_off);
	Bind (wxEVT_LEFT_DOWN, boost::bind(&AudioMappingView::left_down, this, _1));
	Bind (wxEVT_RIGHT_DOWN, boost::bind(&AudioMappingView::right_down, this, _1));
	Bind (wxEVT_MOTION, boost::bind(&AudioMappingView::motion, this, _1));
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::full, this), ID_full);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::minus6dB, this), ID_minus6dB);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::edit, this), ID_edit);

	SetScrollRate (GRID_SPACING, GRID_SPACING);
}

void
AudioMappingView::paint ()
{
	wxPaintDC dc (this);

	wxGraphicsContext* gc = wxGraphicsContext::Create (dc);
	if (!gc) {
		return;
	}

	int sx, sy;
	GetViewStart (&sx, &sy);
	gc->Translate (-sx * GRID_SPACING, -sy * GRID_SPACING);
	dc.SetLogicalOrigin (sx * GRID_SPACING, sy * GRID_SPACING);

	int const output_channels_width = _output_channels.size() * GRID_SPACING;
	int const input_channels_height = _input_channels.size() * GRID_SPACING;

	gc->SetAntialiasMode (wxANTIALIAS_DEFAULT);
	wxGraphicsPath lines = gc->CreatePath ();
	dc.SetFont (wxSWISS_FONT->Bold());
	wxCoord label_width;
	wxCoord label_height;

	/* DCP label at the top */

	dc.GetTextExtent (_("DCP"), &label_width, &label_height);
	dc.DrawText (_("DCP"), LEFT_WIDTH + (output_channels_width - label_width) / 2, (GRID_SPACING - label_height) / 2);

	/* Content label on the left */

	dc.GetTextExtent (_("Content"), &label_width, &label_height);
	dc.DrawRotatedText (
		_("Content"),
		(GRID_SPACING - label_height) / 2,
		TOP_HEIGHT + (input_channels_height + label_width) / 2,
		90
		);

	dc.SetFont (*wxSWISS_FONT);
	gc->SetPen (*wxBLACK_PEN);

	/* Column labels and some lines */

	int N = 0;
	BOOST_FOREACH (string i, _output_channels) {
		dc.GetTextExtent (std_to_wx(i), &label_width, &label_height);
		dc.DrawText (std_to_wx(i), LEFT_WIDTH + GRID_SPACING * N + (GRID_SPACING - label_width) / 2, GRID_SPACING + (GRID_SPACING - label_height) / 2);
		lines.MoveToPoint    (LEFT_WIDTH + GRID_SPACING * N, GRID_SPACING);
		lines.AddLineToPoint (LEFT_WIDTH + GRID_SPACING * N, TOP_HEIGHT + _input_channels.size() * GRID_SPACING);
		++N;
	}
	lines.MoveToPoint    (LEFT_WIDTH + GRID_SPACING * N, GRID_SPACING);
	lines.AddLineToPoint (LEFT_WIDTH + GRID_SPACING * N, TOP_HEIGHT + _input_channels.size() * GRID_SPACING);

	/* Horizontal lines at the top */

	lines.MoveToPoint (LEFT_WIDTH, GRID_SPACING);
		lines.AddLineToPoint (LEFT_WIDTH + output_channels_width, GRID_SPACING);
	lines.MoveToPoint (LEFT_WIDTH, GRID_SPACING * 2);
		lines.AddLineToPoint (LEFT_WIDTH + output_channels_width, GRID_SPACING * 2);

	/* Row channel labels */

	N = 0;
	BOOST_FOREACH (string i, _input_channels) {
		dc.GetTextExtent (std_to_wx(i), &label_width, &label_height);
		dc.DrawText (std_to_wx(i), GRID_SPACING * 2 + (GRID_SPACING - label_width) / 2, TOP_HEIGHT + GRID_SPACING * N + (GRID_SPACING - label_height) / 2);
		lines.MoveToPoint (GRID_SPACING * 2, TOP_HEIGHT + GRID_SPACING * N);
		lines.AddLineToPoint (LEFT_WIDTH + output_channels_width, TOP_HEIGHT + GRID_SPACING * N);
		++N;
	}
	lines.MoveToPoint (GRID_SPACING * 2, TOP_HEIGHT + GRID_SPACING * N);
	lines.AddLineToPoint (LEFT_WIDTH + output_channels_width, TOP_HEIGHT + GRID_SPACING * N);

	/* Vertical lines on the left */

	for (int i = 1; i < 3; ++i) {
		lines.MoveToPoint    (GRID_SPACING * i, TOP_HEIGHT);
		lines.AddLineToPoint (GRID_SPACING * i, TOP_HEIGHT + _input_channels.size() * GRID_SPACING);
	}

	/* Group labels and lines */

	int y = TOP_HEIGHT;
	BOOST_FOREACH (Group i, _input_groups) {
		dc.GetTextExtent (std_to_wx(i.name), &label_width, &label_height);
		int const height = (i.to - i.from + 1) * GRID_SPACING;
		dc.DrawRotatedText (
			std_to_wx(i.name),
			GRID_SPACING + (GRID_SPACING - label_height) / 2,
			y + (height + label_width) / 2,
			90
			);
		lines.MoveToPoint    (GRID_SPACING,     y);
		lines.AddLineToPoint (GRID_SPACING * 2, y);
		y += height;
	}

	lines.MoveToPoint    (GRID_SPACING,     y);
	lines.AddLineToPoint (GRID_SPACING * 2, y);

	gc->StrokePath (lines);

	/* Indicators */

	for (size_t x = 0; x < _output_channels.size(); ++x) {
		for (size_t y = 0; y < _input_channels.size(); ++y) {
			dc.SetBrush (*wxWHITE_BRUSH);
			dc.DrawRectangle (
				wxRect(
					LEFT_WIDTH + x * GRID_SPACING + (GRID_SPACING - INDICATOR_SIZE) / 2,
					TOP_HEIGHT + y * GRID_SPACING + (GRID_SPACING - INDICATOR_SIZE) / 2,
					INDICATOR_SIZE, INDICATOR_SIZE
					)
				);

			float const value_dB = 20 * log10 (_map.get(y, x));
			int const range = 18;
			int height = 0;
			if (value_dB > -range) {
				height = INDICATOR_SIZE * (1 + value_dB / range);
			}

			dc.SetBrush (*wxTheBrushList->FindOrCreateBrush(wxColour (0, 255, 0), wxBRUSHSTYLE_SOLID));
			dc.DrawRectangle (
				wxRect(
					LEFT_WIDTH + x * GRID_SPACING + (GRID_SPACING - INDICATOR_SIZE) / 2,
					TOP_HEIGHT + y * GRID_SPACING + (GRID_SPACING - INDICATOR_SIZE) / 2 + INDICATOR_SIZE - height,
					INDICATOR_SIZE, height
					)
				);
		}
	}

	delete gc;
}

optional<pair<int, int> >
AudioMappingView::mouse_event_to_channels (wxMouseEvent& ev) const
{
	int sx, sy;
	GetViewStart (&sx, &sy);
	int const x = ev.GetX() + sx * GRID_SPACING;
	int const y = ev.GetY() + sy * GRID_SPACING;

	if (x <= LEFT_WIDTH || y < TOP_HEIGHT) {
		return optional<pair<int, int> >();
	}

	int const input = (y - TOP_HEIGHT) / GRID_SPACING;
	int const output = (x - LEFT_WIDTH) / GRID_SPACING;

	if (input >= int(_input_channels.size()) || output >= int(_output_channels.size())) {
		return optional<pair<int, int> >();
	}

	return make_pair (input, output);
}

void
AudioMappingView::left_down (wxMouseEvent& ev)
{
	optional<pair<int, int> > channels = mouse_event_to_channels (ev);
	if (!channels) {
		return;
	}

	if (_map.get(channels->first, channels->second) > 0) {
		_map.set (channels->first, channels->second, 0);
	} else {
		_map.set (channels->first, channels->second, 1);
	}

	map_values_changed ();
}

void
AudioMappingView::right_down (wxMouseEvent& ev)
{
	optional<pair<int, int> > channels = mouse_event_to_channels (ev);
	if (!channels) {
		return;
	}

	_menu_input = channels->first;
	_menu_output = channels->second;
	PopupMenu (_menu, ev.GetPosition());
}

/** Called when any gain value has changed */
void
AudioMappingView::map_values_changed ()
{
	Changed (_map);
	_last_tooltip_channels = optional<pair<int, int> >();
	Refresh ();
}

void
AudioMappingView::off ()
{
	_map.set (_menu_input, _menu_output, 0);
	map_values_changed ();
}

void
AudioMappingView::full ()
{
	_map.set (_menu_input, _menu_output, 1);
	map_values_changed ();
}

void
AudioMappingView::minus6dB ()
{
	_map.set (_menu_input, _menu_output, pow (10, -6.0 / 20));
	map_values_changed ();
}

void
AudioMappingView::edit ()
{
	int const d = _menu_output - 1;

	AudioGainDialog* dialog = new AudioGainDialog (this, _menu_input, _menu_output - 1, _map.get(_menu_input, d));
	if (dialog->ShowModal() == wxID_OK) {
		_map.set (_menu_input, d, dialog->value ());
		map_values_changed ();
	}

	dialog->Destroy ();
}

void
AudioMappingView::set_virtual_size ()
{
	SetVirtualSize (LEFT_WIDTH + _output_channels.size() * GRID_SPACING, TOP_HEIGHT + _input_channels.size() * GRID_SPACING);
}

void
AudioMappingView::set (AudioMapping map)
{
	_map = map;
	Refresh ();
}

void
AudioMappingView::set_input_channels (vector<string> const & names)
{
	_input_channels = names;
	set_virtual_size ();
	Refresh ();
}

void
AudioMappingView::set_output_channels (vector<string> const & names)
{
	_output_channels = names;
	set_virtual_size ();
	Refresh ();
}

void
AudioMappingView::motion (wxMouseEvent& ev)
{
	optional<pair<int, int> > channels = mouse_event_to_channels (ev);
	if (!channels) {
		SetToolTip ("");
		_last_tooltip_channels = channels;
		return;
	}

	if (channels != _last_tooltip_channels) {
		wxString s;
		float const gain = _map.get(channels->first, channels->second);
		if (gain == 0) {
			s = wxString::Format (_("No audio will be passed from content channel %d to DCP channel %d."), channels->first + 1, channels->second + 1);
		} else if (gain == 1) {
			s = wxString::Format (_("Audio will be passed from content channel %d to DCP channel %d unaltered."), channels->first + 1, channels->second + 1);
		} else {
			float const dB = 20 * log10 (gain);
			s = wxString::Format (_("Audio will be passed from content channel %d to DCP channel %d with gain %.1fdB."), channels->first + 1, channels->second + 1, dB);
		}

		SetToolTip (s + " " + _("Right click to change gain."));
		_last_tooltip_channels = channels;
	}

        ev.Skip ();
}

void
AudioMappingView::set_input_groups (vector<Group> const & groups)
{
	_input_groups = groups;
}
