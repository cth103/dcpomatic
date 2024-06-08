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


#include "check_box.h"
#include "colours.h"
#include "dcp_timeline.h"
#include "dcp_timeline_reel_marker_view.h"
#include "dcpomatic_choice.h"
#include "dcpomatic_spin_ctrl.h"
#include "id.h"
#include "timecode.h"
#include "wx_util.h"
#include "lib/atmos_content.h"
#include "lib/audio_content.h"
#include "lib/constants.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include <dcp/scope_guard.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


auto constexpr reel_marker_y_pos = 48;
auto constexpr content_y_pos = 112;
auto constexpr content_type_height = 12;

enum {
	ID_add_reel_boundary = DCPOMATIC_DCP_TIMELINE_MENU
};


class ReelBoundary
{
public:
	ReelBoundary(wxWindow* parent, wxGridBagSizer* sizer, int index, DCPTime maximum, int fps, DCPTimeline& timeline, bool editable)
		: _label(new wxStaticText(parent, wxID_ANY, wxString::Format(_("Reel %d to reel %d"), index + 1, index + 2)))
		, _timecode(new Timecode<DCPTime>(parent, true))
		, _index(index)
		, _view(timeline, reel_marker_y_pos)
		, _fps(fps)
	{
		sizer->Add(_label, wxGBPosition(index, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		sizer->Add(_timecode, wxGBPosition(index, 1));

		_timecode->set_maximum(maximum.split(fps));
		_timecode->set_editable(editable);
		_timecode->Changed.connect(boost::bind(&ReelBoundary::timecode_changed, this));
	}

	~ReelBoundary()
	{
		if (_label) {
			_label->Destroy();
		}

		if (_timecode) {
			_timecode->Destroy();
		}
	}

	ReelBoundary(ReelBoundary const&) = delete;
	ReelBoundary& operator=(ReelBoundary const&) = delete;

	ReelBoundary(ReelBoundary&& other) = delete;
	ReelBoundary& operator=(ReelBoundary&& other) = delete;

	void set_time(DCPTime time)
	{
		if (_timecode) {
			_timecode->set(time, _fps);
		}
		_view.set_time(time);
	}

	dcpomatic::DCPTime time() const {
		return _view.time();
	}

	int index() const {
		return _index;
	}

	DCPTimelineReelMarkerView& view() {
		return _view;
	}

	DCPTimelineReelMarkerView const& view() const {
		return _view;
	}

	boost::signals2::signal<void (int, dcpomatic::DCPTime)> Changed;

private:
	void timecode_changed() {
		set_time(_timecode->get(_fps));
		Changed(_index, time());
	}

	wxStaticText* _label = nullptr;
	Timecode<dcpomatic::DCPTime>* _timecode = nullptr;
	int _index = 0;
	DCPTimelineReelMarkerView _view;
	int _fps;
};


DCPTimeline::DCPTimeline(wxWindow* parent, shared_ptr<Film> film)
	: Timeline(parent)
	, _film(film)
	, _canvas(new wxScrolledCanvas(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE))
	, _reel_settings(new wxPanel(this, wxID_ANY))
	, _reel_detail(new wxPanel(this, wxID_ANY))
	, _reel_detail_sizer(new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP))
{
#ifndef __WXOSX__
	_canvas->SetDoubleBuffered(true);
#endif
	_reel_detail->SetSizer(_reel_detail_sizer);

	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(_reel_settings, 0);
	sizer->Add(_canvas, 0, wxEXPAND);
	sizer->Add(_reel_detail, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);
	SetSizer(sizer);

	SetMinSize(wxSize(640, 480));
	_canvas->SetMinSize({-1, content_y_pos + content_type_height * 4});

	_canvas->Bind(wxEVT_PAINT, boost::bind(&DCPTimeline::paint, this));
	_canvas->Bind(wxEVT_SIZE, boost::bind(&DCPTimeline::setup_pixels_per_second, this));
	_canvas->Bind(wxEVT_LEFT_DOWN, boost::bind(&DCPTimeline::left_down, this, _1));
	_canvas->Bind(wxEVT_RIGHT_DOWN, boost::bind(&DCPTimeline::right_down, this, _1));
	_canvas->Bind(wxEVT_LEFT_UP, boost::bind(&DCPTimeline::left_up, this, _1));
	_canvas->Bind(wxEVT_MOTION, boost::bind(&DCPTimeline::mouse_moved, this, _1));

	_film_connection = film->Change.connect(boost::bind(&DCPTimeline::film_changed, this, _1, _2));

	_menu = new wxMenu;
	_add_reel_boundary = _menu->Append(ID_add_reel_boundary, _("Add reel"));
	_canvas->Bind(wxEVT_MENU, boost::bind(&DCPTimeline::add_reel_boundary, this));

	setup_reel_settings();
	setup_reel_boundaries();

	sizer->Layout();
	setup_pixels_per_second();
	setup_sensitivity();
}


void
DCPTimeline::add_reel_boundary()
{
	auto boundaries = film()->custom_reel_boundaries();
	boundaries.push_back(DCPTime::from_seconds(_right_down_position.x / _pixels_per_second.get_value_or(1)));
	film()->set_custom_reel_boundaries(boundaries);
}


void
DCPTimeline::film_changed(ChangeType type, FilmProperty property)
{
	if (type != ChangeType::DONE) {
		return;
	}

	switch (property) {
	case FilmProperty::REEL_TYPE:
	case FilmProperty::REEL_LENGTH:
	case FilmProperty::CUSTOM_REEL_BOUNDARIES:
		setup_sensitivity();
		setup_reel_boundaries();
		break;
	case FilmProperty::CONTENT:
	case FilmProperty::CONTENT_ORDER:
		setup_pixels_per_second();
		Refresh();
		break;
	default:
		break;
	}
}


void
DCPTimeline::setup_sensitivity()
{
	_snap->Enable(editable());
	_maximum_reel_size->Enable(film()->reel_type() == ReelType::BY_LENGTH);
	_add_reel_boundary->Enable(film()->reel_type() == ReelType::CUSTOM);
}


void
DCPTimeline::setup_reel_settings()
{
	auto sizer = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_reel_settings->SetSizer(sizer);

	int r = 0;
	add_label_to_sizer(sizer, _reel_settings, _("Reel mode"), true, wxGBPosition(r, 0));
	_reel_type = new Choice(_reel_settings);
	_reel_type->add_entry(_("Single reel"));
	_reel_type->add_entry(_("Split by video content"));
	_reel_type->add_entry(_("Split by maximum reel size"));
	_reel_type->add_entry(_("Custom"));
	sizer->Add(_reel_type, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(sizer, _reel_settings, _("Maximum reel size"), true, wxGBPosition(r, 0));
	_maximum_reel_size = new SpinCtrl(_reel_settings, DCPOMATIC_SPIN_CTRL_WIDTH);
	_maximum_reel_size->SetRange(1, 1000);
	{
		auto s = new wxBoxSizer(wxHORIZONTAL);
		s->Add(_maximum_reel_size, 0);
		add_label_to_sizer(s, _reel_settings, _("GB"), false, 0, wxALIGN_CENTER_VERTICAL | wxLEFT);
		sizer->Add(s, wxGBPosition(r, 1));
	}
	++r;

	_snap = new CheckBox(_reel_settings, _("Snap when dragging"));
	sizer->Add(_snap, wxGBPosition(r, 1));
	++r;

	_reel_type->set(static_cast<int>(film()->reel_type()));
	_maximum_reel_size->SetValue(film()->reel_length() / 1000000000LL);

	_reel_type->bind(&DCPTimeline::reel_mode_changed, this);
	_maximum_reel_size->Bind(wxEVT_SPINCTRL, boost::bind(&DCPTimeline::maximum_reel_size_changed, this));
}


void
DCPTimeline::reel_mode_changed()
{
	film()->set_reel_type(static_cast<ReelType>(*_reel_type->get()));
}


void
DCPTimeline::maximum_reel_size_changed()
{
	film()->set_reel_length(_maximum_reel_size->GetValue() * 1000000000LL);
}


void
DCPTimeline::set_reel_boundary(int index, DCPTime time)
{
	auto boundaries = film()->custom_reel_boundaries();
	DCPOMATIC_ASSERT(index >= 0 && index < static_cast<int>(boundaries.size()));
	boundaries[index] = time.round(film()->video_frame_rate());
	film()->set_custom_reel_boundaries(boundaries);
}


void
DCPTimeline::setup_reel_boundaries()
{
	auto const reels = film()->reels();
	if (reels.empty()) {
		_reel_boundaries.clear();
		return;
	}

	size_t const boundaries = reels.size() - 1;
	auto const maximum = film()->length();
	for (size_t i = _reel_boundaries.size(); i < boundaries; ++i) {
		auto boundary = std::make_shared<ReelBoundary>(
				_reel_detail, _reel_detail_sizer, i, maximum, film()->video_frame_rate(), *this, editable()
				);

		boundary->Changed.connect(boost::bind(&DCPTimeline::set_reel_boundary, this, _1, _2));
		_reel_boundaries.push_back(boundary);
	}

	_reel_boundaries.resize(boundaries);

	auto const active = editable();
	for (size_t i = 0; i < boundaries; ++i) {
		_reel_boundaries[i]->set_time(reels[i].to);
		_reel_boundaries[i]->view().set_active(active);
	}

	_reel_detail_sizer->Layout();
	_canvas->Refresh();
}


void
DCPTimeline::paint()
{
	wxPaintDC dc(_canvas);
	dc.Clear();

	if (film()->content().empty()) {
		return;
	}

	_canvas->DoPrepareDC(dc);

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	dcp::ScopeGuard sg = [gc]() { delete gc; };

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

	paint_reels(gc);
	paint_content(gc);
}


void
DCPTimeline::paint_reels(wxGraphicsContext* gc)
{
	constexpr int x_offset = 2;

	for (auto const& boundary: _reel_boundaries) {
		boundary->view().paint(gc);
	}

	gc->SetFont(gc->CreateFont(*wxNORMAL_FONT, wxColour(0, 0, 0)));
	gc->SetPen(*wxThePenList->FindOrCreatePen(wxColour(0, 0, 0), 2, wxPENSTYLE_SOLID));

	auto const pps = pixels_per_second().get_value_or(1);

	auto start = gc->CreatePath();
	start.MoveToPoint(x_offset, reel_marker_y_pos);
	start.AddLineToPoint(x_offset, reel_marker_y_pos + DCPTimelineReelMarkerView::HEIGHT);
	gc->StrokePath(start);

	auto const length = film()->length().seconds() * pps;
	auto end = gc->CreatePath();
	end.MoveToPoint(x_offset + length, reel_marker_y_pos);
	end.AddLineToPoint(x_offset + length, reel_marker_y_pos + DCPTimelineReelMarkerView::HEIGHT);
	gc->StrokePath(end);

	auto const y = reel_marker_y_pos + DCPTimelineReelMarkerView::HEIGHT * 3 / 4;

	auto paint_reel = [gc](double from, double to, int index) {
		auto path = gc->CreatePath();
		path.MoveToPoint(from, y);
		path.AddLineToPoint(to, y);
		gc->StrokePath(path);

		auto str = wxString::Format(wxT("#%d"), index + 1);
		wxDouble str_width;
		wxDouble str_height;
		wxDouble str_descent;
		wxDouble str_leading;
		gc->GetTextExtent(str, &str_width, &str_height, &str_descent, &str_leading);

		if (str_width < (to - from)) {
			gc->DrawText(str, (from + to - str_width) / 2, y - str_height - 2);
		}
	};

	gc->SetPen(*wxThePenList->FindOrCreatePen(wxColour(0, 0, 255), 2, wxPENSTYLE_DOT));
	int index = 0;
	DCPTime last;
	for (auto const& boundary: _reel_boundaries) {
		paint_reel(last.seconds() * pps + 2, boundary->time().seconds() * pps, index++);
		last = boundary->time();
	}

	paint_reel(last.seconds() * pps + 2, film()->length().seconds() * pps, index);
}


void
DCPTimeline::paint_content(wxGraphicsContext* gc)
{
	auto const pps = pixels_per_second().get_value_or(1);
	auto const film = this->film();

	auto const& solid_pen = *wxThePenList->FindOrCreatePen(wxColour(0, 0, 0), 1, wxPENSTYLE_SOLID);
	auto const& dotted_pen = *wxThePenList->FindOrCreatePen(wxColour(0, 0, 0), 1, wxPENSTYLE_DOT);

	auto const& video_brush = *wxTheBrushList->FindOrCreateBrush(VIDEO_CONTENT_COLOUR, wxBRUSHSTYLE_SOLID);
	auto const& audio_brush = *wxTheBrushList->FindOrCreateBrush(AUDIO_CONTENT_COLOUR, wxBRUSHSTYLE_SOLID);
	auto const& text_brush = *wxTheBrushList->FindOrCreateBrush(TEXT_CONTENT_COLOUR, wxBRUSHSTYLE_SOLID);
	auto const& atmos_brush = *wxTheBrushList->FindOrCreateBrush(ATMOS_CONTENT_COLOUR, wxBRUSHSTYLE_SOLID);

	auto maybe_draw =
		[gc, film, pps, solid_pen, dotted_pen]
		(shared_ptr<Content> content, shared_ptr<ContentPart> part, wxBrush brush, int offset) {
		if (part) {
			auto const y = content_y_pos + offset * content_type_height + 1;
			gc->SetPen(solid_pen);
			gc->SetBrush(brush);
			gc->DrawRectangle(
				content->position().seconds() * pps,
				y,
				content->length_after_trim(film).seconds() * pps,
				content_type_height - 2
				);

			gc->SetPen(dotted_pen);
			for (auto split: content->reel_split_points(film)) {
				if (split != content->position()) {
					auto path = gc->CreatePath();
					path.MoveToPoint(split.seconds() * pps, y);
					path.AddLineToPoint(split.seconds() * pps, y + content_type_height - 2);
					gc->StrokePath(path);
				}
			}
		}
	};

	for (auto content: film->content()) {
		maybe_draw(content, dynamic_pointer_cast<ContentPart>(content->video), video_brush, 0);
		maybe_draw(content, dynamic_pointer_cast<ContentPart>(content->audio), audio_brush, 1);
		for (auto text: content->text) {
			maybe_draw(content, dynamic_pointer_cast<ContentPart>(text), text_brush, 2);
		}
		maybe_draw(content, dynamic_pointer_cast<ContentPart>(content->atmos), atmos_brush, 3);
	}
}


void
DCPTimeline::setup_pixels_per_second()
{
	set_pixels_per_second((_canvas->GetSize().GetWidth() - 4) / std::max(1.0, film()->length().seconds()));
}


shared_ptr<ReelBoundary>
DCPTimeline::event_to_reel_boundary(wxMouseEvent& ev) const
{
	Position<int> const position(ev.GetX(), ev.GetY());
	auto iter = std::find_if(_reel_boundaries.begin(), _reel_boundaries.end(), [position](shared_ptr<const ReelBoundary> boundary) {
		return boundary->view().bbox().contains(position);
	});

	if (iter == _reel_boundaries.end()) {
		return {};
	}

	return *iter;
}


void
DCPTimeline::left_down(wxMouseEvent& ev)
{
	if (!editable()) {
		return;
	}

	if (auto boundary = event_to_reel_boundary(ev)) {
		auto const snap_distance = DCPTime::from_seconds((_canvas->GetSize().GetWidth() / _pixels_per_second.get_value_or(1)) / SNAP_SUBDIVISION);
		_drag = DCPTimeline::Drag(
			boundary,
			_reel_boundaries,
			film(),
			static_cast<int>(ev.GetX() - boundary->time().seconds() * _pixels_per_second.get_value_or(0)),
			_snap->get(),
			snap_distance
			);
	} else {
		_drag = boost::none;
	}
}


void
DCPTimeline::right_down(wxMouseEvent& ev)
{
	_right_down_position = ev.GetPosition();
	_canvas->PopupMenu(_menu, _right_down_position);
}


void
DCPTimeline::left_up(wxMouseEvent&)
{
	if (!_drag) {
		return;
	}

	set_reel_boundary(_drag->reel_boundary->index(), _drag->time());
	_drag = boost::none;
}


void
DCPTimeline::mouse_moved(wxMouseEvent& ev)
{
	if (!_drag) {
		return;
	}

	auto time = DCPTime::from_seconds((ev.GetPosition().x - _drag->offset) / _pixels_per_second.get_value_or(1));
	time = std::max(_drag->previous ? _drag->previous->time() : DCPTime(), time);
	time = std::min(_drag->next ? _drag->next->time() : film()->length(), time);
	_drag->set_time(time);
	_canvas->RefreshRect({0, reel_marker_y_pos - 2, _canvas->GetSize().GetWidth(), DCPTimelineReelMarkerView::HEIGHT + 4}, true);
}


void
DCPTimeline::force_redraw(dcpomatic::Rect<int> const & r)
{
	_canvas->RefreshRect(wxRect(r.x, r.y, r.width, r.height), false);
}


shared_ptr<Film>
DCPTimeline::film() const
{
	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);
	return film;
}


bool
DCPTimeline::editable() const
{
	return film()->reel_type() == ReelType::CUSTOM;
}


DCPTimeline::Drag::Drag(
	shared_ptr<ReelBoundary> reel_boundary_,
	vector<shared_ptr<ReelBoundary>> const& reel_boundaries,
	shared_ptr<const Film> film,
	int offset_,
	bool snap,
	DCPTime snap_distance
	)
	: reel_boundary(reel_boundary_)
	, offset(offset_)
	, _snap_distance(snap_distance)
{
	auto iter = std::find(reel_boundaries.begin(), reel_boundaries.end(), reel_boundary);
	auto index = std::distance(reel_boundaries.begin(), iter);

	if (index > 0) {
		previous = reel_boundaries[index - 1];
	}
	if (index < static_cast<int>(reel_boundaries.size() - 1)) {
		next = reel_boundaries[index + 1];
	}

	if (snap) {
		for (auto content: film->content()) {
			for (auto split: content->reel_split_points(film)) {
				_snaps.push_back(split);
			}
		}
	}
}


void
DCPTimeline::Drag::set_time(DCPTime time)
{
	optional<DCPTime> nearest_distance;
	optional<DCPTime> nearest_time;
	for (auto snap: _snaps) {
		auto const distance = time - snap;
		if (!nearest_distance || distance.abs() < nearest_distance->abs()) {
			nearest_distance = distance.abs();
			nearest_time = snap;
		}
	}

	if (nearest_distance && *nearest_distance < _snap_distance) {
		reel_boundary->set_time(*nearest_time);
	} else {
		reel_boundary->set_time(time);
	}
}


DCPTime
DCPTimeline::Drag::time() const
{
	return reel_boundary->time();
}

