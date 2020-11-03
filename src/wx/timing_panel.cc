/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "timing_panel.h"
#include "wx_util.h"
#include "film_viewer.h"
#include "timecode.h"
#include "content_panel.h"
#include "move_to_dialog.h"
#include "static_text.h"
#include "dcpomatic_button.h"
#include "lib/content.h"
#include "lib/image_content.h"
#include "lib/text_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/audio_content.h"
#include "lib/string_text_file_content.h"
#include "lib/video_content.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include <dcp/locale_convert.h>
#include "lib/warnings.h"
#if defined(__WXGTK20__) && !defined(__WXGTK3__)
#define TIMING_PANEL_ALIGNMENT_HACK 1
DCPOMATIC_DISABLE_WARNINGS
#include <gtk/gtk.h>
DCPOMATIC_ENABLE_WARNINGS
#endif
#include <boost/foreach.hpp>
#include <set>
#include <iostream>

using std::cout;
using std::string;
using std::set;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;
using namespace dcpomatic;

TimingPanel::TimingPanel (ContentPanel* p, weak_ptr<FilmViewer> viewer)
	/* horrid hack for apparent lack of context support with wxWidgets i18n code */
	/// TRANSLATORS: translate the word "Timing" here; do not include the "Timing|" prefix
	: ContentSubPanel (p, S_("Timing|Timing"))
	, _viewer (viewer)
	, _film_content_changed_suspender (boost::bind(&TimingPanel::film_content_changed, this, _1))
{
	wxSize size = TimecodeBase::size (this);

	for (int i = 0; i < 3; ++i) {
		_colon[i] = create_label (this, wxT(":"), false);
	}

	//// TRANSLATORS: this is an abbreviation for "hours"
	_h_label = new StaticText (this, _("h"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef TIMING_PANEL_ALIGNMENT_HACK
	/* Hack to work around failure to centre text on GTK */
	gtk_label_set_line_wrap (GTK_LABEL(_h_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "minutes"
	_m_label = new StaticText (this, _("m"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef TIMING_PANEL_ALIGNMENT_HACK
	gtk_label_set_line_wrap (GTK_LABEL (_m_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "seconds"
	_s_label = new StaticText (this, _("s"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef TIMING_PANEL_ALIGNMENT_HACK
	gtk_label_set_line_wrap (GTK_LABEL(_s_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "frames"
	_f_label = new StaticText (this, _("f"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef TIMING_PANEL_ALIGNMENT_HACK
	gtk_label_set_line_wrap (GTK_LABEL(_f_label->GetHandle()), FALSE);
#endif

	_position_label = create_label (this, _("Position"), true);
	_position = new Timecode<DCPTime> (this);
	_move_to_start_of_reel = new Button (this, _("Move to start of reel"));
	_full_length_label = create_label (this, _("Full length"), true);
	_full_length = new Timecode<DCPTime> (this);
	_trim_start_label = create_label (this, _("Trim from start"), true);
	_trim_start = new Timecode<ContentTime> (this);
	_trim_start_to_playhead = new Button (this, _("Trim up to current position"));
	_trim_end_label = create_label (this, _("Trim from end"), true);
	_trim_end = new Timecode<ContentTime> (this);
	_trim_end_to_playhead = new Button (this, _("Trim from current position to end"));
	_play_length_label = create_label (this, _("Play length"), true);
	_play_length = new Timecode<DCPTime> (this);

	_position->Changed.connect    (boost::bind (&TimingPanel::position_changed, this));
	_move_to_start_of_reel->Bind  (wxEVT_BUTTON, boost::bind (&TimingPanel::move_to_start_of_reel_clicked, this));
	_full_length->Changed.connect (boost::bind (&TimingPanel::full_length_changed, this));
	_trim_start->Changed.connect  (boost::bind (&TimingPanel::trim_start_changed, this));
	_trim_start_to_playhead->Bind (wxEVT_BUTTON, boost::bind (&TimingPanel::trim_start_to_playhead_clicked, this));
	_trim_end->Changed.connect    (boost::bind (&TimingPanel::trim_end_changed, this));
	_trim_end_to_playhead->Bind   (wxEVT_BUTTON, boost::bind (&TimingPanel::trim_end_to_playhead_clicked, this));
	_play_length->Changed.connect (boost::bind (&TimingPanel::play_length_changed, this));

	shared_ptr<FilmViewer> fv = _viewer.lock ();
	DCPOMATIC_ASSERT (fv);
	fv->ImageChanged.connect (boost::bind (&TimingPanel::setup_sensitivity, this));

	setup_sensitivity ();
	add_to_grid ();
}

void
TimingPanel::add_to_grid ()
{
	bool const full = Config::instance()->interface_complexity() == Config::INTERFACE_FULL;

	int r = 0;

	wxSizer* labels = new wxBoxSizer (wxHORIZONTAL);
	labels->Add (_h_label, 1, wxEXPAND);
	add_label_to_sizer (labels, _colon[0], false);
	labels->Add (_m_label, 1, wxEXPAND);
	add_label_to_sizer (labels, _colon[1], false);
	labels->Add (_s_label, 1, wxEXPAND);
	add_label_to_sizer (labels, _colon[2], false);
	labels->Add (_f_label, 1, wxEXPAND);
	_grid->Add (labels, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (_grid, _position_label, true, wxGBPosition(r, 0));
	_grid->Add (_position, wxGBPosition(r, 1));
	++r;

	_move_to_start_of_reel->Show (full);
	_full_length_label->Show (full);
	_full_length->Show (full);
	_play_length_label->Show (full);
	_play_length->Show (full);

	if (full) {
		_grid->Add (_move_to_start_of_reel, wxGBPosition(r, 1));
		++r;

		add_label_to_sizer (_grid, _full_length_label, true, wxGBPosition(r, 0));
		_grid->Add (_full_length, wxGBPosition(r, 1));
		++r;
	}

	add_label_to_sizer (_grid, _trim_start_label, true, wxGBPosition(r, 0));
	_grid->Add (_trim_start, wxGBPosition(r, 1));
	++r;

	_grid->Add (_trim_start_to_playhead, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (_grid, _trim_end_label, true, wxGBPosition(r, 0));
	_grid->Add (_trim_end, wxGBPosition(r, 1));
	++r;

	_grid->Add (_trim_end_to_playhead, wxGBPosition(r, 1));
	++r;

	if (full) {
		add_label_to_sizer (_grid, _play_length_label, true, wxGBPosition(r, 0));
		_grid->Add (_play_length, wxGBPosition(r, 1));
		++r;
	}

	/* Completely speculative fix for #891 */
	_grid->Layout ();
}

void
TimingPanel::update_full_length ()
{
	set<DCPTime> check;
	BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
		check.insert (i->full_length(_parent->film()));
	}

	if (check.size() == 1) {
		_full_length->set (_parent->selected().front()->full_length(_parent->film()), _parent->film()->video_frame_rate());
	} else {
		_full_length->clear ();
	}
}

void
TimingPanel::update_play_length ()
{
	set<DCPTime> check;
	BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
		check.insert (i->length_after_trim(_parent->film()));
	}

	if (check.size() == 1) {
		_play_length->set (_parent->selected().front()->length_after_trim(_parent->film()), _parent->film()->video_frame_rate());
	} else {
		_play_length->clear ();
	}
}

void
TimingPanel::film_content_changed (int property)
{
	if (_film_content_changed_suspender.check(property)) {
		return;
	}

	int const film_video_frame_rate = _parent->film()->video_frame_rate ();

	/* Here we check to see if we have exactly one different value of various
	   properties, and fill the controls with that value if so.
	*/

	if (property == ContentProperty::POSITION) {

		set<DCPTime> check;
		BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
			check.insert (i->position ());
		}

		if (check.size() == 1) {
			_position->set (_parent->selected().front()->position(), film_video_frame_rate);
		} else {
			_position->clear ();
		}

	} else if (
		property == ContentProperty::LENGTH ||
		property == ContentProperty::VIDEO_FRAME_RATE ||
		property == VideoContentProperty::FRAME_TYPE
		) {

		update_full_length ();

	} else if (property == ContentProperty::TRIM_START) {

		set<ContentTime> check;
		BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
			check.insert (i->trim_start ());
		}

		if (check.size() == 1) {
			_trim_start->set (_parent->selected().front()->trim_start (), film_video_frame_rate);
		} else {
			_trim_start->clear ();
		}

	} else if (property == ContentProperty::TRIM_END) {

		set<ContentTime> check;
		BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
			check.insert (i->trim_end ());
		}

		if (check.size() == 1) {
			_trim_end->set (_parent->selected().front()->trim_end (), film_video_frame_rate);
		} else {
			_trim_end->clear ();
		}
	}

	if (
		property == ContentProperty::LENGTH ||
		property == ContentProperty::TRIM_START ||
		property == ContentProperty::TRIM_END ||
		property == ContentProperty::VIDEO_FRAME_RATE ||
		property == VideoContentProperty::FRAME_TYPE
		) {

		update_play_length ();
	}

	if (property == ContentProperty::VIDEO_FRAME_RATE) {
		set<double> check_vc;
		shared_ptr<const Content> content;
		int count_ac = 0;
		int count_sc = 0;
		BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
			if (i->video && i->video_frame_rate()) {
				check_vc.insert (i->video_frame_rate().get());
				content = i;
			}
			if (i->audio && i->video_frame_rate()) {
				++count_ac;
				content = i;
			}
			if (!i->text.empty() && i->video_frame_rate()) {
				++count_sc;
				content = i;
			}

		}
	}

	bool have_still = false;
	BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
		shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (i);
		if (ic && ic->still ()) {
			have_still = true;
		}
	}

	_full_length->set_editable (have_still);
	_play_length->set_editable (!have_still);
	setup_sensitivity ();
}

void
TimingPanel::position_changed ()
{
	DCPTime const pos = _position->get (_parent->film()->video_frame_rate ());
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		i->set_position (_parent->film(), pos);
	}
}

void
TimingPanel::full_length_changed ()
{
	int const vfr = _parent->film()->video_frame_rate ();
	Frame const len = _full_length->get (vfr).frames_round (vfr);
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (i);
		if (ic && ic->still ()) {
			ic->video->set_length (len);
		}
	}
}

void
TimingPanel::trim_start_changed ()
{
	shared_ptr<FilmViewer> fv = _viewer.lock ();
	if (!fv) {
		return;
	}

	DCPTime const ph = fv->position ();

	fv->set_coalesce_player_changes (true);

	shared_ptr<Content> ref;
	optional<FrameRateChange> ref_frc;
	optional<DCPTime> ref_ph;

	Suspender::Block bl = _film_content_changed_suspender.block ();
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		if (i->position() <= ph && ph < i->end(_parent->film())) {
			/* The playhead is in i.  Use it as a reference to work out
			   where to put the playhead post-trim; we're trying to keep the playhead
			   at the same frame of content that we're looking at pre-trim.
			*/
			ref = i;
			ref_frc = _parent->film()->active_frame_rate_change (i->position ());
			ref_ph = ph - i->position() + DCPTime (i->trim_start(), ref_frc.get());
		}

		ContentTime const trim = _trim_start->get (i->video_frame_rate().get_value_or(_parent->film()->video_frame_rate()));
		i->set_trim_start (trim);
	}

	if (ref) {
		fv->seek (max(DCPTime(), ref_ph.get() + ref->position() - DCPTime(ref->trim_start(), ref_frc.get())), true);
	}

	fv->set_coalesce_player_changes (false);
}

void
TimingPanel::trim_end_changed ()
{
	shared_ptr<FilmViewer> fv = _viewer.lock ();
	if (!fv) {
		return;
	}

	fv->set_coalesce_player_changes (true);

	Suspender::Block bl = _film_content_changed_suspender.block ();
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		ContentTime const trim = _trim_end->get (i->video_frame_rate().get_value_or(_parent->film()->video_frame_rate()));
		i->set_trim_end (trim);
	}

	/* XXX: maybe playhead-off-the-end-of-the-film should be handled elsewhere */
	if (fv->position() >= _parent->film()->length()) {
		fv->seek (_parent->film()->length() - DCPTime::from_frames(1, _parent->film()->video_frame_rate()), true);
	}

	fv->set_coalesce_player_changes (false);
}

void
TimingPanel::play_length_changed ()
{
	DCPTime const play_length = _play_length->get (_parent->film()->video_frame_rate());
	Suspender::Block bl = _film_content_changed_suspender.block ();
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		FrameRateChange const frc = _parent->film()->active_frame_rate_change (i->position ());
		i->set_trim_end (
			ContentTime (max(DCPTime(), i->full_length(_parent->film()) - play_length), frc) - i->trim_start()
			);
	}
}


void
TimingPanel::content_selection_changed ()
{
	setup_sensitivity ();

	film_content_changed (ContentProperty::POSITION);
	film_content_changed (ContentProperty::LENGTH);
	film_content_changed (ContentProperty::TRIM_START);
	film_content_changed (ContentProperty::TRIM_END);
	film_content_changed (ContentProperty::VIDEO_FRAME_RATE);
}

void
TimingPanel::film_changed (Film::Property p)
{
	if (p == Film::VIDEO_FRAME_RATE) {
		update_full_length ();
		update_play_length ();
	}
}

void
TimingPanel::trim_start_to_playhead_clicked ()
{
	shared_ptr<FilmViewer> fv = _viewer.lock ();
	if (!fv) {
		return;
	}

	shared_ptr<const Film> film = _parent->film ();
	DCPTime const ph = fv->position().floor (film->video_frame_rate ());
	optional<DCPTime> new_ph;

	fv->set_coalesce_player_changes (true);

	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		if (i->position() < ph && ph < i->end(film)) {
			FrameRateChange const frc = film->active_frame_rate_change (i->position());
			i->set_trim_start (i->trim_start() + ContentTime (ph - i->position(), frc));
			new_ph = i->position ();
		}
	}

	fv->set_coalesce_player_changes (false);

	if (new_ph) {
		fv->seek (new_ph.get(), true);
	}
}

void
TimingPanel::trim_end_to_playhead_clicked ()
{
	shared_ptr<FilmViewer> fv = _viewer.lock ();
	if (!fv) {
		return;
	}

	shared_ptr<const Film> film = _parent->film ();
	DCPTime const ph = fv->position().floor (film->video_frame_rate ());
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		if (i->position() < ph && ph < i->end(film)) {
			FrameRateChange const frc = film->active_frame_rate_change (i->position ());
			i->set_trim_end (ContentTime(i->position() + i->full_length(film) - ph, frc) - i->trim_start());
		}
	}
}

void
TimingPanel::setup_sensitivity ()
{
	bool const e = !_parent->selected().empty ();

	_position->Enable (e);
	_move_to_start_of_reel->Enable (e);
	_full_length->Enable (e);
	_trim_start->Enable (e);
	_trim_end->Enable (e);
	_play_length->Enable (e);

	shared_ptr<FilmViewer> fv = _viewer.lock ();
	DCPOMATIC_ASSERT (fv);
	DCPTime const ph = fv->position ();
	bool any_over_ph = false;
	BOOST_FOREACH (shared_ptr<const Content> i, _parent->selected ()) {
		if (i->position() <= ph && ph < i->end(_parent->film())) {
			any_over_ph = true;
		}
	}

	_trim_start_to_playhead->Enable (any_over_ph);
	_trim_end_to_playhead->Enable (any_over_ph);
}

void
TimingPanel::move_to_start_of_reel_clicked ()
{
	/* Find common position of all selected content, if it exists */

	optional<DCPTime> position;
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		if (!position) {
			position = i->position();
		} else {
			if (position.get() != i->position()) {
				position.reset ();
				break;
			}
		}
	}

	MoveToDialog* d = new MoveToDialog (this, position, _parent->film());

	if (d->ShowModal() == wxID_OK) {
		BOOST_FOREACH (shared_ptr<Content> i, _parent->selected()) {
			i->set_position (_parent->film(), d->position());
		}
	}
	d->Destroy ();
}
