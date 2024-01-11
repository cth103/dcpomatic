/*
    Copyright (C) 2021-2022 Carl Hetherington <cth@carlh.net>

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


#include "film_viewer.h"
#include "id.h"
#include "markers.h"
#include "markers_panel.h"
#include "wx_util.h"
#include "lib/film.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
#include <wx/tipwin.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>
#include <boost/version.hpp>


using std::shared_ptr;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


enum {
	ID_move_marker_to_current_position = DCPOMATIC_MARKERS_PANEL_MENU,
	ID_remove_marker,
	ID_add_marker,
	/* Leave some space after this one as we use an ID for each marker type
	 * starting with ID_add_base.
	 */
	ID_add_base,
};


static constexpr auto line_to_label_gap = 2;


MarkersPanel::MarkersPanel(wxWindow* parent, FilmViewer& viewer)
	: wxPanel (parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 16))
	, _viewer (viewer)
{
	Bind (wxEVT_PAINT, boost::bind(&MarkersPanel::paint, this));
	Bind (wxEVT_MOTION, boost::bind(&MarkersPanel::mouse_moved, this, _1));

	Bind (wxEVT_LEFT_DOWN, boost::bind(&MarkersPanel::mouse_left_down, this));
	Bind (wxEVT_RIGHT_DOWN, boost::bind(&MarkersPanel::mouse_right_down, this, _1));

	Bind (wxEVT_MENU, boost::bind(&MarkersPanel::move_marker_to_current_position, this), ID_move_marker_to_current_position);
	Bind (wxEVT_MENU, boost::bind(&MarkersPanel::remove_marker, this), ID_remove_marker);
	Bind (wxEVT_MENU, boost::bind(&MarkersPanel::add_marker, this, _1), ID_add_base, ID_add_base + all_editable_markers().size());
}


void
MarkersPanel::set_film (weak_ptr<Film> weak_film)
{
	_film = weak_film;
	auto film = weak_film.lock ();
	if (film) {
		film->Change.connect (boost::bind(&MarkersPanel::film_changed, this, _1, _2));
		update_from_film (film);
	}
}


void
MarkersPanel::film_changed(ChangeType type, FilmProperty property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	auto film = _film.lock();
	if (!film) {
		return;
	}

	if (property == FilmProperty::MARKERS || property == FilmProperty::CONTENT || property == FilmProperty::CONTENT_ORDER || property == FilmProperty::VIDEO_FRAME_RATE) {
		update_from_film (film);
	}
}


void
MarkersPanel::update_from_film (shared_ptr<Film> film)
{
	_markers.clear ();
	for (auto const& marker: film->markers()) {
		_markers[marker.first] = Marker(
			marker.second,
			marker.first == dcp::Marker::FFOC ||
			marker.first == dcp::Marker::FFTC ||
			marker.first == dcp::Marker::FFOI ||
			marker.first == dcp::Marker::FFEC ||
			marker.first == dcp::Marker::FFMC
			);

	}
	Refresh ();
}


int
MarkersPanel::position (dcpomatic::DCPTime time, int width) const
{
#ifdef DCPOMATIC_LINUX
	/* Number of pixels between the left/right bounding box edge of a wxSlider
	 * and the start of the "track".
	 */
	auto constexpr end_gap = 12;
#else
	auto constexpr end_gap = 0;
#endif
	auto film = _film.lock ();
	if (!film) {
		return 0;
	}

	return end_gap + time.get() * (width - end_gap * 2) / film->length().get();
}


void
MarkersPanel::mouse_moved (wxMouseEvent& ev)
{
	_over = boost::none;

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	auto const panel_width = GetSize().GetWidth();
#if !defined(DCPOMATIC_LINUX)
	auto const panel_height = GetSize().GetHeight();
	auto const factor = GetContentScaleFactor();
#endif

	auto const x = ev.GetPosition().x;
	for (auto const& marker: _markers) {
		auto const pos = position(marker.second.time, panel_width);
		auto const width = marker.second.width ? marker.second.width : 4;
		auto const x1 = marker.second.line_before_label ? pos : pos - width - line_to_label_gap;
		auto const x2 = marker.second.line_before_label ? pos + width + line_to_label_gap : pos;
		if (x1 <= x && x < x2) {
			_over = marker.first;
/* Tooltips flicker really badly on Wayland for some reason, so only do this on Windows/macOS for now */
#if !defined(DCPOMATIC_LINUX)
			if (!_tip) {
				auto mouse = ClientToScreen (ev.GetPosition());
				auto rect = wxRect(mouse.x, mouse.y, width * factor, panel_height * factor);
				auto hmsf = marker.second.time.split(film->video_frame_rate());
				char timecode_buffer[64];
				snprintf (timecode_buffer, sizeof(timecode_buffer), " %02d:%02d:%02d:%02d", hmsf.h, hmsf.m, hmsf.s, hmsf.f);
				auto tip_text = dcp::marker_to_string(marker.first) + std::string(timecode_buffer);
				_tip = new wxTipWindow (this, std_to_wx(tip_text), 100, &_tip, &rect);
			}
#endif
		}
	}
}


void
MarkersPanel::paint ()
{
	wxPaintDC dc (this);

	std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
	if (!gc) {
		return;
	}

	gc->SetAntialiasMode (wxANTIALIAS_DEFAULT);
	gc->SetPen (wxPen(wxColour(200, 0, 0)));
	gc->SetFont (gc->CreateFont(*wxSMALL_FONT, wxColour(200, 0, 0)));

	auto const panel_width = GetSize().GetWidth();
	auto const panel_height = GetSize().GetHeight();

	for (auto& marker: _markers) {
		auto label = std_to_wx(dcp::marker_to_string(marker.first));
		if (marker.second.width == 0) {
			/* We don't know the width of this marker label yet, so calculate it now */
			wxDouble width, height, descent, external_leading;
			gc->GetTextExtent (label, &width, &height, &descent, &external_leading);
			marker.second.width = width;
		}
		auto line = gc->CreatePath ();
		auto const pos = position(marker.second.time, panel_width);
		line.MoveToPoint (pos, 0);
		line.AddLineToPoint (pos, panel_height);
		gc->StrokePath (line);
		if (marker.second.line_before_label) {
			gc->DrawText (label, pos + line_to_label_gap, 0);
		} else {
			gc->DrawText (label, pos - line_to_label_gap - marker.second.width, 0);
		}
	}
}


void
MarkersPanel::mouse_left_down ()
{
	if (_over) {
		_viewer.seek(_markers[*_over].time, true);
	}
}


void
MarkersPanel::mouse_right_down (wxMouseEvent& ev)
{
	wxMenu menu;
	if (_over) {
		menu.Append (ID_move_marker_to_current_position, wxString::Format(_("Move %s marker to current position"), wx_to_std(dcp::marker_to_string(*_over))));
		menu.Append (ID_remove_marker, wxString::Format(_("Remove %s marker"), wx_to_std(dcp::marker_to_string(*_over))));
	}

	auto add_marker = new wxMenu ();
	for (auto const& marker: all_editable_markers()) {
		add_marker->Append (static_cast<int>(ID_add_base) + static_cast<int>(marker.second), marker.first);
	}
	menu.Append (ID_add_marker, _("Add or move marker to current position"), add_marker);

	_menu_marker = _over;
	PopupMenu (&menu, ev.GetPosition());
}


void
MarkersPanel::move_marker_to_current_position ()
{
	auto film = _film.lock ();
	if (!film || !_menu_marker) {
		return;
	}

	film->set_marker(*_menu_marker, _viewer.position());
}


void
MarkersPanel::remove_marker ()
{
	auto film = _film.lock ();
	if (!film || !_menu_marker) {
		return;
	}

	film->unset_marker(*_menu_marker);
}


void
MarkersPanel::add_marker (wxCommandEvent& ev)
{
	auto film = _film.lock ();
	if (!film) {
		return;
	}

	auto marker = static_cast<dcp::Marker>(ev.GetId() - ID_add_base);
	film->set_marker(marker, _viewer.position());
}

