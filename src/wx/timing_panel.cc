/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "lib/content.h"
#include "lib/image_content.h"
#include "timing_panel.h"
#include "wx_util.h"
#include "timecode.h"
#include "film_editor.h"

using std::cout;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

TimingPanel::TimingPanel (FilmEditor* e)
	/* horrid hack for apparent lack of context support with wxWidgets i18n code */
	: FilmEditorPanel (e, S_("Timing|Timing"))
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	_sizer->Add (grid, 0, wxALL, 8);

	add_label_to_sizer (grid, this, _("Position"), true);
	_position = new Timecode (this);
	grid->Add (_position);
	add_label_to_sizer (grid, this, _("Full length"), true);
	_full_length = new Timecode (this);
	grid->Add (_full_length);
	add_label_to_sizer (grid, this, _("Trim from start"), true);
	_trim_start = new Timecode (this);
	grid->Add (_trim_start);
	add_label_to_sizer (grid, this, _("Trim from end"), true);
	_trim_end = new Timecode (this);
	grid->Add (_trim_end);
	add_label_to_sizer (grid, this, _("Play length"), true);
	_play_length = new Timecode (this);
	grid->Add (_play_length);

	{
		add_label_to_sizer (grid, this, _("Video frame rate"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_video_frame_rate = new wxTextCtrl (this, wxID_ANY);
		s->Add (_video_frame_rate, 1, wxEXPAND);
		_set_video_frame_rate = new wxButton (this, wxID_ANY, _("Set"));
		_set_video_frame_rate->Enable (false);
		s->Add (_set_video_frame_rate, 0, wxLEFT | wxRIGHT, 8);
		grid->Add (s, 1, wxEXPAND);
	}

	_position->Changed.connect    (boost::bind (&TimingPanel::position_changed, this));
	_full_length->Changed.connect (boost::bind (&TimingPanel::full_length_changed, this));
	_trim_start->Changed.connect  (boost::bind (&TimingPanel::trim_start_changed, this));
	_trim_end->Changed.connect    (boost::bind (&TimingPanel::trim_end_changed, this));
	_play_length->Changed.connect (boost::bind (&TimingPanel::play_length_changed, this));
	_video_frame_rate->Bind       (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&TimingPanel::video_frame_rate_changed, this));
	_set_video_frame_rate->Bind   (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&TimingPanel::set_video_frame_rate, this));
}

void
TimingPanel::film_content_changed (int property)
{
	ContentList cl = _editor->selected_content ();
	shared_ptr<Content> content;
	if (cl.size() == 1) {
		content = cl.front ();
	}

	int const film_video_frame_rate = _editor->film()->video_frame_rate ();
	
	if (property == ContentProperty::POSITION) {
		if (content) {
			_position->set (content->position (), film_video_frame_rate);
		} else {
			_position->set (DCPTime () , 24);
		}
	} else if (
		property == ContentProperty::LENGTH ||
		property == VideoContentProperty::VIDEO_FRAME_RATE ||
		property == VideoContentProperty::VIDEO_FRAME_TYPE
		) {
		if (content) {
			_full_length->set (content->full_length (), film_video_frame_rate);
			_play_length->set (content->length_after_trim (), film_video_frame_rate);
		} else {
			_full_length->set (DCPTime (), 24);
			_play_length->set (DCPTime (), 24);
		}
	} else if (property == ContentProperty::TRIM_START) {
		if (content) {
			_trim_start->set (content->trim_start (), film_video_frame_rate);
			_play_length->set (content->length_after_trim (), film_video_frame_rate);
		} else {
			_trim_start->set (DCPTime (), 24);
			_play_length->set (DCPTime (), 24);
		}
	} else if (property == ContentProperty::TRIM_END) {
		if (content) {
			_trim_end->set (content->trim_end (), film_video_frame_rate);
			_play_length->set (content->length_after_trim (), film_video_frame_rate);
		} else {
			_trim_end->set (DCPTime (), 24);
			_play_length->set (DCPTime (), 24);
		}
	}

	if (property == VideoContentProperty::VIDEO_FRAME_RATE) {
		if (content) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (content);
			if (vc) {
				_video_frame_rate->SetValue (std_to_wx (lexical_cast<string> (vc->video_frame_rate ())));
			} else {
				_video_frame_rate->SetValue ("24");
			}
		} else {
			_video_frame_rate->SetValue ("24");
		}
	}

	shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (content);
	_full_length->set_editable (ic && ic->still ());
	_play_length->set_editable (!ic || !ic->still ());
	_video_frame_rate->Enable (ic && !ic->still ());
	_set_video_frame_rate->Enable (false);
}

void
TimingPanel::position_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		c.front()->set_position (_position->get (_editor->film()->video_frame_rate ()));
	}
}

void
TimingPanel::full_length_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (c.front ());
		if (ic && ic->still ()) {
			/* XXX: No effective FRC here... is this right? */
			ic->set_video_length (ContentTime (_full_length->get (_editor->film()->video_frame_rate()), FrameRateChange (1, 1)));
		}
	}
}

void
TimingPanel::trim_start_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		c.front()->set_trim_start (_trim_start->get (_editor->film()->video_frame_rate ()));
	}
}


void
TimingPanel::trim_end_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		c.front()->set_trim_end (_trim_end->get (_editor->film()->video_frame_rate ()));
	}
}

void
TimingPanel::play_length_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		c.front()->set_trim_end (c.front()->full_length() - _play_length->get (_editor->film()->video_frame_rate()) - c.front()->trim_start());
	}
}

void
TimingPanel::video_frame_rate_changed ()
{
	_set_video_frame_rate->Enable (true);
}

void
TimingPanel::set_video_frame_rate ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (c.front ());
		if (ic) {
			ic->set_video_frame_rate (lexical_cast<float> (wx_to_std (_video_frame_rate->GetValue ())));
		}
		_set_video_frame_rate->Enable (false);
	}
}

void
TimingPanel::content_selection_changed ()
{
	ContentList sel = _editor->selected_content ();
	bool const single = sel.size() == 1;

	/* Things that are only allowed with single selections */
	_position->Enable (single);
	_full_length->Enable (single);
	_trim_start->Enable (single);
	_trim_end->Enable (single);
	_play_length->Enable (single);
	_video_frame_rate->Enable (single);
	
	film_content_changed (ContentProperty::POSITION);
	film_content_changed (ContentProperty::LENGTH);
	film_content_changed (ContentProperty::TRIM_START);
	film_content_changed (ContentProperty::TRIM_END);
	film_content_changed (VideoContentProperty::VIDEO_FRAME_RATE);
}
