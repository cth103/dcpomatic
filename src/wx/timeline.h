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


#include "content_menu.h"
#include "timeline_content_view.h"
#include "lib/film.h"
#include "lib/rect.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class ContentPanel;
class Film;
class FilmViewer;
class TimelineLabelsView;
class TimelineReelsView;
class TimelineTimeAxisView;
class TimelineView;


class Timeline : public wxPanel
{
public:
	Timeline (wxWindow *, ContentPanel *, std::shared_ptr<Film>, FilmViewer& viewer);

	std::shared_ptr<const Film> film () const;

	void force_redraw (dcpomatic::Rect<int> const &);

	int width () const;

	int pixels_per_track () const {
		return _pixels_per_track;
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
		ZOOM,
		ZOOM_ALL,
		SNAP,
		SEQUENCE
	};

	void tool_clicked (Tool t);

	int tracks_y_offset () const;

	void keypress(wxKeyEvent const &);

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
	void film_change (ChangeType type, Film::Property);
	void film_content_change (ChangeType type, int, bool frequent);
	void resized ();
	void assign_tracks ();
	void set_position_from_event (wxMouseEvent& ev, bool force_emit = false);
	void clear_selection ();
	void recreate_views ();
	void setup_scrollbars ();
	void scrolled (wxScrollWinEvent& ev);
	void set_pixels_per_second (double pps);
	void set_pixels_per_track (int h);
	void zoom_all ();
	void update_playhead ();

	std::shared_ptr<TimelineView> event_to_view (wxMouseEvent &);
	TimelineContentViewList selected_views () const;
	ContentList selected_content () const;
	void maybe_snap (dcpomatic::DCPTime a, dcpomatic::DCPTime b, boost::optional<dcpomatic::DCPTime>& nearest_distance) const;

	wxScrolledCanvas* _labels_canvas;
	wxScrolledCanvas* _main_canvas;
	ContentPanel* _content_panel;
	std::weak_ptr<Film> _film;
	FilmViewer& _viewer;
	TimelineViewList _views;
	std::shared_ptr<TimelineTimeAxisView> _time_axis_view;
	std::shared_ptr<TimelineReelsView> _reels_view;
	std::shared_ptr<TimelineLabelsView> _labels_view;
	int _tracks;
	boost::optional<double> _pixels_per_second;
	bool _left_down;
	wxPoint _down_point;
	boost::optional<wxPoint> _zoom_point;
	std::shared_ptr<TimelineContentView> _down_view;
	dcpomatic::DCPTime _down_view_position;
	bool _first_move;
	ContentMenu _menu;
	bool _snap;
	std::list<dcpomatic::DCPTime> _start_snaps;
	std::list<dcpomatic::DCPTime> _end_snaps;
	Tool _tool;
	int _x_scroll_rate;
	int _y_scroll_rate;
	int _pixels_per_track;
	bool _first_resize;
	wxTimer _timer;

	static double const _minimum_pixels_per_second;
	static int const _minimum_pixels_per_track;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _film_content_change_connection;
};
