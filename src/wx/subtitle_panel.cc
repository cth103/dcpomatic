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

#include <boost/lexical_cast.hpp>
#include <wx/spinctrl.h>
#include "subtitle_panel.h"
#include "film_editor.h"
#include "wx_util.h"

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::dynamic_pointer_cast;

SubtitlePanel::SubtitlePanel (FilmEditor* e)
	: FilmEditorPanel (e, _("Subtitles"))
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	_with_subtitles = new wxCheckBox (this, wxID_ANY, _("With Subtitles"));
	grid->Add (_with_subtitles, 1);
	grid->AddSpacer (0);
	
	{
		add_label_to_sizer (grid, this, _("Subtitle Offset"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_offset = new wxSpinCtrl (this);
		s->Add (_offset);
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
	
	_offset->SetRange (-100, 100);
	_scale->SetRange (1, 1000);
	_scale->SetValue (100);

	_with_subtitles->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&SubtitlePanel::with_subtitles_toggled, this));
	_offset->Bind         (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::offset_changed, this));
	_scale->Bind          (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::scale_changed, this));
	_stream->Bind         (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&SubtitlePanel::stream_changed, this));
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
SubtitlePanel::film_content_changed (shared_ptr<Content> c, int property)
{
	shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (c);
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	
	if (property == FFmpegContentProperty::SUBTITLE_STREAMS) {
		_stream->Clear ();
		if (fc) {
			vector<shared_ptr<FFmpegSubtitleStream> > s = fc->subtitle_streams ();
			for (vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = s.begin(); i != s.end(); ++i) {
				_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx (lexical_cast<string> ((*i)->id))));
			}
			
			if (fc->subtitle_stream()) {
				checked_set (_stream, lexical_cast<string> (fc->subtitle_stream()->id));
			} else {
				_stream->SetSelection (wxNOT_FOUND);
			}
		}
		setup_sensitivity ();
	} else if (property == SubtitleContentProperty::SUBTITLE_OFFSET) {
		checked_set (_offset, sc ? (sc->subtitle_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::SUBTITLE_SCALE) {
		checked_set (_scale, sc ? (sc->subtitle_scale() * 100) : 100);
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
	_offset->Enable (j);
	_scale->Enable (j);
	_stream->Enable (j);
}

void
SubtitlePanel::stream_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}
	
	vector<shared_ptr<FFmpegSubtitleStream> > a = fc->subtitle_streams ();
	vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = a.begin ();
	string const s = string_client_data (_stream->GetClientObject (_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> ((*i)->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		fc->set_subtitle_stream (*i);
	}
}

void
SubtitlePanel::offset_changed ()
{
	shared_ptr<SubtitleContent> c = _editor->selected_subtitle_content ();
	if (!c) {
		return;
	}

	c->set_subtitle_offset (_offset->GetValue() / 100.0);
}

void
SubtitlePanel::scale_changed ()
{
	shared_ptr<SubtitleContent> c = _editor->selected_subtitle_content ();
	if (!c) {
		return;
	}

	c->set_subtitle_scale (_scale->GetValue() / 100.0);
}

