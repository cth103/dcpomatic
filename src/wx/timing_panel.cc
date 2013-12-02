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
TimingPanel::film_content_changed (int property)
{
	ContentList cl = _editor->selected_content ();
	shared_ptr<Content> content;
	if (cl.size() == 1) {
		content = cl.front ();
	}
	
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

	shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (content);
	_length->set_editable (ic && ic->still ());
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
TimingPanel::length_changed ()
{
	ContentList c = _editor->selected_content ();
	if (c.size() == 1) {
		shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (c.front ());
		if (ic && ic->still ()) {
			ic->set_video_length (rint (_length->get (_editor->film()->video_frame_rate()) * ic->video_frame_rate() / TIME_HZ));
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
TimingPanel::content_selection_changed ()
{
	ContentList sel = _editor->selected_content ();
	bool const single = sel.size() == 1;

	/* Things that are only allowed with single selections */
	_position->Enable (single);
	_length->Enable (single);
	_trim_start->Enable (single);
	_trim_end->Enable (single);
	
	film_content_changed (ContentProperty::POSITION);
	film_content_changed (ContentProperty::LENGTH);
	film_content_changed (ContentProperty::TRIM_START);
	film_content_changed (ContentProperty::TRIM_END);
}
