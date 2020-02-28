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
#ifdef DCPOMATIC_LINUX
	/* Hack to work around failure to centre text on GTK */
	gtk_label_set_line_wrap (GTK_LABEL(_h_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "minutes"
	_m_label = new StaticText (this, _("m"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef DCPOMATIC_LINUX
	gtk_label_set_line_wrap (GTK_LABEL (_m_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "seconds"
	_s_label = new StaticText (this, _("s"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef DCPOMATIC_LINUX
	gtk_label_set_line_wrap (GTK_LABEL(_s_label->GetHandle()), FALSE);
#endif
	//// TRANSLATORS: this is an abbreviation for "frames"
	_f_label = new StaticText (this, _("f"), wxDefaultPosition, size, wxALIGN_CENTRE_HORIZONTAL);
#ifdef DCPOMATIC_LINUX
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
	_trim_end_to_playhead = new Button (this, _("Trim after current position"));
	_play_length_label = create_label (this, _("Play length"), true);
	_play_length = new Timecode<DCPTime> (this);

	_video_frame_rate_label = create_label (this, _("Video frame rate"), true);
	_video_frame_rate = new wxTextCtrl (this, wxID_ANY);
	_set_video_frame_rate = new Button (this, _("Set"));
	_set_video_frame_rate->Enable (false);

	/* We can't use Wrap() here as it doesn't work with markup:
	 * http://trac.wxwidgets.org/ticket/13389
	 */

	wxString in = _("<i>Only change this if the content's frame rate has been read incorrectly.</i>");
	wxString out;
	int const width = 20;
	int current = 0;
	for (size_t i = 0; i < in.Length(); ++i) {
		if (in[i] == ' ' && current >= width) {
			out += '\n';
			current = 0;
		} else {
			out += in[i];
			++current;
		}
	}

	_tip = new StaticText (this, wxT (""));
	_tip->SetLabelMarkup (out);
#ifdef DCPOMATIC_OSX
	/* Hack to stop hidden text on some versions of OS X */
	_tip->SetMinSize (wxSize (-1, 256));
#endif

	_position->Changed.connect    (boost::bind (&TimingPanel::position_changed, this));
	_move_to_start_of_reel->Bind  (wxEVT_BUTTON, boost::bind (&TimingPanel::move_to_start_of_reel_clicked, this));
	_full_length->Changed.connect (boost::bind (&TimingPanel::full_length_changed, this));
	_trim_start->Changed.connect  (boost::bind (&TimingPanel::trim_start_changed, this));
	_trim_start_to_playhead->Bind (wxEVT_BUTTON, boost::bind (&TimingPanel::trim_start_to_playhead_clicked, this));
	_trim_end->Changed.connect    (boost::bind (&TimingPanel::trim_end_changed, this));
	_trim_end_to_playhead->Bind   (wxEVT_BUTTON, boost::bind (&TimingPanel::trim_end_to_playhead_clicked, this));
	_play_length->Changed.connect (boost::bind (&TimingPanel::play_length_changed, this));
	_video_frame_rate->Bind       (wxEVT_TEXT, boost::bind (&TimingPanel::video_frame_rate_changed, this));
	_set_video_frame_rate->Bind   (wxEVT_BUTTON, boost::bind (&TimingPanel::set_video_frame_rate, this));

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
	_video_frame_rate_label->Show (full);
	_video_frame_rate->Show (full);
	_set_video_frame_rate->Show (full);
	_tip->Show (full);

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

		{
			add_label_to_sizer (_grid, _video_frame_rate_label, true, wxGBPosition(r, 0));
			wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
			s->Add (_video_frame_rate, 1, wxEXPAND);
			s->Add (_set_video_frame_rate, 0, wxLEFT | wxRIGHT, 8);
			_grid->Add (s, wxGBPosition(r, 1), wxGBSpan(1, 2));
		}
		++r;

		_grid->Add (_tip, wxGBPosition(r, 1), wxGBSpan(1, 2));
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

		bool const single_frame_image_content = content && dynamic_pointer_cast<const ImageContent> (content) && content->number_of_paths() == 1;

		if ((check_vc.size() == 1 || count_ac == 1 || count_sc == 1) && !single_frame_image_content) {
			checked_set (_video_frame_rate, locale_convert<string> (content->video_frame_rate().get(), 5));
			_video_frame_rate->Enable (true);
		} else {
			checked_set (_video_frame_rate, wxT (""));
			_video_frame_rate->Enable (false);
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
	_set_video_frame_rate->Enable (false);
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
TimingPanel::video_frame_rate_changed ()
{
	bool enable = true;
	if (_video_frame_rate->GetValue() == wxT("")) {
		/* No frame rate has been entered; if the user clicks "set" now it would unset the video
		   frame rate in the selected content.  This can't be allowed for some content types.
		*/
		BOOST_FOREACH (shared_ptr<Content> i, _parent->selected()) {
			if (
				dynamic_pointer_cast<DCPContent>(i) ||
				dynamic_pointer_cast<FFmpegContent>(i)
				) {
				enable = false;
			}
		}
	}

	_set_video_frame_rate->Enable (enable);
}

void
TimingPanel::set_video_frame_rate ()
{
	optional<double> fr;
	if (_video_frame_rate->GetValue() != wxT("")) {
		fr = locale_convert<double> (wx_to_std (_video_frame_rate->GetValue ()));
	}
	Suspender::Block bl = _film_content_changed_suspender.block ();
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected ()) {
		if (fr) {
			i->set_video_frame_rate (*fr);
		} else {
			i->unset_video_frame_rate ();
		}
	}

	_set_video_frame_rate->Enable (false);
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

	if (new_ph) {
		fv->seek (new_ph.get(), true);
	}

	fv->set_coalesce_player_changes (false);
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
			i->set_trim_end (ContentTime(i->position() + i->full_length(film) - ph - DCPTime::from_frames(1, frc.dcp), frc) - i->trim_start());
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
	_video_frame_rate->Enable (e);

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
