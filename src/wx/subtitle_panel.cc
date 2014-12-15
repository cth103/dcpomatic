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
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/subrip_decoder.h"
#include "lib/dcp_subtitle_decoder.h"
#include "subtitle_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "subtitle_view.h"
#include "content_panel.h"
#include "fonts_dialog.h"

using std::vector;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::dynamic_pointer_cast;

SubtitlePanel::SubtitlePanel (ContentPanel* p)
	: ContentSubPanel (p, _("Subtitles"))
	, _subtitle_view (0)
	, _fonts_dialog (0)
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	_use = new wxCheckBox (this, wxID_ANY, _("Use subtitles"));
	grid->Add (_use);
	grid->AddSpacer (0);

	{
		add_label_to_sizer (grid, this, _("X Offset"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_x_offset = new wxSpinCtrl (this);
		s->Add (_x_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}

	{
		add_label_to_sizer (grid, this, _("Y Offset"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_y_offset = new wxSpinCtrl (this);
		s->Add (_y_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}
	
	{
		add_label_to_sizer (grid, this, _("X Scale"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_x_scale = new wxSpinCtrl (this);
		s->Add (_x_scale);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}

	{
		add_label_to_sizer (grid, this, _("Y Scale"), true);
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_y_scale = new wxSpinCtrl (this);
		s->Add (_y_scale);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s);
	}

	add_label_to_sizer (grid, this, _("Language"), true);
	_language = new wxTextCtrl (this, wxID_ANY);
	grid->Add (_language, 1, wxEXPAND);
	
	add_label_to_sizer (grid, this, _("Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_stream, 1, wxEXPAND);

	_subtitle_view_button = new wxButton (this, wxID_ANY, _("View..."));
	grid->Add (_subtitle_view_button);
	grid->AddSpacer (0);

	_fonts_dialog_button = new wxButton (this, wxID_ANY, _("Fonts..."));
	grid->Add (_fonts_dialog_button);
	grid->AddSpacer (0);
	
	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_x_scale->SetRange (10, 1000);
	_y_scale->SetRange (10, 1000);

	_use->Bind                  (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&SubtitlePanel::use_toggled, this));
	_x_offset->Bind             (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::x_offset_changed, this));
	_y_offset->Bind             (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::y_offset_changed, this));
	_x_scale->Bind              (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::x_scale_changed, this));
	_y_scale->Bind              (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&SubtitlePanel::y_scale_changed, this));
	_language->Bind             (wxEVT_COMMAND_TEXT_UPDATED,     boost::bind (&SubtitlePanel::language_changed, this));
	_stream->Bind               (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&SubtitlePanel::stream_changed, this));
	_subtitle_view_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&SubtitlePanel::subtitle_view_clicked, this));
	_fonts_dialog_button->Bind  (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&SubtitlePanel::fonts_dialog_clicked, this));
}

void
SubtitlePanel::film_changed (Film::Property property)
{
	if (property == Film::CONTENT) {
		setup_sensitivity ();
	}
}

void
SubtitlePanel::film_content_changed (int property)
{
	FFmpegContentList fc = _parent->selected_ffmpeg ();
	SubtitleContentList sc = _parent->selected_subtitle ();

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
	} else if (property == SubtitleContentProperty::USE_SUBTITLES) {
		checked_set (_use, scs ? scs->use_subtitles() : false);
		setup_sensitivity ();
	} else if (property == SubtitleContentProperty::SUBTITLE_X_OFFSET) {
		checked_set (_x_offset, scs ? (scs->subtitle_x_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::SUBTITLE_Y_OFFSET) {
		checked_set (_y_offset, scs ? (scs->subtitle_y_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::SUBTITLE_X_SCALE) {
		checked_set (_x_scale, scs ? int (rint (scs->subtitle_x_scale() * 100)) : 100);
	} else if (property == SubtitleContentProperty::SUBTITLE_Y_SCALE) {
		checked_set (_y_scale, scs ? int (rint (scs->subtitle_y_scale() * 100)) : 100);
	} else if (property == SubtitleContentProperty::SUBTITLE_LANGUAGE) {
		checked_set (_language, scs ? scs->subtitle_language() : "");
	}
}

void
SubtitlePanel::use_toggled ()
{
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_use_subtitles (_use->GetValue());
	}
}

void
SubtitlePanel::setup_sensitivity ()
{
	int any_subs = 0;
	int ffmpeg_subs = 0;
	int subrip_or_dcp_subs = 0;
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::const_iterator i = c.begin(); i != c.end(); ++i) {
		shared_ptr<const FFmpegContent> fc = boost::dynamic_pointer_cast<const FFmpegContent> (*i);
		shared_ptr<const SubRipContent> sc = boost::dynamic_pointer_cast<const SubRipContent> (*i);
		shared_ptr<const DCPSubtitleContent> dsc = boost::dynamic_pointer_cast<const DCPSubtitleContent> (*i);
		if (fc) {
			if (fc->has_subtitles ()) {
				++ffmpeg_subs;
				++any_subs;
			}
		} else if (sc || dsc) {
			++subrip_or_dcp_subs;
			++any_subs;
		} else {
			++any_subs;
		}
	}
		
	_use->Enable (any_subs > 0);
	bool const use = _use->GetValue ();
	
	_x_offset->Enable (any_subs > 0 && use);
	_y_offset->Enable (any_subs > 0 && use);
	_x_scale->Enable (any_subs > 0 && use);
	_y_scale->Enable (any_subs > 0 && use);
	_language->Enable (any_subs > 0 && use);
	_stream->Enable (ffmpeg_subs == 1);
	_subtitle_view_button->Enable (subrip_or_dcp_subs == 1);
	_fonts_dialog_button->Enable (subrip_or_dcp_subs == 1);
}

void
SubtitlePanel::stream_changed ()
{
	FFmpegContentList fc = _parent->selected_ffmpeg ();
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
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_subtitle_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::y_offset_changed ()
{
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_subtitle_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::x_scale_changed ()
{
	SubtitleContentList c = _parent->selected_subtitle ();
	if (c.size() == 1) {
		c.front()->set_subtitle_x_scale (_x_scale->GetValue() / 100.0);
	}
}

void
SubtitlePanel::y_scale_changed ()
{
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_subtitle_y_scale (_y_scale->GetValue() / 100.0);
	}
}

void
SubtitlePanel::language_changed ()
{
	SubtitleContentList c = _parent->selected_subtitle ();
	for (SubtitleContentList::iterator i = c.begin(); i != c.end(); ++i) {
		(*i)->set_subtitle_language (wx_to_std (_language->GetValue()));
	}
}

void
SubtitlePanel::content_selection_changed ()
{
	film_content_changed (FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (SubtitleContentProperty::USE_SUBTITLES);
	film_content_changed (SubtitleContentProperty::SUBTITLE_X_OFFSET);
	film_content_changed (SubtitleContentProperty::SUBTITLE_Y_OFFSET);
	film_content_changed (SubtitleContentProperty::SUBTITLE_X_SCALE);
	film_content_changed (SubtitleContentProperty::SUBTITLE_Y_SCALE);
	film_content_changed (SubtitleContentProperty::SUBTITLE_LANGUAGE);
	film_content_changed (SubtitleContentProperty::FONTS);
}

void
SubtitlePanel::subtitle_view_clicked ()
{
	if (_subtitle_view) {
		_subtitle_view->Destroy ();
		_subtitle_view = 0;
	}

	SubtitleContentList c = _parent->selected_subtitle ();
	assert (c.size() == 1);

	shared_ptr<SubtitleDecoder> decoder;
	
	shared_ptr<SubRipContent> sr = dynamic_pointer_cast<SubRipContent> (c.front ());
	if (sr) {
		decoder.reset (new SubRipDecoder (sr));
	}
	
	shared_ptr<DCPSubtitleContent> dc = dynamic_pointer_cast<DCPSubtitleContent> (c.front ());
	if (dc) {
		decoder.reset (new DCPSubtitleDecoder (dc));
	}
	
	if (decoder) {
		_subtitle_view = new SubtitleView (this, _parent->film(), decoder, c.front()->position ());
		_subtitle_view->Show ();
	}
}

void
SubtitlePanel::fonts_dialog_clicked ()
{
	if (_fonts_dialog) {
		_fonts_dialog->Destroy ();
		_fonts_dialog = 0;
	}

	SubtitleContentList c = _parent->selected_subtitle ();
	assert (c.size() == 1);

	_fonts_dialog = new FontsDialog (this, c.front ());
	_fonts_dialog->Show ();
}
