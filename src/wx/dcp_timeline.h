/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_DCP_TIMELINE_H
#define DCPOMATIC_DCP_TIMELINE_H


#include "timecode.h"
#include "timeline.h"
#include "lib/change_signaller.h"
#include "lib/film_property.h"
#include "lib/rect.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <memory>


class CheckBox;
class Choice;
class Film;
class ReelBoundary;
class SpinCtrl;
class wxGridBagSizer;


class DCPTimeline : public Timeline
{
public:
	DCPTimeline(wxWindow* parent, std::shared_ptr<Film> film);

	void force_redraw(dcpomatic::Rect<int> const &);

private:
	void paint();
	void paint_reels(wxGraphicsContext* gc);
	void paint_content(wxGraphicsContext* gc);
	void setup_pixels_per_second();
	void left_down(wxMouseEvent& ev);
	void right_down(wxMouseEvent& ev);
	void left_up(wxMouseEvent& ev);
	void mouse_moved(wxMouseEvent& ev);
	void reel_mode_changed();
	void maximum_reel_size_changed();
	void film_changed(ChangeType type, FilmProperty property);
	std::shared_ptr<Film> film() const;
	void setup_sensitivity();

	void add_reel_boundary();
	void setup_reel_settings();
	void setup_reel_boundaries();
	std::shared_ptr<ReelBoundary> event_to_reel_boundary(wxMouseEvent& ev) const;
	std::shared_ptr<ReelBoundary> position_to_reel_boundary(Position<int> position) const;
	void set_reel_boundary(int index, dcpomatic::DCPTime time);
	bool editable() const;

	std::weak_ptr<Film> _film;

	wxScrolledCanvas* _canvas;

	class Drag
	{
	public:
		Drag(
			std::shared_ptr<ReelBoundary> reel_boundary_,
			std::vector<std::shared_ptr<ReelBoundary>> const& reel_boundaries,
			std::shared_ptr<const Film> film,
			int offset_,
			bool snap,
			dcpomatic::DCPTime snap_distance
		    );

		std::shared_ptr<ReelBoundary> reel_boundary;
		std::shared_ptr<ReelBoundary> previous;
		std::shared_ptr<ReelBoundary> next;
		int offset = 0;

		void set_time(dcpomatic::DCPTime time);
		dcpomatic::DCPTime time() const;

	private:
		std::vector<dcpomatic::DCPTime> _snaps;
		dcpomatic::DCPTime _snap_distance;
	};

	boost::optional<Drag> _drag;

	wxPoint _right_down_position;

	wxPanel* _reel_settings;
	Choice* _reel_type;
	SpinCtrl* _maximum_reel_size;
	CheckBox* _snap;
	wxPanel* _reel_detail;
	wxGridBagSizer* _reel_detail_sizer;

	wxMenu* _menu;
	wxMenuItem* _add_reel_boundary;

	boost::signals2::scoped_connection _film_connection;

	std::vector<std::shared_ptr<ReelBoundary>> _reel_boundaries;
};


#endif

