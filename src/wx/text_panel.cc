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

#include "text_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "text_view.h"
#include "content_panel.h"
#include "fonts_dialog.h"
#include "dcp_text_track_dialog.h"
#include "subtitle_appearance_dialog.h"
#include "lib/ffmpeg_content.h"
#include "lib/string_text_file_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/string_text_file_decoder.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/dcp_content.h"
#include "lib/text_content.h"
#include "lib/decoder_factory.h"
#include <wx/spinctrl.h>
#include <boost/foreach.hpp>

using std::vector;
using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

/** @param t Original text type of the content, if known */
TextPanel::TextPanel (ContentPanel* p, TextType t)
	: ContentSubPanel (p, std_to_wx(text_type_to_name(t)))
	, _text_view (0)
	, _fonts_dialog (0)
	, _original_type (t)
{
	wxString refer = _("Use this DCP's subtitle as OV and make VF");
	if (t == TEXT_CLOSED_CAPTION) {
		refer = _("Use this DCP's closed caption as OV and make VF");
	}

	_reference = new wxCheckBox (this, wxID_ANY, refer);
	_reference_note = new wxStaticText (this, wxID_ANY, wxT(""));
	_reference_note->Wrap (200);
	wxFont font = _reference_note->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_reference_note->SetFont(font);

	_use = new wxCheckBox (this, wxID_ANY, _("Use as"));
	_type = new wxChoice (this, wxID_ANY);
	_type->Append (_("open subtitles"));
	_type->Append (_("closed captions"));

	_burn = new wxCheckBox (this, wxID_ANY, _("Burn subtitles into image"));

	_offset_label = create_label (this, _("Offset"), true);
	_x_offset_label = create_label (this, _("X"), true);
	_x_offset = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(64, -1));
	_x_offset_pc_label = new wxStaticText (this, wxID_ANY, _("%"));
	_y_offset_label = create_label (this, _("Y"), true);
	_y_offset = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(64, -1));
	_y_offset_pc_label = new wxStaticText (this, wxID_ANY, _("%"));

	_scale_label = create_label (this, _("Scale"), true);
	_x_scale_label = create_label (this, _("X"), true);
	_x_scale = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(64, -1));
	_x_scale_pc_label = new wxStaticText (this, wxID_ANY, _("%"));
	_y_scale_label = create_label (this, _("Y"), true);
	_y_scale = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(64, -1));
	_y_scale_pc_label = new wxStaticText (this, wxID_ANY, _("%"));

	_line_spacing_label = create_label (this, _("Line spacing"), true);
	_line_spacing = new wxSpinCtrl (this);
	_line_spacing_pc_label = new wxStaticText (this, wxID_ANY, _("%"));

	_dcp_track_label = create_label (this, _("DCP track"), true);
	_dcp_track = new wxChoice (this, wxID_ANY);

	_language_label = create_label (this, _("Language"), true);
	_language = new wxTextCtrl (this, wxID_ANY);

	_stream_label = create_label (this, _("Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);

	_text_view_button = new wxButton (this, wxID_ANY, _("View..."));
	_fonts_dialog_button = new wxButton (this, wxID_ANY, _("Fonts..."));
	_appearance_dialog_button = new wxButton (this, wxID_ANY, _("Appearance..."));

	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_x_scale->SetRange (10, 1000);
	_y_scale->SetRange (10, 1000);
	_line_spacing->SetRange (10, 1000);

	update_dcp_tracks ();

	content_selection_changed ();

	_reference->Bind                (wxEVT_CHECKBOX, boost::bind (&TextPanel::reference_clicked, this));
	_use->Bind                      (wxEVT_CHECKBOX, boost::bind (&TextPanel::use_toggled, this));
	_type->Bind                     (wxEVT_CHOICE,   boost::bind (&TextPanel::type_changed, this));
	_burn->Bind                     (wxEVT_CHECKBOX, boost::bind (&TextPanel::burn_toggled, this));
	_x_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&TextPanel::x_offset_changed, this));
	_y_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&TextPanel::y_offset_changed, this));
	_x_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&TextPanel::x_scale_changed, this));
	_y_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&TextPanel::y_scale_changed, this));
	_line_spacing->Bind             (wxEVT_SPINCTRL, boost::bind (&TextPanel::line_spacing_changed, this));
	_dcp_track->Bind                (wxEVT_CHOICE,   boost::bind (&TextPanel::dcp_track_changed, this));
	_language->Bind                 (wxEVT_TEXT,     boost::bind (&TextPanel::language_changed, this));
	_stream->Bind                   (wxEVT_CHOICE,   boost::bind (&TextPanel::stream_changed, this));
	_text_view_button->Bind         (wxEVT_BUTTON,   boost::bind (&TextPanel::text_view_clicked, this));
	_fonts_dialog_button->Bind      (wxEVT_BUTTON,   boost::bind (&TextPanel::fonts_dialog_clicked, this));
	_appearance_dialog_button->Bind (wxEVT_BUTTON,   boost::bind (&TextPanel::appearance_dialog_clicked, this));

	add_to_grid();
}

void
TextPanel::add_to_grid ()
{
	Config::Interface const interface = Config::instance()->interface_complexity();

	int r = 0;

	_reference->Show (interface == Config::INTERFACE_FULL);
	_reference_note->Show (interface == Config::INTERFACE_FULL);

	if (interface == Config::INTERFACE_FULL) {
		wxBoxSizer* reference_sizer = new wxBoxSizer (wxVERTICAL);
		reference_sizer->Add (_reference, 0);
		reference_sizer->Add (_reference_note, 0);
		_grid->Add (reference_sizer, wxGBPosition(r, 0), wxGBSpan(1, 4));
		++r;
	}

	wxBoxSizer* use = new wxBoxSizer (wxHORIZONTAL);
	use->Add (_use, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
	use->Add (_type, 1, wxEXPAND, 0);
	_grid->Add (use, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_grid->Add (_burn, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_sizer (_grid, _offset_label, true, wxGBPosition (r, 0));
	wxBoxSizer* offset = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (offset, _x_offset_label, true);
	offset->Add (_x_offset, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	offset->Add (_x_offset_pc_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP * 2);
	add_label_to_sizer (offset, _y_offset_label, true);
	offset->Add (_y_offset, 0);
	add_label_to_sizer (offset, _y_offset_pc_label, false);
	_grid->Add (offset, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_grid, _scale_label, true, wxGBPosition (r, 0));
	wxBoxSizer* scale = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (scale, _x_scale_label, true);
	scale->Add (_x_scale, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	scale->Add (_x_scale_pc_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP * 2);
	add_label_to_sizer (scale, _y_scale_label, true);
	scale->Add (_y_scale, 0);
	add_label_to_sizer (scale, _y_scale_pc_label, false);
	_grid->Add (scale, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_sizer (_grid, _line_spacing_label, true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_line_spacing);
		add_label_to_sizer (s, _line_spacing_pc_label, false);
		_grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	add_label_to_sizer (_grid, _dcp_track_label, true, wxGBPosition(r, 0));
	_grid->Add (_dcp_track, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	add_label_to_sizer (_grid, _language_label, true, wxGBPosition (r, 0));
	_grid->Add (_language, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_grid, _stream_label, true, wxGBPosition (r, 0));
	_grid->Add (_stream, wxGBPosition (r, 1));
	++r;

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		s->Add (_text_view_button, 1, wxALL, DCPOMATIC_SIZER_GAP);
		s->Add (_fonts_dialog_button, 1, wxALL, DCPOMATIC_SIZER_GAP);
		s->Add (_appearance_dialog_button, 1, wxALL, DCPOMATIC_SIZER_GAP);

		_grid->Add (s, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;
	}
}

void
TextPanel::update_dcp_track_selection ()
{
	optional<DCPTextTrack> selected;
	bool many = false;
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text()) {
		shared_ptr<TextContent> t = i->text_of_original_type(_original_type);
		if (t) {
			optional<DCPTextTrack> dt = t->dcp_track();
			if (dt && selected && *dt != *selected) {
				many = true;
			} else if (!selected) {
				selected = dt;
			}
		}
	}

	int n = 0;
	BOOST_FOREACH (DCPTextTrack i, _parent->film()->closed_caption_tracks()) {
		if (!many && selected && *selected == i) {
			_dcp_track->SetSelection (n);
		}
		++n;
	}

	if (!selected || many) {
		_dcp_track->SetSelection (wxNOT_FOUND);
	}
}

void
TextPanel::update_dcp_tracks ()
{
	_dcp_track->Clear ();
	BOOST_FOREACH (DCPTextTrack i, _parent->film()->closed_caption_tracks()) {
		_dcp_track->Append (std_to_wx(i.summary()));
	}

	if (_parent->film()->closed_caption_tracks().size() < 6) {
		_dcp_track->Append (_("Add new..."));
	}

	update_dcp_track_selection ();
}

void
TextPanel::dcp_track_changed ()
{
	optional<DCPTextTrack> track;

	if (_dcp_track->GetSelection() == int(_dcp_track->GetCount()) - 1) {
		DCPTextTrackDialog* d = new DCPTextTrackDialog (this);
		if (d->ShowModal() == wxID_OK) {
			track = d->get();
		}
		d->Destroy ();
	} else {
		/* Find the DCPTextTrack that was selected */
		BOOST_FOREACH (DCPTextTrack i, _parent->film()->closed_caption_tracks()) {
			if (i.summary() == wx_to_std(_dcp_track->GetStringSelection())) {
				track = i;
			}
		}
	}

	if (track) {
		BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text()) {
			shared_ptr<TextContent> t = i->text_of_original_type(_original_type);
			if (t && t->type() == TEXT_CLOSED_CAPTION) {
				t->set_dcp_track(*track);
			}
		}
	}

	update_dcp_tracks ();
}

void
TextPanel::film_changed (Film::Property property)
{
	if (property == Film::CONTENT || property == Film::REEL_TYPE) {
		setup_sensitivity ();
	}
}

void
TextPanel::film_content_changed (int property)
{
	FFmpegContentList fc = _parent->selected_ffmpeg ();
	ContentList sc = _parent->selected_text ();

	shared_ptr<FFmpegContent> fcs;
	if (fc.size() == 1) {
		fcs = fc.front ();
	}

	shared_ptr<Content> scs;
	if (sc.size() == 1) {
		scs = sc.front ();
	}

	shared_ptr<TextContent> text;
	if (scs) {
		text = scs->text_of_original_type(_original_type);
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
	} else if (property == TextContentProperty::USE) {
		checked_set (_use, text ? text->use() : false);
		setup_sensitivity ();
	} else if (property == TextContentProperty::TYPE) {
		if (text) {
			switch (text->type()) {
			case TEXT_OPEN_SUBTITLE:
				_type->SetSelection (0);
				break;
			case TEXT_CLOSED_CAPTION:
				_type->SetSelection (1);
				break;
			default:
				DCPOMATIC_ASSERT (false);
			}
		} else {
			_type->SetSelection (0);
		}
		setup_sensitivity ();
		update_dcp_track_selection ();
	} else if (property == TextContentProperty::BURN) {
		checked_set (_burn, text ? text->burn() : false);
	} else if (property == TextContentProperty::X_OFFSET) {
		checked_set (_x_offset, text ? lrint (text->x_offset() * 100) : 0);
	} else if (property == TextContentProperty::Y_OFFSET) {
		checked_set (_y_offset, text ? lrint (text->y_offset() * 100) : 0);
	} else if (property == TextContentProperty::X_SCALE) {
		checked_set (_x_scale, text ? lrint (text->x_scale() * 100) : 100);
	} else if (property == TextContentProperty::Y_SCALE) {
		checked_set (_y_scale, text ? lrint (text->y_scale() * 100) : 100);
	} else if (property == TextContentProperty::LINE_SPACING) {
		checked_set (_line_spacing, text ? lrint (text->line_spacing() * 100) : 100);
	} else if (property == TextContentProperty::LANGUAGE) {
		checked_set (_language, text ? text->language() : "");
	} else if (property == TextContentProperty::DCP_TRACK) {
		update_dcp_track_selection ();
	} else if (property == DCPContentProperty::REFERENCE_TEXT) {
		if (scs) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (scs);
			checked_set (_reference, dcp ? dcp->reference_text(_original_type) : false);
		} else {
			checked_set (_reference, false);
		}

		setup_sensitivity ();
	} else if (property == DCPContentProperty::TEXTS) {
		setup_sensitivity ();
	}
}

void
TextPanel::use_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text()) {
		i->text_of_original_type(_original_type)->set_use (_use->GetValue());
	}
}

/** @return the text type that is currently selected in the drop-down */
TextType
TextPanel::current_type () const
{
	switch (_type->GetSelection()) {
	case 0:
		return TEXT_OPEN_SUBTITLE;
	case 1:
		return TEXT_CLOSED_CAPTION;
	}

	return TEXT_UNKNOWN;
}

void
TextPanel::type_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text()) {
		i->text_of_original_type(_original_type)->set_type (current_type ());
	}
}

void
TextPanel::burn_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_burn (_burn->GetValue());
	}
}

void
TextPanel::setup_sensitivity ()
{
	int any_subs = 0;
	int ffmpeg_subs = 0;
	ContentList sel = _parent->selected_text ();
	BOOST_FOREACH (shared_ptr<Content> i, sel) {
		/* These are the content types that could include subtitles */
		shared_ptr<const FFmpegContent> fc = boost::dynamic_pointer_cast<const FFmpegContent> (i);
		shared_ptr<const StringTextFileContent> sc = boost::dynamic_pointer_cast<const StringTextFileContent> (i);
		shared_ptr<const DCPContent> dc = boost::dynamic_pointer_cast<const DCPContent> (i);
		shared_ptr<const DCPSubtitleContent> dsc = boost::dynamic_pointer_cast<const DCPSubtitleContent> (i);
		if (fc) {
			if (!fc->text.empty()) {
				++ffmpeg_subs;
				++any_subs;
			}
		} else if (sc || dc || dsc) {
			/* XXX: in the future there could be bitmap subs from DCPs */
			++any_subs;
		}
	}

	/* Decide whether we can reference these subs */

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent> (sel.front ());
	}

	string why_not;
	bool const can_reference = dcp && dcp->can_reference_text (_parent->film(), _original_type, why_not);
	setup_refer_button (_reference, _reference_note, dcp, can_reference, why_not);

	bool const reference = _reference->GetValue ();

	TextType const type = current_type ();

	/* Set up sensitivity */
	_use->Enable (!reference && any_subs > 0);
	bool const use = _use->GetValue ();
	_type->Enable (!reference && any_subs > 0 && use);
	_burn->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_x_offset->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_y_offset->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_x_scale->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_y_scale->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_line_spacing->Enable (!reference && use && type == TEXT_OPEN_SUBTITLE);
	_dcp_track->Enable (!reference && any_subs > 0 && use && type == TEXT_CLOSED_CAPTION);
	_language->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
	_stream->Enable (!reference && ffmpeg_subs == 1);
	_text_view_button->Enable (!reference);
	_fonts_dialog_button->Enable (!reference && type == TEXT_OPEN_SUBTITLE);
	_appearance_dialog_button->Enable (!reference && any_subs > 0 && use && type == TEXT_OPEN_SUBTITLE);
}

void
TextPanel::stream_changed ()
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
TextPanel::x_offset_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
TextPanel::y_offset_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
TextPanel::x_scale_changed ()
{
	ContentList c = _parent->selected_text ();
	if (c.size() == 1) {
		c.front()->text_of_original_type(_original_type)->set_x_scale (_x_scale->GetValue() / 100.0);
	}
}

void
TextPanel::y_scale_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_y_scale (_y_scale->GetValue() / 100.0);
	}
}

void
TextPanel::line_spacing_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_line_spacing (_line_spacing->GetValue() / 100.0);
	}
}

void
TextPanel::language_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_language (wx_to_std (_language->GetValue()));
	}
}

void
TextPanel::content_selection_changed ()
{
	film_content_changed (FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (TextContentProperty::USE);
	film_content_changed (TextContentProperty::BURN);
	film_content_changed (TextContentProperty::X_OFFSET);
	film_content_changed (TextContentProperty::Y_OFFSET);
	film_content_changed (TextContentProperty::X_SCALE);
	film_content_changed (TextContentProperty::Y_SCALE);
	film_content_changed (TextContentProperty::LINE_SPACING);
	film_content_changed (TextContentProperty::LANGUAGE);
	film_content_changed (TextContentProperty::FONTS);
	film_content_changed (TextContentProperty::TYPE);
	film_content_changed (TextContentProperty::DCP_TRACK);
	film_content_changed (DCPContentProperty::REFERENCE_TEXT);
}

void
TextPanel::text_view_clicked ()
{
	if (_text_view) {
		_text_view->Destroy ();
		_text_view = 0;
	}

	ContentList c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	shared_ptr<Decoder> decoder = decoder_factory (_parent->film(), c.front(), false);

	if (decoder) {
		_text_view = new TextView (this, _parent->film(), c.front(), c.front()->text_of_original_type(_original_type), decoder, _parent->film_viewer());
		_text_view->Show ();
	}
}

void
TextPanel::fonts_dialog_clicked ()
{
	if (_fonts_dialog) {
		_fonts_dialog->Destroy ();
		_fonts_dialog = 0;
	}

	ContentList c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	_fonts_dialog = new FontsDialog (this, c.front(), c.front()->text_of_original_type(_original_type));
	_fonts_dialog->Show ();
}

void
TextPanel::reference_clicked ()
{
	ContentList c = _parent->selected ();
	if (c.size() != 1) {
		return;
	}

	shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (c.front ());
	if (!d) {
		return;
	}

	d->set_reference_text (_original_type, _reference->GetValue ());
}

void
TextPanel::appearance_dialog_clicked ()
{
	ContentList c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	SubtitleAppearanceDialog* d = new SubtitleAppearanceDialog (this, _parent->film(), c.front(), c.front()->text_of_original_type(_original_type));
	if (d->ShowModal () == wxID_OK) {
		d->apply ();
	}
	d->Destroy ();
}
