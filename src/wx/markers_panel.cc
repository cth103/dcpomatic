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


using std::make_pair;
using std::pair;
using std::shared_ptr;
using std::vector;
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


MarkersPanel::MarkersPanel(wxWindow* parent, FilmViewer& viewer)
	: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 40))
	, _viewer(viewer)
{
	Bind(wxEVT_PAINT, boost::bind(&MarkersPanel::paint, this));
	Bind(wxEVT_MOTION, boost::bind(&MarkersPanel::mouse_moved, this, _1));
	Bind(wxEVT_SIZE, boost::bind(&MarkersPanel::size, this));

	Bind(wxEVT_LEFT_DOWN, boost::bind(&MarkersPanel::mouse_left_down, this));
	Bind(wxEVT_RIGHT_DOWN, boost::bind(&MarkersPanel::mouse_right_down, this, _1));

	Bind(wxEVT_MENU, boost::bind(&MarkersPanel::move_marker_to_current_position, this), ID_move_marker_to_current_position);
	Bind(wxEVT_MENU, boost::bind(&MarkersPanel::remove_marker, this), ID_remove_marker);
	Bind(wxEVT_MENU, boost::bind(&MarkersPanel::add_marker, this, _1), ID_add_base, ID_add_base + all_editable_markers().size() + uneditable_markers);
}


void
MarkersPanel::size()
{
	layout();
}


void
MarkersPanel::set_film(weak_ptr<Film> weak_film)
{
	_film = weak_film;
	if (auto film = weak_film.lock()) {
		film->Change.connect(boost::bind(&MarkersPanel::film_changed, this, _1, _2));
		layout();
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
		layout();
	}
}


void
MarkersPanel::layout()
{
	auto film = _film.lock();
	if (!film || !film->length().get()) {
		_components.clear();
		return;
	}

	wxClientDC dc(this);
	auto const panel_width = GetSize().GetWidth();

#ifdef DCPOMATIC_LINUX
		/* Number of pixels between the left/right bounding box edge of a wxSlider
		 * and the start of the "track".
		 */
		auto constexpr end_gap = 12;
#else
		auto constexpr end_gap = 0;
#endif

	_components = layout_markers(
		film->markers(),
		panel_width - end_gap,
		film->length(),
		12,
		4,
		[&dc](std::string text) { return dc.GetTextExtent(std_to_wx(text)).GetWidth(); }
	);

	_over = nullptr;
	_menu_marker = nullptr;

	Refresh();
}


void
MarkersPanel::mouse_moved(wxMouseEvent& ev)
{
	_over = nullptr;

	auto film = _film.lock();
	if (!film) {
		return;
	}

	auto const panel_width = GetSize().GetWidth();

#if !defined(DCPOMATIC_LINUX)
	auto const panel_height = GetSize().GetHeight();
	auto const factor = GetContentScaleFactor();
#endif

	auto const scale = static_cast<float>(panel_width) / film->length().get();

	auto const x = ev.GetPosition().x;

	for (auto const& marker: _components) {
		auto const p = marker.t1.get() * scale;
		if (marker.marker && std::abs(p - x) < 16) {
			_over = &marker;
/* Tooltips flicker really badly on Wayland for some reason, so only do this on Windows/macOS for now */
#if !defined(DCPOMATIC_LINUX)
			if (!_tip) {
				auto mouse = ClientToScreen(ev.GetPosition());
				auto rect = wxRect(mouse.x, mouse.y, 8 * factor, panel_height * factor);
				auto hmsf = marker.t1.split(film->video_frame_rate());
				auto const tip_text = fmt::format("{} {:02d}:{:02d}:{:02d}:{:02d}", dcp::marker_to_string(*marker.marker), hmsf.h, hmsf.m, hmsf.s, hmsf.f);
				_tip = new wxTipWindow(this, std_to_wx(tip_text), 100, &_tip, &rect);
			}
#endif
		}
	}
}


void
MarkersPanel::paint()
{
	wxPaintDC dc(this);

	std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
	if (!gc) {
		return;
	}

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
	auto const colour = gui_is_dark() ? wxColour(199, 139, 167) : wxColour(200, 0, 0);
	gc->SetPen(colour);
	gc->SetFont(gc->CreateFont(*wxSMALL_FONT, colour));
	gc->SetBrush(GetBackgroundColour());

	auto const panel_height = GetSize().GetHeight();

	int rows = 0;
	for (auto const& component: _components) {
		rows = std::max(rows, component.y + 1);
	}

	auto const row_height = std::min(static_cast<float>(panel_height) / rows, 16.0f);
	auto const row_gap = 3;

	auto const base = [row_height, panel_height](MarkerLayoutComponent const& component) {
		return panel_height - (component.y + 1) * row_height;
	};

	for (auto const& component: _components) {
		if (component.type == MarkerLayoutComponent::Type::LINE) {
			gc->CreatePath();
			auto line = gc->CreatePath();
			line.MoveToPoint(component.x1, base(component) + (row_height - row_gap) / 2);
			line.AddLineToPoint(component.x2, base(component) + (row_height - row_gap) / 2);
			gc->StrokePath(line);
		}
	}

	for (auto const& component: _components) {
		switch (component.type) {
		case MarkerLayoutComponent::Type::LEFT:
		case MarkerLayoutComponent::Type::RIGHT:
		{
			gc->CreatePath();
			auto line = gc->CreatePath();
			line.MoveToPoint(component.x1, base(component));
			line.AddLineToPoint(component.x1, base(component) + row_height - row_gap);
			gc->StrokePath(line);
			break;
		}
		case MarkerLayoutComponent::Type::LABEL:
		{
			gc->CreatePath();
			auto rectangle = gc->CreatePath();
			rectangle.MoveToPoint(component.x1 - 2, base(component));
			rectangle.AddRectangle(component.x1 - 2, base(component), component.x2 - component.x1, row_height);
			gc->FillPath(rectangle);
			gc->DrawText(std_to_wx(component.text), component.x1, base(component) - 4);
			break;
		}

		case MarkerLayoutComponent::Type::LINE:
			break;
		}
	}
}


void
MarkersPanel::mouse_left_down()
{
	if (_over) {
		_viewer.seek(_over->t1, true);
	}
}


void
MarkersPanel::mouse_right_down(wxMouseEvent& ev)
{
	wxMenu menu;
	if (_over) {
		DCPOMATIC_ASSERT(_over->marker);
		menu.Append(ID_move_marker_to_current_position, wxString::Format(_("Move %s marker to current position"), std_to_wx(dcp::marker_to_string(*_over->marker))));
		menu.Append(ID_remove_marker, wxString::Format(_("Remove %s marker"), std_to_wx(dcp::marker_to_string(*_over->marker))));
	}

	auto add_marker = new wxMenu();
	for (auto const& marker: all_editable_markers()) {
		add_marker->Append(static_cast<int>(ID_add_base) + static_cast<int>(marker.second), marker.first);
	}
	menu.Append(ID_add_marker, _("Add or move marker to current position"), add_marker);

	_menu_marker = _over;
	PopupMenu(&menu, ev.GetPosition());
}


void
MarkersPanel::move_marker_to_current_position()
{
	auto film = _film.lock();
	if (!film || !_menu_marker) {
		return;
	}

	DCPOMATIC_ASSERT(static_cast<bool>(_menu_marker->marker));
	film->set_marker(*_menu_marker->marker, _viewer.position());
}


void
MarkersPanel::remove_marker()
{
	auto film = _film.lock();
	if (!film || !_menu_marker) {
		return;
	}

	DCPOMATIC_ASSERT(static_cast<bool>(_menu_marker->marker));
	film->unset_marker(*_menu_marker->marker);
}


void
MarkersPanel::add_marker(wxCommandEvent& ev)
{
	auto film = _film.lock();
	if (!film) {
		return;
	}

	auto marker = static_cast<dcp::Marker>(ev.GetId() - ID_add_base);
	film->set_marker(marker, _viewer.position());
}

