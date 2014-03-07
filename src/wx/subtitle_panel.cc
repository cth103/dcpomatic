/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/lexical_cast.hpp>
#include <wx/spinctrl.h>
#include "lib/ffmpeg_content.h"
#include "lib/subrip_content.h"
#include "subtitle_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "subtitle_view.h"

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::dynamic_pointer_cast;

SubtitlePanel::SubtitlePanel (FilmEditor* e)
	: FilmEditorPanel (e, _("Subtitles"))
	, _view (0)
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	_with_subtitles = new wxCheckBox (this, wxID_ANY, _("With Subtitles"));
	grid->Add (_with_subtitles, 1);
	grid->AddSpacer (0);
	
	{
		add_label_to_sizer (grid, this, _("Subtitle X Offset"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_x_offset = new wxSpinCtrl (this);
		s->Add (_x_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}

	{
		add_label_to_sizer (grid, this, _("Subtitle Y Offset"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_y_offset = new wxSpinCtrl (this);
		s->Add (_y_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}
	
	{
		add_label_to_sizer (grid, this, _("Subtitle Scale"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_scale = new wxSpinCtrl (this);
		s->Add (_scale);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}

	add_label_to_sizer (grid, this, _("Subtitle Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_stream, 1, wxEXPAND);

	_view_button = new wxButton (this, wxID_ANY, _("View..."));
	grid->Add (_view_button);
	
	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_scale->SetRange (1, 1000);
	_scale->SetValue (100);

	_with_subtitles->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&SubtitlePanel::with_subtitles_toggled, this));
	_x_offset->Bind       (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::x_offset_changed, this));
	_y_offset->Bind       (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::y_offset_changed, this));
	_scale->Bind          (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::scale_changed, this));
	_stream->Bind         (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&SubtitlePanel::stream_changed, this));
	_view_button->Bind    (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&SubtitlePanel::view_clicked, this));
}

void
SubtitlePanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::CONTENT:
		setup_sensitivity ();
		break;
	case Film::WITH_SUBTITLES:
		checked_set (_with_subtitles, _editor->film()->with_subtitles ());
		setup_sensitivity ();
		break;
	default:
		break;
	}
}

void
SubtitlePanel::film_content_changed (int property)
{
	FFmpegContentList fc = _editor->selected_ffmpeg_content ();
	SubtitleContentList sc = _editor->selected_subtitle_content ();

	shared_ptr<FFmpegContent> fcs;
	if (fc.size() == 1) {
		fcs = fc.front ();
	}

	shared_ptr<SubtitleContent> scs;
	if (sc.size() == 1) {
		scs = sc.front ();
	}
	
	if (property == FFmpegContentProperty::SUBTITLE_STREAMS) {
		_stream->Clear ();
		if (fcs) {
			vector<shared_ptr<FFmpegSubtitleStream> > s = fcs->subtitle_streams ();
			for (vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = s.begin(); i != s.end(); ++i) {
				_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx ((*i)->identifier ())));
			}
			
			if (fcs->subtitle_stream()) {
				checked_set (_stream, fcs->subtitle_stream()->identifier ());
			} else {
				_stream->SetSelection (wxNOT_FOUND);
			}
		}
		setup_sensitivity ();
	} else if (property == SubtitleContentProperty::SUBTITLE_X_OFFSET) {
		checked_set (_x_offset, scs ? (scs->subtitle_x_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::SUBTITLE_Y_OFFSET) {
		checked_set (_y_offset, scs ? (scs->subtitle_y_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::SUBTITLE_SCALE) {
		checked_set (_scale, scs ? (scs->subtitle_scale() * 100) : 100);
	}
}

void
SubtitlePanel::with_subtitles_toggled ()
{
	if (!_editor->film()) {
		return;
	}

	_editor->film()->set_with_subtitles (_with_subtitles->GetValue ());
}

void
SubtitlePanel::setup_sensitivity ()
{
	bool h = false;
	bool j = false;
	if (_editor->film()) {
		h = _editor->film()->has_subtitles ();
		j = _editor->film()->with_subtitles ();
	}
	
	_with_subtitles->Enable (h);
	_x_offset->Enable (j);
	_y_offset->Enable (j);
	_scale->Enable (j);
	_stream->Enable (j);

	SubtitleContentList c = _editor->selected_subtitle_content ();
	_view_button->Enable (c.size() == 1);
}

void
SubtitlePanel::stream_changed ()
{
	FFmpegContentList fc = _editor->selected_ffmpeg_content ();
	if (fc.size() != 1) {
		return;
	}

	shared_ptr<FFmpegContent> fcs = fc.front ();
	
	vector<shared_ptr<FFmpegSubtitleStream> > a = fcs->subtitle_streams ();
	vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = a.begin ();
	string const s = string_client_data (_stream->GetClientObject (_stream->GetSelection ()));
	while (i != a.end() && (*i)->identifier () != s) {
		++i;
	}

	if (i != a.end ()) {
		fcs->set_subtitle_stream (*i);
	}
}

void
SubtitlePanel::x_offset_changed ()
{
	SubtitleContentList c = _editor->selected_subtitle_content ();
	if (c.size() == 1) {
		c.front()->set_subtitle_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::y_offset_changed ()
{
	SubtitleContentList c = _editor->selected_subtitle_content ();
	if (c.size() == 1) {
		c.front()->set_subtitle_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::scale_changed ()
{
	SubtitleContentList c = _editor->selected_subtitle_content ();
	if (c.size() == 1) {
		c.front()->set_subtitle_scale (_scale->GetValue() / 100.0);
	}
}

void
SubtitlePanel::content_selection_changed ()
{
	film_content_changed (FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (SubtitleContentProperty::SUBTITLE_X_OFFSET);
	film_content_changed (SubtitleContentProperty::SUBTITLE_Y_OFFSET);
	film_content_changed (SubtitleContentProperty::SUBTITLE_SCALE);
}

void
SubtitlePanel::view_clicked ()
{
	if (_view) {
		_view->Destroy ();
		_view = 0;
	}

	SubtitleContentList c = _editor->selected_subtitle_content ();
	assert (c.size() == 1);
	shared_ptr<SubRipContent> sr = dynamic_pointer_cast<SubRipContent> (c.front ());
	if (sr) {
		_view = new SubtitleView (this, sr);
	}

	_view->Show ();
}
