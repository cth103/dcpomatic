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
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
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
	: wxPanel (parent, wxID_ANY)
	, _menu_input (0)
	, _menu_output (1)
{
	_menu = new wxMenu;
	_menu->Append (ID_off, _("Off"));
	_menu->Append (ID_full, _("Full"));
	_menu->Append (ID_minus6dB, _("-6dB"));
	_menu->Append (ID_edit, _("Edit..."));

	_body = new wxPanel (this, wxID_ANY);
	_vertical_scroll = new wxScrollBar (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
	_horizontal_scroll = new wxScrollBar (this, wxID_ANY);

	Bind (wxEVT_SIZE, boost::bind(&AudioMappingView::size, this, _1));
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::off, this), ID_off);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::full, this), ID_full);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::minus6dB, this), ID_minus6dB);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::edit, this), ID_edit);
	Bind (wxEVT_MOUSEWHEEL, boost::bind(&AudioMappingView::mouse_wheel, this, _1));
	_body->Bind (wxEVT_PAINT, boost::bind(&AudioMappingView::paint, this));
	_body->Bind (wxEVT_LEFT_DOWN, boost::bind(&AudioMappingView::left_down, this, _1));
	_body->Bind (wxEVT_RIGHT_DOWN, boost::bind(&AudioMappingView::right_down, this, _1));
	_body->Bind (wxEVT_MOTION, boost::bind(&AudioMappingView::motion, this, _1));
	_vertical_scroll->Bind (wxEVT_SCROLL_TOP, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_BOTTOM, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_LINEUP, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_LINEDOWN, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_PAGEUP, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_PAGEDOWN, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_THUMBTRACK, boost::bind(&AudioMappingView::scroll, this));
	_vertical_scroll->Bind (wxEVT_SCROLL_THUMBRELEASE, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_TOP, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_BOTTOM, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_LINEUP, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_LINEDOWN, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_PAGEUP, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_PAGEDOWN, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_THUMBTRACK, boost::bind(&AudioMappingView::scroll, this));
	_horizontal_scroll->Bind (wxEVT_SCROLL_THUMBRELEASE, boost::bind(&AudioMappingView::scroll, this));
}

void
AudioMappingView::size (wxSizeEvent& ev)
{
	setup ();
	ev.Skip ();
}

void
AudioMappingView::setup ()
{
	wxSize const s = GetSize();
	int const w = _vertical_scroll->GetSize().GetWidth();
	int const h = _horizontal_scroll->GetSize().GetHeight();

	_vertical_scroll->SetPosition (wxPoint(s.GetWidth() - w, 0));
	_vertical_scroll->SetSize (wxSize(w, max(0, s.GetHeight() - h)));

	_body->SetSize (wxSize(max(0, s.GetWidth() - w), max(0, s.GetHeight() - h)));

	_horizontal_scroll->SetPosition (wxPoint(0, s.GetHeight() - h));
	_horizontal_scroll->SetSize (wxSize(max(0, s.GetWidth() - w), h));

	_vertical_scroll->SetScrollbar (
		_vertical_scroll->GetThumbPosition(),
		s.GetHeight() - h - 8,
		GRID_SPACING * (2 + _input_channels.size()),
		GRID_SPACING,
		true
		);

	_horizontal_scroll->SetScrollbar (
		_horizontal_scroll->GetThumbPosition(),
		s.GetWidth() - w - 8,
		GRID_SPACING * (3 + _output_channels.size()),
		GRID_SPACING,
		true);
}

void
AudioMappingView::scroll ()
{
	Refresh ();
}

void
AudioMappingView::paint_static (wxDC& dc)
{
	dc.SetFont (wxSWISS_FONT->Bold());
	wxCoord label_width;
	wxCoord label_height;

	/* DCP label at the top */

	dc.GetTextExtent (_("DCP"), &label_width, &label_height);
	dc.DrawText (_("DCP"), LEFT_WIDTH + (_output_channels.size() * GRID_SPACING - label_width) / 2, (GRID_SPACING - label_height) / 2);

	/* Content label on the left */

	dc.GetTextExtent (_("Content"), &label_width, &label_height);
	dc.DrawRotatedText (
		_("Content"),
		(GRID_SPACING - label_height) / 2,
		TOP_HEIGHT + (_input_channels.size() * GRID_SPACING + label_width) / 2,
		90
		);

	dc.SetFont (*wxSWISS_FONT);
}

void
AudioMappingView::paint_column_labels (wxDC& dc)
{
	wxCoord label_width;
	wxCoord label_height;
	int N = 0;
	BOOST_FOREACH (string i, _output_channels) {
		dc.GetTextExtent (std_to_wx(i), &label_width, &label_height);
		dc.DrawText (std_to_wx(i), LEFT_WIDTH + GRID_SPACING * N + (GRID_SPACING - label_width) / 2, GRID_SPACING + (GRID_SPACING - label_height) / 2);
		++N;
	}

	dc.DrawLine(wxPoint(LEFT_WIDTH, GRID_SPACING), wxPoint(LEFT_WIDTH + _output_channels.size() * GRID_SPACING, GRID_SPACING));
	dc.DrawLine(wxPoint(LEFT_WIDTH, GRID_SPACING * 2), wxPoint(LEFT_WIDTH + _output_channels.size() * GRID_SPACING, GRID_SPACING * 2));
}

void
AudioMappingView::paint_column_lines (wxDC& dc)
{
	for (size_t i = 0; i < _output_channels.size(); ++i) {
		dc.DrawLine (
			wxPoint(LEFT_WIDTH + GRID_SPACING * i, GRID_SPACING),
			wxPoint(LEFT_WIDTH + GRID_SPACING * i, TOP_HEIGHT + _input_channels.size() * GRID_SPACING)
			);
	}

	dc.DrawLine (
		wxPoint(LEFT_WIDTH + GRID_SPACING * _output_channels.size(), GRID_SPACING),
		wxPoint(LEFT_WIDTH + GRID_SPACING * _output_channels.size(), TOP_HEIGHT + _input_channels.size() * GRID_SPACING)
		);
}

void
AudioMappingView::paint_row_labels (wxDC& dc)
{
	wxCoord label_width;
	wxCoord label_height;

	/* Row channel labels */

	int N = 0;
	BOOST_FOREACH (string i, _input_channels) {
		dc.GetTextExtent (std_to_wx(i), &label_width, &label_height);
		dc.DrawText (std_to_wx(i), GRID_SPACING * 2 + (GRID_SPACING - label_width) / 2, TOP_HEIGHT + GRID_SPACING * N + (GRID_SPACING - label_height) / 2);
		++N;
	}

	/* Vertical lines on the left */

	for (int i = 1; i < 3; ++i) {
		dc.DrawLine (
			wxPoint(GRID_SPACING * i, TOP_HEIGHT),
			wxPoint(GRID_SPACING * i, TOP_HEIGHT + _input_channels.size() * GRID_SPACING)
			);
	}

	/* Group labels and lines */

	int y = TOP_HEIGHT;
	BOOST_FOREACH (Group i, _input_groups) {
		int const height = (i.to - i.from + 1) * GRID_SPACING;
		dc.GetTextExtent (std_to_wx(i.name), &label_width, &label_height);
		if (label_width > height) {
			label_width = height - 8;
		}

		{
			int yp = y;
			if ((yp - 2 * GRID_SPACING) < dc.GetLogicalOrigin().y) {
				yp += dc.GetLogicalOrigin().y;
			}

			wxCoord old_x, old_y, old_width, old_height;
			dc.GetClippingBox (&old_x, &old_y, &old_width, &old_height);
			dc.DestroyClippingRegion ();
			dc.SetClippingRegion (GRID_SPACING, yp + 4, GRID_SPACING, height - 8);

			dc.DrawRotatedText (
				std_to_wx(i.name),
				GRID_SPACING + (GRID_SPACING - label_height) / 2,
				y + (height + label_width) / 2,
				90
				);

			dc.DestroyClippingRegion ();
			dc.SetClippingRegion (old_x, old_y, old_width, old_height);
		}

		dc.DrawLine (wxPoint(GRID_SPACING, y), wxPoint(GRID_SPACING * 2, y));
		y += height;
	}

	dc.DrawLine (wxPoint(GRID_SPACING, y), wxPoint(GRID_SPACING * 2, y));
	}

void
AudioMappingView::paint_row_lines (wxDC& dc)
{
	for (size_t i = 0; i < _input_channels.size(); ++i) {
		dc.DrawLine (
			wxPoint(GRID_SPACING * 2, TOP_HEIGHT + GRID_SPACING * i),
			wxPoint(LEFT_WIDTH + _output_channels.size() * GRID_SPACING, TOP_HEIGHT + GRID_SPACING * i)
			);
	}
	dc.DrawLine (
		wxPoint(GRID_SPACING * 2, TOP_HEIGHT + GRID_SPACING * _input_channels.size()),
		wxPoint(LEFT_WIDTH + _output_channels.size() * GRID_SPACING, TOP_HEIGHT + GRID_SPACING * _input_channels.size())
		);
}

void
AudioMappingView::paint_indicators (wxDC& dc)
{
	/* _{input,output}_channels and _map may not always be in sync, be careful here */
	size_t const output = min(_output_channels.size(), size_t(_map.output_channels()));
	size_t const input = min(_input_channels.size(), size_t(_map.input_channels()));

	for (size_t x = 0; x < output; ++x) {
		for (size_t y = 0; y < input; ++y) {
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
}

static
void clip (wxDC& dc, int x, int y, int w, int h)
{
	dc.SetClippingRegion (x, y, w, h);
}

static
void translate (wxDC& dc, int x, int y)
{
	dc.SetLogicalOrigin (x, y);
}

static
void restore (wxDC& dc)
{
	dc.SetLogicalOrigin (0, 0);
	dc.DestroyClippingRegion ();
}

void
AudioMappingView::paint ()
{
	wxPaintDC dc (_body);

	int const hs = _horizontal_scroll->GetThumbPosition ();
	int const vs = _vertical_scroll->GetThumbPosition ();

	paint_static (dc);

	clip (dc, LEFT_WIDTH, 0, GRID_SPACING * _output_channels.size(), GRID_SPACING * (2 + _input_channels.size()));
	translate (dc, hs, 0);
	paint_column_labels (dc);
	restore (dc);

	clip (dc, 0, TOP_HEIGHT, GRID_SPACING * (3 + _output_channels.size()), GRID_SPACING * _input_channels.size() + 1);
	translate (dc, 0, vs);
	paint_row_labels (dc);
	restore (dc);

	clip (dc, GRID_SPACING * 2, TOP_HEIGHT, GRID_SPACING * (1 + _output_channels.size()), GRID_SPACING * _input_channels.size() + 1);
	translate (dc, hs, vs);
	paint_row_lines (dc);
	restore (dc);

	clip (dc, LEFT_WIDTH, GRID_SPACING, GRID_SPACING * (1 + _output_channels.size()), GRID_SPACING * (1 + _input_channels.size()));
	translate (dc, hs, vs);
	paint_column_lines (dc);
	restore (dc);

	clip (dc, LEFT_WIDTH, TOP_HEIGHT, GRID_SPACING * _output_channels.size(), GRID_SPACING * _input_channels.size());
	translate (dc, hs, vs);
	paint_indicators (dc);
	restore (dc);
}

optional<pair<int, int> >
AudioMappingView::mouse_event_to_channels (wxMouseEvent& ev) const
{
	int const x = ev.GetX() + _horizontal_scroll->GetThumbPosition();
	int const y = ev.GetY() + _vertical_scroll->GetThumbPosition();

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

optional<string>
AudioMappingView::mouse_event_to_input_group_name (wxMouseEvent& ev) const
{
	int const x = ev.GetX() + _horizontal_scroll->GetThumbPosition();
	if (x < GRID_SPACING || x > (2 * GRID_SPACING)) {
		return optional<string>();
	}

	int y = (ev.GetY() + _vertical_scroll->GetThumbPosition() - (GRID_SPACING * 2)) / GRID_SPACING;
	BOOST_FOREACH (Group i, _input_groups) {
		if (i.from <= y && y <= i.to) {
			return i.name;
		}
	}

	return optional<string>();
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

void
AudioMappingView::mouse_wheel (wxMouseEvent& ev)
{
	if (ev.ShiftDown()) {
		_horizontal_scroll->SetThumbPosition (
			_horizontal_scroll->GetThumbPosition() + (ev.GetWheelRotation() > 0 ? -GRID_SPACING : GRID_SPACING)
			);

	} else {
		_vertical_scroll->SetThumbPosition (
			_vertical_scroll->GetThumbPosition() + (ev.GetWheelRotation() > 0 ? -GRID_SPACING : GRID_SPACING)
			);
	}
	Refresh ();
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
	AudioGainDialog* dialog = new AudioGainDialog (this, _menu_input, _menu_output, _map.get(_menu_input, _menu_output));
	if (dialog->ShowModal() == wxID_OK) {
		_map.set (_menu_input, _menu_output, dialog->value ());
		map_values_changed ();
	}

	dialog->Destroy ();
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
	setup ();
	Refresh ();
}

void
AudioMappingView::set_output_channels (vector<string> const & names)
{
	_output_channels = names;
	setup ();
	Refresh ();
}

void
AudioMappingView::motion (wxMouseEvent& ev)
{
	optional<pair<int, int> > channels = mouse_event_to_channels (ev);
	if (channels) {
		if (channels != _last_tooltip_channels) {
			wxString s;
			float const gain = _map.get(channels->first, channels->second);
			if (gain == 0) {
				s = wxString::Format (
					_("No audio will be passed from content channel %d to DCP channel %d."),
					channels->first + 1, channels->second + 1
					);
			} else if (gain == 1) {
				s = wxString::Format (
					_("Audio will be passed from content channel %d to DCP channel %d unaltered."),
					channels->first + 1, channels->second + 1
					);
			} else {
				float const dB = 20 * log10 (gain);
				s = wxString::Format (
					_("Audio will be passed from content channel %d to DCP channel %d with gain %.1fdB."),
					channels->first + 1, channels->second + 1, dB
					);
			}

			SetToolTip (s + " " + _("Right click to change gain."));
		}
	} else {
		optional<string> group = mouse_event_to_input_group_name (ev);
		if (group) {
			SetToolTip (std_to_wx(*group));
		} else {
			SetToolTip ("");
		}
	}

	_last_tooltip_channels = channels;
        ev.Skip ();
}

void
AudioMappingView::set_input_groups (vector<Group> const & groups)
{
	_input_groups = groups;
}
