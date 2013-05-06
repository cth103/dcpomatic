/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "lib/ffmpeg_content.h"
#include "ffmpeg_content_dialog.h"
#include "wx_util.h"

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;

FFmpegContentDialog::FFmpegContentDialog (wxWindow* parent, shared_ptr<FFmpegContent> content)
	: wxDialog (parent, wxID_ANY, _("Video"))
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (3, 6, 6);
	grid->AddGrowableCol (1, 1);

	add_label_to_sizer (grid, this, _("Audio Stream"));
	_audio_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_audio_stream, 1, wxEXPAND | wxALL, 6);
	_audio_description = new wxStaticText (this, wxID_ANY, wxT (""));
	grid->Add (_audio_description, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
	
	add_label_to_sizer (grid, this, _("Subtitle stream"));
	_subtitle_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_subtitle_stream, 1, wxEXPAND | wxALL, 6);
	grid->AddSpacer (0);

	_audio_stream->Clear ();
	vector<FFmpegAudioStream> a = content->audio_streams ();
	for (vector<FFmpegAudioStream>::iterator i = a.begin(); i != a.end(); ++i) {
		_audio_stream->Append (std_to_wx (i->name), new wxStringClientData (std_to_wx (lexical_cast<string> (i->id))));
	}
	
	if (content->audio_stream()) {
		checked_set (_audio_stream, lexical_cast<string> (content->audio_stream()->id));
	}

	_subtitle_stream->Clear ();
	vector<FFmpegSubtitleStream> s = content->subtitle_streams ();
	if (s.empty ()) {
		_subtitle_stream->Enable (false);
	}
	for (vector<FFmpegSubtitleStream>::iterator i = s.begin(); i != s.end(); ++i) {
		_subtitle_stream->Append (std_to_wx (i->name), new wxStringClientData (std_to_wx (lexical_cast<string> (i->id))));
	}
	
	if (content->subtitle_stream()) {
		checked_set (_subtitle_stream, lexical_cast<string> (content->subtitle_stream()->id));
	} else {
		_subtitle_stream->SetSelection (wxNOT_FOUND);
	}

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (grid, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_audio_stream->Connect    (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (FFmpegContentDialog::audio_stream_changed), 0, this);
	_subtitle_stream->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (FFmpegContentDialog::subtitle_stream_changed), 0, this);
}

void
FFmpegContentDialog::audio_stream_changed (wxCommandEvent &)
{
	shared_ptr<FFmpegContent> c = _content.lock ();
	if (!c) {
		return;
	}
	
	vector<FFmpegAudioStream> a = c->audio_streams ();
	vector<FFmpegAudioStream>::iterator i = a.begin ();
	string const s = string_client_data (_audio_stream->GetClientObject (_audio_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> (i->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		c->set_audio_stream (*i);
	}

	if (!c->audio_stream ()) {
		_audio_description->SetLabel (wxT (""));
	} else {
		wxString s;
		if (c->audio_channels() == 1) {
			s << _("1 channel");
		} else {
			s << c->audio_channels() << wxT (" ") << _("channels");
		}
		s << wxT (", ") << c->audio_frame_rate() << _("Hz");
		_audio_description->SetLabel (s);
	}
}



void
FFmpegContentDialog::subtitle_stream_changed (wxCommandEvent &)
{
	shared_ptr<FFmpegContent> c = _content.lock ();
	if (!c) {
		return;
	}
	
	vector<FFmpegSubtitleStream> a = c->subtitle_streams ();
	vector<FFmpegSubtitleStream>::iterator i = a.begin ();
	string const s = string_client_data (_subtitle_stream->GetClientObject (_subtitle_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> (i->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		c->set_subtitle_stream (*i);
	}
}
