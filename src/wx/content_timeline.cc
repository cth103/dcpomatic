/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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

#include "content_panel.h"
#include "content_timeline.h"
#include "film_editor.h"
#include "film_viewer.h"
#include "content_timeline_atmos_view.h"
#include "content_timeline_audio_view.h"
#include "content_timeline_text_view.h"
#include "content_timeline_video_view.h"
#include "timeline_labels_view.h"
#include "timeline_reels_view.h"
#include "timeline_time_axis_view.h"
#include "wx_util.h"
#include "lib/atmos_mxf_content.h"
#include "lib/audio_content.h"
#include "lib/constants.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/playlist.h"
#include "lib/text_content.h"
#include "lib/timer.h"
#include "lib/video_content.h"
#include <dcp/scope_guard.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS
#include <iterator>
#include <list>


using std::abs;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::max;
using std::min;
using std::shared_ptr;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


int const ContentTimeline::_minimum_pixels_per_track = 16;


ContentTimeline::ContentTimeline(wxWindow* parent, ContentPanel* cp, shared_ptr<Film> film, FilmViewer& viewer)
	: Timeline(parent)
	, _labels_canvas(new wxScrolledCanvas(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE))
	, _main_canvas(new wxScrolledCanvas(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE))
	, _content_panel(cp)
	, _film(film)
	, _viewer(viewer)
	, _time_axis_view(new TimelineTimeAxisView(*this, 64))
	, _reels_view(new TimelineReelsView(*this, 32))
	, _labels_view(new TimelineLabelsView(*this))
	, _tracks(0)
	, _left_down(false)
	, _down_view_position(0)
	, _first_move(false)
	, _menu(this, viewer)
	, _snap(true)
	, _tool(SELECT)
	, _x_scroll_rate(16)
	, _y_scroll_rate(16)
	, _pixels_per_track(48)
	, _first_resize(true)
	, _timer(this)
{
#ifndef __WXOSX__
	_labels_canvas->SetDoubleBuffered(true);
	_main_canvas->SetDoubleBuffered(true);
#endif

	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(_labels_canvas, 0, wxEXPAND);
	_labels_canvas->SetMinSize(wxSize(_labels_view->bbox().width, -1));
	sizer->Add(_main_canvas, 1, wxEXPAND);
	SetSizer(sizer);

	_labels_canvas->Bind(wxEVT_PAINT,      boost::bind(&ContentTimeline::paint_labels, this));
	_main_canvas->Bind(wxEVT_PAINT,      boost::bind(&ContentTimeline::paint_main,   this));
	_main_canvas->Bind(wxEVT_LEFT_DOWN,  boost::bind(&ContentTimeline::left_down,    this, _1));
	_main_canvas->Bind(wxEVT_LEFT_UP,    boost::bind(&ContentTimeline::left_up,      this, _1));
	_main_canvas->Bind(wxEVT_RIGHT_DOWN, boost::bind(&ContentTimeline::right_down,   this, _1));
	_main_canvas->Bind(wxEVT_MOTION,     boost::bind(&ContentTimeline::mouse_moved,  this, _1));
	_main_canvas->Bind(wxEVT_SIZE,       boost::bind(&ContentTimeline::resized,      this));
	_main_canvas->Bind(wxEVT_MOUSEWHEEL, boost::bind(&ContentTimeline::mouse_wheel_turned, this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_TOP,        boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_BOTTOM,     boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_LINEUP,     boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_LINEDOWN,   boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_PAGEUP,     boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_PAGEDOWN,   boost::bind(&ContentTimeline::scrolled,     this, _1));
	_main_canvas->Bind(wxEVT_SCROLLWIN_THUMBTRACK, boost::bind(&ContentTimeline::scrolled,     this, _1));

	film_change(ChangeType::DONE, FilmProperty::CONTENT);

	SetMinSize(wxSize(640, 4 * pixels_per_track() + 96));

	_film_changed_connection = film->Change.connect(bind(&ContentTimeline::film_change, this, _1, _2));
	_film_content_change_connection = film->ContentChange.connect(bind(&ContentTimeline::film_content_change, this, _1, _2, _3));

	Bind(wxEVT_TIMER, boost::bind(&ContentTimeline::update_playhead, this));
	_timer.Start(200, wxTIMER_CONTINUOUS);

	setup_scrollbars();
	_labels_canvas->ShowScrollbars(wxSHOW_SB_NEVER, wxSHOW_SB_NEVER);
}


void
ContentTimeline::mouse_wheel_turned(wxMouseEvent& event)
{
	auto const rotation = event.GetWheelRotation();

	if (event.ControlDown()) {
		/* On my mouse one click of the scroll wheel is 120, and it's -ve when
		 * scrolling the wheel towards me.
		 */
		auto const scale = rotation > 0 ?
			(1.0 / (rotation / 90.0)) :
			(-rotation / 90.0);

		int before_start_x;
		int before_start_y;
		_main_canvas->GetViewStart(&before_start_x, &before_start_y);

		auto const before_pps = _pixels_per_second.get_value_or(1);
		auto const before_pos = _last_mouse_wheel_x && *_last_mouse_wheel_x == event.GetX() ?
			*_last_mouse_wheel_time :
			(before_start_x * _x_scroll_rate + event.GetX()) / before_pps;

		set_pixels_per_second(before_pps * scale);
		setup_scrollbars();

		auto after_left = std::max(0.0, before_pos * _pixels_per_second.get_value_or(1) - event.GetX());
		_main_canvas->Scroll(after_left / _x_scroll_rate, before_start_y);
		_labels_canvas->Scroll(0, before_start_y);
		Refresh();

		if (!_last_mouse_wheel_x || *_last_mouse_wheel_x != event.GetX()) {
			_last_mouse_wheel_x = event.GetX();
			_last_mouse_wheel_time = before_pos;
		}
	} else if (event.ShiftDown()) {
		int before_start_x;
		int before_start_y;
		_main_canvas->GetViewStart(&before_start_x, &before_start_y);
		auto const width = _main_canvas->GetSize().GetWidth();
		_main_canvas->Scroll(std::max(0.0, before_start_x - rotation * 100.0 / width), before_start_y);
	}
}


void
ContentTimeline::update_playhead()
{
	Refresh();
}


void
ContentTimeline::paint_labels()
{
	wxPaintDC dc(_labels_canvas);

	auto film = _film.lock();
	if (film->content().empty()) {
		return;
	}

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	dcp::ScopeGuard sg = [gc]() { delete gc; };

	int vsx, vsy;
	_labels_canvas->GetViewStart(&vsx, &vsy);
	gc->Translate(-vsx * _x_scroll_rate, -vsy * _y_scroll_rate + tracks_y_offset());

	_labels_view->paint(gc, {});
}


void
ContentTimeline::paint_main()
{
	wxPaintDC dc(_main_canvas);
	dc.Clear();

	auto film = _film.lock();
	if (film->content().empty()) {
		return;
	}

	_main_canvas->DoPrepareDC(dc);

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	dcp::ScopeGuard sg = [gc]() { delete gc; };

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

	for (auto i: _views) {

		auto ic = dynamic_pointer_cast<TimelineContentView>(i);

		/* Find areas of overlap with other content views, so that we can plot them */
		list<dcpomatic::Rect<int>> overlaps;
		for (auto j: _views) {
			auto jc = dynamic_pointer_cast<TimelineContentView>(j);
			/* No overlap with non-content views, views on different tracks, audio views or non-active views */
			if (!ic || !jc || i == j || ic->track() != jc->track() || ic->track().get_value_or(2) >= 2 || !ic->active() || !jc->active()) {
				continue;
			}

			auto r = j->bbox().intersection(i->bbox());
			if (r) {
				overlaps.push_back(r.get());
			}
		}

		i->paint(gc, overlaps);
	}

	if (_zoom_point) {
		gc->SetPen(gui_is_dark() ? *wxWHITE_PEN : *wxBLACK_PEN);
		gc->SetBrush(*wxTRANSPARENT_BRUSH);
		gc->DrawRectangle(
			min(_down_point.x, _zoom_point->x),
			min(_down_point.y, _zoom_point->y),
			abs(_down_point.x - _zoom_point->x),
			abs(_down_point.y - _zoom_point->y)
			);
	}

	/* Playhead */

	gc->SetPen(*wxRED_PEN);
	auto path = gc->CreatePath();
	double const ph = _viewer.position().seconds() * pixels_per_second().get_value_or(0);
	path.MoveToPoint(ph, 0);
	path.AddLineToPoint(ph, pixels_per_track() * _tracks + 32);
	gc->StrokePath(path);
}


void
ContentTimeline::film_change(ChangeType type, FilmProperty p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (p == FilmProperty::CONTENT || p == FilmProperty::REEL_TYPE || p == FilmProperty::REEL_LENGTH) {
		ensure_ui_thread();
		recreate_views();
	} else if (p == FilmProperty::CONTENT_ORDER) {
		Refresh();
	}
}


void
ContentTimeline::recreate_views()
{
	auto film = _film.lock();
	if (!film) {
		return;
	}

	_views.clear();
	_views.push_back(_time_axis_view);
	_views.push_back(_reels_view);

	for (auto i: film->content()) {
		if (i->video) {
			_views.push_back(make_shared<ContentTimelineVideoView>(*this, i));
		}

		if (i->has_mapped_audio()) {
			_views.push_back(make_shared<ContentTimelineAudioView>(*this, i));
		}

		for (auto j: i->text) {
			_views.push_back(make_shared<ContentTimelineTextView>(*this, i, j));
		}

		if (i->atmos) {
			_views.push_back(make_shared<ContentTimelineAtmosView>(*this, i));
		}
	}

	assign_tracks();
	setup_scrollbars();
	Refresh();
}


void
ContentTimeline::film_content_change(ChangeType type, int property, bool frequent)
{
	if (type != ChangeType::DONE) {
		return;
	}

	ensure_ui_thread();

	switch (property) {
	case AudioContentProperty::STREAMS:
	case VideoContentProperty::FRAME_TYPE:
		recreate_views();
		break;
	case ContentProperty::POSITION:
	case ContentProperty::LENGTH:
		_reels_view->force_redraw();
		break;
	default:
		if (!frequent) {
			setup_scrollbars();
			Refresh();
		}
		break;
	}
}


template <class T>
int
place(shared_ptr<const Film> film, ContentTimelineViewList& views, int& tracks)
{
	int const base = tracks;

	for (auto i: views) {
		if (!dynamic_pointer_cast<T>(i)) {
			continue;
		}

		auto cv = dynamic_pointer_cast<TimelineContentView>(i);
		DCPOMATIC_ASSERT(cv);

		int t = base;

		auto content = cv->content();
		DCPTimePeriod const content_period = content->period(film);

		while (true) {
			auto j = views.begin();
			while (j != views.end()) {
				auto test = dynamic_pointer_cast<T>(*j);
				if (!test) {
					++j;
					continue;
				}

				auto test_content = test->content();
				if (
					test->track() && test->track().get() == t &&
					content_period.overlap(test_content->period(film))
				   ) {
					/* we have an overlap on track `t' */
					++t;
					break;
				}

				++j;
			}

			if (j == views.end()) {
				/* no overlap on `t' */
				break;
			}
		}

		cv->set_track(t);
		tracks = max(tracks, t + 1);
	}

	return tracks - base;
}


/** Compare the mapped output channels of two TimelineViews, so we can into
 *  order of first mapped DCP channel.
 */
struct AudioMappingComparator {
	bool operator()(shared_ptr<ContentTimelineView> a, shared_ptr<ContentTimelineView> b) {
		int la = -1;
		auto cva = dynamic_pointer_cast<ContentTimelineAudioView>(a);
		if (cva) {
			auto oc = cva->content()->audio->mapping().mapped_output_channels();
			la = *min_element(boost::begin(oc), boost::end(oc));
		}
		int lb = -1;
		auto cvb = dynamic_pointer_cast<ContentTimelineAudioView>(b);
		if (cvb) {
			auto oc = cvb->content()->audio->mapping().mapped_output_channels();
			lb = *min_element(boost::begin(oc), boost::end(oc));
		}
		return la < lb;
	}
};


void
ContentTimeline::assign_tracks()
{
	/* Tracks are:
	   Video 1
	   Video 2
	   Video N
	   Text 1
	   Text 2
	   Text N
	   Atmos
	   Audio 1
	   Audio 2
	   Audio N
	*/

	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	_tracks = 0;

	for (auto i: _views) {
		auto c = dynamic_pointer_cast<TimelineContentView>(i);
		if (c) {
			c->unset_track();
		}
	}

	int const video_tracks = place<ContentTimelineVideoView>(film, _views, _tracks);
	int const text_tracks = place<ContentTimelineTextView>(film, _views, _tracks);

	/* Atmos */

	bool have_atmos = false;
	for (auto i: _views) {
		auto cv = dynamic_pointer_cast<ContentTimelineAtmosView>(i);
		if (cv) {
			cv->set_track(_tracks);
			have_atmos = true;
		}
	}

	if (have_atmos) {
		++_tracks;
	}

	/* Audio.  We're sorting the views so that we get the audio views in order of increasing
	   DCP channel index.
	*/

	auto views = _views;
	sort(views.begin(), views.end(), AudioMappingComparator());
	int const audio_tracks = place<ContentTimelineAudioView>(film, views, _tracks);

	_labels_view->set_video_tracks(video_tracks);
	_labels_view->set_audio_tracks(audio_tracks);
	_labels_view->set_text_tracks(text_tracks);
	_labels_view->set_atmos(have_atmos);

	_time_axis_view->set_y(tracks());
	_reels_view->set_y(8);
}


int
ContentTimeline::tracks() const
{
	return _tracks;
}


void
ContentTimeline::setup_scrollbars()
{
	auto film = _film.lock();
	if (!film || !_pixels_per_second) {
		return;
	}

	int const h = tracks() * pixels_per_track() + tracks_y_offset() + _time_axis_view->bbox().height;

	_labels_canvas->SetVirtualSize(_labels_view->bbox().width, h);
	_labels_canvas->SetScrollRate(_x_scroll_rate, _y_scroll_rate);
	_main_canvas->SetVirtualSize(*_pixels_per_second * film->length().seconds(), h);
	_main_canvas->SetScrollRate(_x_scroll_rate, _y_scroll_rate);
}


shared_ptr<ContentTimelineView>
ContentTimeline::event_to_view(wxMouseEvent& ev)
{
	/* Search backwards through views so that we find the uppermost one first */
	auto i = _views.rbegin();

	int vsx, vsy;
	_main_canvas->GetViewStart(&vsx, &vsy);
	Position<int> const p(ev.GetX() + vsx * _x_scroll_rate, ev.GetY() + vsy * _y_scroll_rate);

	while (i != _views.rend() && !(*i)->bbox().contains(p)) {
		++i;
	}

	if (i == _views.rend()) {
		return {};
	}

	return *i;
}


void
ContentTimeline::left_down(wxMouseEvent& ev)
{
	_left_down = true;
	_down_point = ev.GetPosition();

	switch (_tool) {
	case SELECT:
		left_down_select(ev);
		break;
	case ZOOM:
	case ZOOM_ALL:
	case SNAP:
	case SEQUENCE:
		/* Nothing to do */
		break;
	}
}


void
ContentTimeline::left_down_select(wxMouseEvent& ev)
{
	auto view = event_to_view(ev);
	auto content_view = dynamic_pointer_cast<TimelineContentView>(view);

	_down_view.reset();
	_first_move = false;

	if (dynamic_pointer_cast<TimelineTimeAxisView>(view)) {
		/* Seek when clicking in the time axis */
		int vsx, vsy;
		_main_canvas->GetViewStart(&vsx, &vsy);
		_viewer.seek(DCPTime::from_seconds((ev.GetPosition().x + vsx * _x_scroll_rate) / _pixels_per_second.get_value_or(1)), true);
	}

	if (!content_view) {
		/* Click outside all content selects none */
		for (auto i: _views) {
			if (auto cv = dynamic_pointer_cast<TimelineContentView>(i)) {
				cv->set_selected(false);
			}
		}
		return;
	}

	_down_view = content_view;
	_down_view_position = content_view->content()->position();

	if (ev.ShiftDown()) {
		/* Toggle */
		content_view->set_selected(!content_view->selected());
	} else if (!content_view->selected()) {
		/* Select one */
		for (auto i: _views) {
			if (auto cv = dynamic_pointer_cast<TimelineContentView>(i)) {
				cv->set_selected(view == i);
			}
		}
	}

	/* Pre-compute the points that we might snap to */
	for (auto i: _views) {
		auto cv = dynamic_pointer_cast<TimelineContentView>(i);
		if (!cv || cv == _down_view || cv->content() == _down_view->content()) {
			continue;
		}

		auto film = _film.lock();
		DCPOMATIC_ASSERT(film);

		_start_snaps.push_back(cv->content()->position());
		_end_snaps.push_back(cv->content()->position());
		_start_snaps.push_back(cv->content()->end(film));
		_end_snaps.push_back(cv->content()->end(film));

		for (auto i: cv->content()->reel_split_points(film)) {
			_start_snaps.push_back(i);
		}
	}

	/* Tell everyone that things might change frequently during the drag */
	_down_view->content()->set_change_signals_frequent(true);
}


void
ContentTimeline::left_up(wxMouseEvent& ev)
{
	_left_down = false;

	switch (_tool) {
	case SELECT:
		left_up_select(ev);
		break;
	case ZOOM:
		left_up_zoom(ev);
		break;
	case ZOOM_ALL:
	case SNAP:
	case SEQUENCE:
		break;
	}
}


void
ContentTimeline::left_up_select(wxMouseEvent& ev)
{
	if (_down_view) {
		_down_view->content()->set_change_signals_frequent(false);
	}

	_content_panel->set_selection(selected_content());
	/* Since we may have just set change signals back to `not-frequent', we have to
	   make sure this position change is signalled, even if the position value has
	   not changed since the last time it was set (with frequent=true).  This is
	   a bit of a hack.
	*/
	set_position_from_event(ev, true);

	/* Clear up up the stuff we don't do during drag */
	assign_tracks();
	setup_scrollbars();
	Refresh();

	_start_snaps.clear();
	_end_snaps.clear();
}


void
ContentTimeline::left_up_zoom(wxMouseEvent& ev)
{
	_zoom_point = ev.GetPosition();

	int vsx, vsy;
	_main_canvas->GetViewStart(&vsx, &vsy);

	wxPoint top_left(min(_down_point.x, _zoom_point->x), min(_down_point.y, _zoom_point->y));
	wxPoint bottom_right(max(_down_point.x, _zoom_point->x), max(_down_point.y, _zoom_point->y));

	if ((bottom_right.x - top_left.x) < 8 || (bottom_right.y - top_left.y) < 8) {
		/* Very small zoom rectangle: we assume it wasn't intentional */
		_zoom_point = optional<wxPoint>();
		Refresh();
		return;
	}

	auto const time_left = DCPTime::from_seconds((top_left.x + vsx) / *_pixels_per_second);
	auto const time_right = DCPTime::from_seconds((bottom_right.x + vsx) / *_pixels_per_second);
	set_pixels_per_second(double(GetSize().GetWidth()) / (time_right.seconds() - time_left.seconds()));

	double const tracks_top = double(top_left.y - tracks_y_offset()) / _pixels_per_track;
	double const tracks_bottom = double(bottom_right.y - tracks_y_offset()) / _pixels_per_track;
	set_pixels_per_track(lrint(GetSize().GetHeight() / (tracks_bottom - tracks_top)));

	setup_scrollbars();
	int const y = (tracks_top * _pixels_per_track + tracks_y_offset()) / _y_scroll_rate;
	_main_canvas->Scroll(time_left.seconds() * *_pixels_per_second / _x_scroll_rate, y);
	_labels_canvas->Scroll(0, y);

	_zoom_point = optional<wxPoint>();
	Refresh();
}


void
ContentTimeline::set_pixels_per_track(int h)
{
	_pixels_per_track = max(_minimum_pixels_per_track, h);
}


void
ContentTimeline::mouse_moved(wxMouseEvent& ev)
{
	switch (_tool) {
	case SELECT:
		mouse_moved_select(ev);
		break;
	case ZOOM:
		mouse_moved_zoom(ev);
		break;
	case ZOOM_ALL:
	case SNAP:
	case SEQUENCE:
		break;
	}
}


void
ContentTimeline::mouse_moved_select(wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	set_position_from_event(ev);
}


void
ContentTimeline::mouse_moved_zoom(wxMouseEvent& ev)
{
	if (!_left_down) {
		return;
	}

	_zoom_point = ev.GetPosition();
	setup_scrollbars();
	Refresh();
}


void
ContentTimeline::right_down(wxMouseEvent& ev)
{
	switch (_tool) {
	case SELECT:
		right_down_select(ev);
		break;
	case ZOOM:
		/* Zoom out */
		set_pixels_per_second(*_pixels_per_second / 2);
		set_pixels_per_track(_pixels_per_track / 2);
		setup_scrollbars();
		Refresh();
		break;
	case ZOOM_ALL:
	case SNAP:
	case SEQUENCE:
		break;
	}
}


void
ContentTimeline::right_down_select(wxMouseEvent& ev)
{
	auto view = event_to_view(ev);
	auto cv = dynamic_pointer_cast<TimelineContentView>(view);
	if (!cv) {
		return;
	}

	if (!cv->selected()) {
		clear_selection();
		cv->set_selected(true);
	}

	_menu.popup(_film, selected_content(), selected_views(), ev.GetPosition());
}


void
ContentTimeline::maybe_snap(DCPTime a, DCPTime b, optional<DCPTime>& nearest_distance) const
{
	auto const d = a - b;
	if (!nearest_distance || d.abs() < nearest_distance.get().abs()) {
		nearest_distance = d;
	}
}


void
ContentTimeline::set_position_from_event(wxMouseEvent& ev, bool force_emit)
{
	if (!_pixels_per_second) {
		return;
	}

	double const pps = _pixels_per_second.get();

	auto const p = ev.GetPosition();

	if (!_first_move) {
		/* We haven't moved yet; in that case, we must move the mouse some reasonable distance
		   before the drag is considered to have started.
		*/
		int const dist = sqrt(pow(p.x - _down_point.x, 2) + pow(p.y - _down_point.y, 2));
		if (dist < 8) {
			return;
		}
		_first_move = true;
	}

	if (!_down_view) {
		return;
	}

	auto new_position = _down_view_position + DCPTime::from_seconds((p.x - _down_point.x) / pps);

	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);

	if (_snap) {
		auto const new_end = new_position + _down_view->content()->length_after_trim(film);
		/* Signed `distance' to nearest thing (i.e. negative is left on the timeline,
		   positive is right).
		*/
		optional<DCPTime> nearest_distance;

		/* Find the nearest snap point */

		for (auto i: _start_snaps) {
			maybe_snap(i, new_position, nearest_distance);
		}

		for (auto i: _end_snaps) {
			maybe_snap(i, new_end, nearest_distance);
		}

		if (nearest_distance) {
			/* Snap if it's close; `close' means within a proportion of the time on the timeline */
			if (nearest_distance.get().abs() < DCPTime::from_seconds((width() / pps) / SNAP_SUBDIVISION)) {
				new_position += nearest_distance.get();
			}
		}
	}

	if (new_position < DCPTime()) {
		new_position = DCPTime();
	}

	_down_view->content()->set_position(film, new_position, force_emit);

	film->set_sequence(false);
}


void
ContentTimeline::force_redraw(dcpomatic::Rect<int> const & r)
{
	_main_canvas->RefreshRect(wxRect(r.x, r.y, r.width, r.height), false);
}


shared_ptr<const Film>
ContentTimeline::film() const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);
	return film;
}


void
ContentTimeline::resized()
{
	if (_main_canvas->GetSize().GetWidth() > 0 && _first_resize) {
		zoom_all();
		_first_resize = false;
	}
	setup_scrollbars();
}


void
ContentTimeline::clear_selection()
{
	for (auto i: _views) {
		shared_ptr<TimelineContentView> cv = dynamic_pointer_cast<TimelineContentView>(i);
		if (cv) {
			cv->set_selected(false);
		}
	}
}


TimelineContentViewList
ContentTimeline::selected_views() const
{
	TimelineContentViewList sel;

	for (auto i: _views) {
		auto cv = dynamic_pointer_cast<TimelineContentView>(i);
		if (cv && cv->selected()) {
			sel.push_back(cv);
		}
	}

	return sel;
}


ContentList
ContentTimeline::selected_content() const
{
	ContentList sel;

	for (auto i: selected_views()) {
		sel.push_back(i->content());
	}

	return sel;
}


void
ContentTimeline::set_selection(ContentList selection)
{
	for (auto i: _views) {
		auto cv = dynamic_pointer_cast<TimelineContentView>(i);
		if (cv) {
			cv->set_selected(find(selection.begin(), selection.end(), cv->content()) != selection.end());
		}
	}
}


int
ContentTimeline::tracks_y_offset() const
{
	return _reels_view->bbox().height + 4;
}


int
ContentTimeline::width() const
{
	return _main_canvas->GetVirtualSize().GetWidth();
}


void
ContentTimeline::scrolled(wxScrollWinEvent& ev)
{
	if (ev.GetOrientation() == wxVERTICAL) {
		int x, y;
		_main_canvas->GetViewStart(&x, &y);
		_labels_canvas->Scroll(0, y);
	}
	ev.Skip();
}


void
ContentTimeline::tool_clicked(Tool t)
{
	switch (t) {
	case ZOOM:
	case SELECT:
		_tool = t;
		break;
	case ZOOM_ALL:
		zoom_all();
		break;
	case SNAP:
	case SEQUENCE:
		break;
	}
}


void
ContentTimeline::zoom_all()
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);
	set_pixels_per_second((_main_canvas->GetSize().GetWidth() - 32) / std::max(1.0, film->length().seconds()));
	set_pixels_per_track((_main_canvas->GetSize().GetHeight() - tracks_y_offset() - _time_axis_view->bbox().height - 32) / std::max(1, _tracks));
	setup_scrollbars();
	_main_canvas->Scroll(0, 0);
	_labels_canvas->Scroll(0, 0);
	Refresh();
}


void
ContentTimeline::keypress(wxKeyEvent const& event)
{
	if (event.GetKeyCode() == WXK_DELETE) {
		auto film = _film.lock();
		DCPOMATIC_ASSERT(film);
		film->remove_content(selected_content());
	} else {
		switch (event.GetRawKeyCode()) {
		case '+':
			set_pixels_per_second(_pixels_per_second.get_value_or(1) * 2);
			setup_scrollbars();
			break;
		case '-':
			set_pixels_per_second(_pixels_per_second.get_value_or(1) / 2);
			setup_scrollbars();
			break;
		}
	}
}

