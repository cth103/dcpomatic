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
#include "lib/still_image_content.h"
#include "timing_panel.h"
#include "wx_util.h"
#include "timecode.h"
#include "film_editor.h"

using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

TimingPanel::TimingPanel (FilmEditor* e)
	: FilmEditorPanel (e, _("Timing"))
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	_sizer->Add (grid, 0, wxALL, 8);

	add_label_to_sizer (grid, this, _("Position"), true);
	_position = new Timecode (this);
	grid->Add (_position);
	add_label_to_sizer (grid, this, _("Length"), true);
	_length = new Timecode (this);
	grid->Add (_length);
	add_label_to_sizer (grid, this, _("Trim from start"), true);
	_trim_start = new Timecode (this);
	grid->Add (_trim_start);
	add_label_to_sizer (grid, this, _("Trim from end"), true);
	_trim_end = new Timecode (this);
	grid->Add (_trim_end);

	_position->Changed.connect   (boost::bind (&TimingPanel::position_changed, this));
	_length->Changed.connect     (boost::bind (&TimingPanel::length_changed, this));
	_trim_start->Changed.connect (boost::bind (&TimingPanel::trim_start_changed, this));
	_trim_end->Changed.connect   (boost::bind (&TimingPanel::trim_end_changed, this));
}

void
TimingPanel::film_content_changed (shared_ptr<Content> content, int property)
{
	if (property == ContentProperty::POSITION) {
		if (content) {
			_position->set (content->position (), _editor->film()->video_frame_rate ());
		} else {
			_position->set (0, 24);
		}
	} else if (property == ContentProperty::LENGTH) {
		if (content) {
			_length->set (content->full_length (), _editor->film()->video_frame_rate ());
		} else {
			_length->set (0, 24);
		}
	} else if (property == ContentProperty::TRIM_START) {
		if (content) {
			_trim_start->set (content->trim_start (), _editor->film()->video_frame_rate ());
		} else {
			_trim_start->set (0, 24);
		}
	} else if (property == ContentProperty::TRIM_END) {
		if (content) {
			_trim_end->set (content->trim_end (), _editor->film()->video_frame_rate ());
		} else {
			_trim_end->set (0, 24);
		}
	}	

	_length->set_editable (dynamic_pointer_cast<StillImageContent> (content));
}

void
TimingPanel::position_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	c->set_position (_position->get (_editor->film()->video_frame_rate ()));
}

void
TimingPanel::length_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<StillImageContent> ic = dynamic_pointer_cast<StillImageContent> (c);
	if (ic) {
		ic->set_video_length (rint (_length->get (_editor->film()->video_frame_rate()) * ic->video_frame_rate() / TIME_HZ));
	}
}

void
TimingPanel::trim_start_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	c->set_trim_start (_trim_start->get (_editor->film()->video_frame_rate ()));
}


void
TimingPanel::trim_end_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	c->set_trim_end (_trim_end->get (_editor->film()->video_frame_rate ()));
}
