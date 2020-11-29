/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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
#include "lib/warnings.h"
#include <dcp/locale_convert.h>
#include <dcp/types.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
#include <wx/renderer.h>
#include <wx/grid.h>
#include <wx/graphics.h>
DCPOMATIC_ENABLE_WARNINGS
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
	ID_minus6dB = 2,
	ID_0dB = 3,
	ID_plus3dB = 4,
	ID_edit = 5
};

AudioMappingView::AudioMappingView (wxWindow* parent, wxString left_label, wxString from, wxString top_label, wxString to)
	: wxPanel (parent, wxID_ANY)
	, _menu_input (0)
	, _menu_output (1)
	, _left_label (left_label)
	, _from (from)
	, _top_label (top_label)
	, _to (to)
{
	_menu = new wxMenu;
	_menu->Append (ID_off, _("Off"));
	_menu->Append (ID_minus6dB, _("-6dB"));
	_menu->Append (ID_0dB, _("0dB (unchanged)"));
	_menu->Append (ID_plus3dB, _("+3dB"));
	_menu->Append (ID_edit, _("Edit..."));

	_body = new wxPanel (this, wxID_ANY);
	_vertical_scroll = new wxScrollBar (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSB_VERTICAL);
	_horizontal_scroll = new wxScrollBar (this, wxID_ANY);

#ifndef __WXOSX__
	SetDoubleBuffered (true);
#endif

	Bind (wxEVT_SIZE, boost::bind(&AudioMappingView::size, this, _1));
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::set_gain_from_menu, this, 0), ID_off);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::set_gain_from_menu, this, db_to_linear(-6)), ID_minus6dB);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::set_gain_from_menu, this, 1), ID_0dB);
	Bind (wxEVT_MENU, boost::bind(&AudioMappingView::set_gain_from_menu, this, db_to_linear(3)), ID_plus3dB);
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

	dc.GetTextExtent (_top_label, &label_width, &label_height);
	dc.DrawText (_top_label, LEFT_WIDTH + (_output_channels.size() * GRID_SPACING - label_width) / 2, (GRID_SPACING - label_height) / 2);

	dc.GetTextExtent (_left_label, &label_width, &label_height);
	dc.DrawRotatedText (
		_left_label,
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
	BOOST_FOREACH (NamedChannel const& i, _output_channels) {
		dc.GetTextExtent (std_to_wx(i.name), &label_width, &label_height);
		dc.DrawText (std_to_wx(i.name), LEFT_WIDTH + GRID_SPACING * N + (GRID_SPACING - label_width) / 2, GRID_SPACING + (GRID_SPACING - label_height) / 2);
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
	BOOST_FOREACH (NamedChannel const& i, _input_channels) {
		dc.GetTextExtent (std_to_wx(i.name), &label_width, &label_height);
		dc.DrawText (std_to_wx(i.name), GRID_SPACING * 2 + (GRID_SPACING - label_width) / 2, TOP_HEIGHT + GRID_SPACING * N + (GRID_SPACING - label_height) / 2);
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
			wxDCClipper clip (dc, wxRect(GRID_SPACING, y, GRID_SPACING, height));
			int yp = y;
			if ((yp - 2 * GRID_SPACING) < dc.GetLogicalOrigin().y) {
				yp += dc.GetLogicalOrigin().y;
			}

			dc.DrawRotatedText (
				std_to_wx(i.name),
				GRID_SPACING + (GRID_SPACING - label_height) / 2,
				y + (height + label_width) / 2,
				90
				);
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

			float const value_dB = linear_to_db(_map.get(_input_channels[y].index, _output_channels[x].index));
			wxColour const colour = value_dB <= 0 ? wxColour(0, 255, 0) : wxColour(255, 150, 0);
			int const range = 18;
			int height = 0;
			if (value_dB > -range) {
				height = min(INDICATOR_SIZE, static_cast<int>(INDICATOR_SIZE * (1 + value_dB / range)));
			}

			dc.SetBrush (*wxTheBrushList->FindOrCreateBrush(colour, wxBRUSHSTYLE_SOLID));
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

	clip (
		dc,
		LEFT_WIDTH,
		0,
		GRID_SPACING * _output_channels.size(),
		GRID_SPACING * (2 + _input_channels.size())
	     );
	translate (dc, hs, 0);
	paint_column_labels (dc);
	restore (dc);

	clip (
		dc,
		0,
		TOP_HEIGHT,
		GRID_SPACING * 3,
		min(int(GRID_SPACING * _input_channels.size()), GetSize().GetHeight() - TOP_HEIGHT)
	     );
	translate (dc, 0, vs);
	paint_row_labels (dc);
	restore (dc);

	clip (
		dc,
		GRID_SPACING * 2,
		TOP_HEIGHT,
		GRID_SPACING * (1 + _output_channels.size()),
		min(int(GRID_SPACING * (2 + _input_channels.size())), GetSize().GetHeight() - TOP_HEIGHT)
	     );
	translate (dc, hs, vs);
	paint_row_lines (dc);
	restore (dc);

	clip (
		dc,
		LEFT_WIDTH,
		GRID_SPACING,
		GRID_SPACING * (1 + _output_channels.size()),
		min(int(GRID_SPACING * (1 + _input_channels.size())), GetSize().GetHeight() - GRID_SPACING)
	     );
	translate (dc, hs, vs);
	paint_column_lines (dc);
	restore (dc);

	clip (
		dc,
		LEFT_WIDTH,
		TOP_HEIGHT,
		GRID_SPACING * _output_channels.size(),
		min(int(GRID_SPACING * _input_channels.size()), GetSize().GetHeight() - TOP_HEIGHT)
	     );
	translate (dc, hs, vs);
	paint_indicators (dc);
	restore (dc);
}

optional<pair<NamedChannel, NamedChannel> >
AudioMappingView::mouse_event_to_channels (wxMouseEvent& ev) const
{
	int const x = ev.GetX() + _horizontal_scroll->GetThumbPosition();
	int const y = ev.GetY() + _vertical_scroll->GetThumbPosition();

	if (x <= LEFT_WIDTH || y < TOP_HEIGHT) {
		return optional<pair<NamedChannel, NamedChannel> >();
	}

	int const input = (y - TOP_HEIGHT) / GRID_SPACING;
	int const output = (x - LEFT_WIDTH) / GRID_SPACING;

	if (input >= int(_input_channels.size()) || output >= int(_output_channels.size())) {
		return optional<pair<NamedChannel, NamedChannel> >();
	}

	return make_pair (_input_channels[input], _output_channels[output]);
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
	optional<pair<NamedChannel, NamedChannel> > channels = mouse_event_to_channels (ev);
	if (!channels) {
		return;
	}

	if (_map.get(channels->first.index, channels->second.index) > 0) {
		_map.set (channels->first.index, channels->second.index, 0);
	} else {
		_map.set (channels->first.index, channels->second.index, 1);
	}

	map_values_changed ();
}

void
AudioMappingView::right_down (wxMouseEvent& ev)
{
	optional<pair<NamedChannel, NamedChannel> > channels = mouse_event_to_channels (ev);
	if (!channels) {
		return;
	}

	_menu_input = channels->first.index;
	_menu_output = channels->second.index;
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
	_last_tooltip_channels = optional<pair<NamedChannel, NamedChannel> >();
	Refresh ();
}

void
AudioMappingView::set_gain_from_menu (double linear)
{
	_map.set (_menu_input, _menu_output, linear);
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
AudioMappingView::set_input_channels (vector<NamedChannel> const& channels)
{
	_input_channels = channels;
	setup ();
	Refresh ();
}

void
AudioMappingView::set_output_channels (vector<NamedChannel> const & channels)
{
	_output_channels = channels;
	setup ();
	Refresh ();
}


wxString
AudioMappingView::input_channel_name_with_group (NamedChannel const& n) const
{
	optional<wxString> group;
	BOOST_FOREACH (Group i, _input_groups) {
		if (i.from <= n.index && n.index <= i.to) {
			group = std_to_wx (i.name);
		}
	}

	if (group && !group->IsEmpty()) {
		return wxString::Format ("%s/%s", group->data(), std_to_wx(n.name).data());
	}

	return std_to_wx(n.name);
}


void
AudioMappingView::motion (wxMouseEvent& ev)
{
	optional<pair<NamedChannel, NamedChannel> > channels = mouse_event_to_channels (ev);
	if (channels) {
		if (channels != _last_tooltip_channels) {
			wxString s;
			float const gain = _map.get(channels->first.index, channels->second.index);
			if (gain == 0) {
				s = wxString::Format (
					_("No audio will be passed from %s channel '%s' to %s channel '%s'."),
					_from,
					input_channel_name_with_group(channels->first),
					_to,
					std_to_wx(channels->second.name)
					);
			} else if (gain == 1) {
				s = wxString::Format (
					_("Audio will be passed from %s channel %s to %s channel %s unaltered."),
					_from,
					input_channel_name_with_group(channels->first),
					_to,
					std_to_wx(channels->second.name)
					);
			} else {
				float const dB = linear_to_db(gain);
				s = wxString::Format (
					_("Audio will be passed from %s channel %s to %s channel %s with gain %.1fdB."),
					_from,
					input_channel_name_with_group(channels->first),
					_to,
					std_to_wx(channels->second.name),
					dB
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
