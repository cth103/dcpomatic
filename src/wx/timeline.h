/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "content_menu.h"
#include "timeline_content_view.h"
#include "lib/util.h"
#include "lib/rect.h"
#include "lib/film.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/signals2.hpp>

class Film;
class ContentPanel;
class TimelineView;
class TimelineTimeAxisView;
class TimelineReelsView;
class TimelineLabelsView;

class Timeline : public wxPanel
{
public:
	Timeline (wxWindow *, ContentPanel *, boost::shared_ptr<Film>);

	boost::shared_ptr<const Film> film () const;

	void force_redraw (dcpomatic::Rect<int> const &);

	int width () const {
		return GetVirtualSize().GetWidth ();
	}

	int track_height () const {
		return _track_height;
	}

	boost::optional<double> pixels_per_second () const {
		return _pixels_per_second;
	}

	int tracks () const;

	void set_snap (bool s) {
		_snap = s;
	}

	bool snap () const {
		return _snap;
	}

	void set_selection (ContentList selection);

	enum Tool {
		SELECT,
		ZOOM
	};

	void set_tool (Tool t) {
		_tool = t;
	}

private:
	void paint_labels ();
	void paint_main ();
	void left_down (wxMouseEvent &);
	void left_down_select (wxMouseEvent &);
	void left_up (wxMouseEvent &);
	void left_up_select (wxMouseEvent &);
	void left_up_zoom (wxMouseEvent &);
	void right_down (wxMouseEvent &);
	void right_down_select (wxMouseEvent &);
	void mouse_moved (wxMouseEvent &);
	void mouse_moved_select (wxMouseEvent &);
	void mouse_moved_zoom (wxMouseEvent &);
	void film_changed (Film::Property);
	void film_content_changed (int, bool frequent);
	void resized ();
	void assign_tracks ();
	void set_position_from_event (wxMouseEvent &);
	void clear_selection ();
	void recreate_views ();
	void setup_scrollbars ();

	boost::shared_ptr<TimelineView> event_to_view (wxMouseEvent &);
	TimelineContentViewList selected_views () const;
	ContentList selected_content () const;
	void maybe_snap (DCPTime a, DCPTime b, boost::optional<DCPTime>& nearest_distance) const;

	wxPanel* _labels_panel;
	wxScrolledCanvas* _main_canvas;
	ContentPanel* _content_panel;
	boost::weak_ptr<Film> _film;
	TimelineViewList _views;
	boost::shared_ptr<TimelineTimeAxisView> _time_axis_view;
	boost::shared_ptr<TimelineReelsView> _reels_view;
	boost::shared_ptr<TimelineLabelsView> _labels_view;
	int _tracks;
	boost::optional<double> _pixels_per_second;
	bool _left_down;
	wxPoint _down_point;
	boost::optional<wxPoint> _zoom_point;
	boost::shared_ptr<TimelineContentView> _down_view;
	DCPTime _down_view_position;
	bool _first_move;
	ContentMenu _menu;
	bool _snap;
	std::list<DCPTime> _start_snaps;
	std::list<DCPTime> _end_snaps;
	Tool _tool;
	int _x_scroll_rate;
	int _y_scroll_rate;
	int _track_height;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _film_content_changed_connection;
};
