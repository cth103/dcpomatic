/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "static_text.h"
#include "check_box.h"
#include "dcpomatic_button.h"
#include "film_viewer.h"
#include "lib/job_manager.h"
#include "lib/ffmpeg_content.h"
#include "lib/string_text_file_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/string_text_file_decoder.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/dcp_content.h"
#include "lib/text_content.h"
#include "lib/decoder_factory.h"
#include "lib/analyse_subtitles_job.h"
#include "lib/subtitle_analysis.h"
#include <wx/spinctrl.h>

using std::vector;
using std::string;
using std::list;
using std::cout;
using std::shared_ptr;
using boost::optional;
using std::dynamic_pointer_cast;
using boost::bind;

/** @param t Original text type of the content, if known */
TextPanel::TextPanel (ContentPanel* p, TextType t)
	: ContentSubPanel (p, std_to_wx(text_type_to_name(t)))
	, _outline_subtitles (0)
	, _dcp_track_label (0)
	, _dcp_track (0)
	, _text_view (0)
	, _fonts_dialog (0)
	, _original_type (t)
	, _loading_analysis (false)
{
	wxString refer = _("Use this DCP's subtitle as OV and make VF");
	if (t == TextType::CLOSED_CAPTION) {
		refer = _("Use this DCP's closed caption as OV and make VF");
	}

	_reference = new CheckBox (this, refer);
	_reference_note = new StaticText (this, wxT(""));
	_reference_note->Wrap (200);
	wxFont font = _reference_note->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_reference_note->SetFont(font);

	_use = new CheckBox (this, _("Use as"));
	_type = new wxChoice (this, wxID_ANY);
	_type->Append (_("open subtitles"));
	_type->Append (_("closed captions"));

	_burn = new CheckBox (this, _("Burn subtitles into image"));

#ifdef __WXGTK3__
	int const spin_width = 118;
#else
	int const spin_width = 56;
#endif

	_offset_label = create_label (this, _("Offset"), true);
	_x_offset_label = create_label (this, _("X"), true);
	_x_offset = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1));
	_x_offset_pc_label = new StaticText (this, _("%"));
	_y_offset_label = create_label (this, _("Y"), true);
	_y_offset = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1));
	_y_offset_pc_label = new StaticText (this, _("%"));

	_scale_label = create_label (this, _("Scale"), true);
	_x_scale_label = create_label (this, _("X"), true);
	_x_scale = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1));
	_x_scale_pc_label = new StaticText (this, _("%"));
	_y_scale_label = create_label (this, S_("Coord|Y"), true);
	_y_scale = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(spin_width, -1));
	_y_scale_pc_label = new StaticText (this, _("%"));

	_line_spacing_label = create_label (this, _("Line spacing"), true);
	_line_spacing = new wxSpinCtrl (this);
	_line_spacing_pc_label = new StaticText (this, _("%"));

	_stream_label = create_label (this, _("Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);

	_text_view_button = new Button (this, _("View..."));
	_fonts_dialog_button = new Button (this, _("Fonts..."));
	_appearance_dialog_button = new Button (this, _("Appearance..."));

	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_x_scale->SetRange (10, 1000);
	_y_scale->SetRange (10, 1000);
	_line_spacing->SetRange (10, 1000);

	_reference->Bind                (wxEVT_CHECKBOX, boost::bind (&TextPanel::reference_clicked, this));
	_use->Bind                      (wxEVT_CHECKBOX, boost::bind (&TextPanel::use_toggled, this));
	_type->Bind                     (wxEVT_CHOICE,   boost::bind (&TextPanel::type_changed, this));
	_burn->Bind                     (wxEVT_CHECKBOX, boost::bind (&TextPanel::burn_toggled, this));
	_x_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&TextPanel::x_offset_changed, this));
	_y_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&TextPanel::y_offset_changed, this));
	_x_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&TextPanel::x_scale_changed, this));
	_y_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&TextPanel::y_scale_changed, this));
	_line_spacing->Bind             (wxEVT_SPINCTRL, boost::bind (&TextPanel::line_spacing_changed, this));
	_stream->Bind                   (wxEVT_CHOICE,   boost::bind (&TextPanel::stream_changed, this));
	_text_view_button->Bind         (wxEVT_BUTTON,   boost::bind (&TextPanel::text_view_clicked, this));
	_fonts_dialog_button->Bind      (wxEVT_BUTTON,   boost::bind (&TextPanel::fonts_dialog_clicked, this));
	_appearance_dialog_button->Bind (wxEVT_BUTTON,   boost::bind (&TextPanel::appearance_dialog_clicked, this));

	add_to_grid();
	content_selection_changed ();
}

void
TextPanel::setup_visibility ()
{
	switch (current_type()) {
	case TextType::OPEN_SUBTITLE:
		if (_dcp_track_label) {
			_dcp_track_label->Destroy ();
			_dcp_track_label = 0;
		}
		if (_dcp_track) {
			_dcp_track->Destroy ();
			_dcp_track = 0;
		}
		if (!_outline_subtitles) {
			_outline_subtitles = new CheckBox (this, _("Show subtitle area"));
			_outline_subtitles->Bind (wxEVT_CHECKBOX, boost::bind (&TextPanel::outline_subtitles_changed, this));
			_grid->Add (_outline_subtitles, wxGBPosition(_outline_subtitles_row, 0), wxGBSpan(1, 2));
		}

		break;
	case TextType::CLOSED_CAPTION:
		if (!_dcp_track_label) {
			_dcp_track_label = create_label (this, _("CCAP track"), true);
			add_label_to_sizer (_grid, _dcp_track_label, true, wxGBPosition(_ccap_track_row, 0));
		}
		if (!_dcp_track) {
			_dcp_track = new wxChoice (this, wxID_ANY);
			_dcp_track->Bind (wxEVT_CHOICE, boost::bind(&TextPanel::dcp_track_changed, this));
			_grid->Add (_dcp_track, wxGBPosition(_ccap_track_row, 1), wxDefaultSpan, wxEXPAND);
			update_dcp_tracks ();
			film_content_changed (TextContentProperty::DCP_TRACK);
		}
		if (_outline_subtitles) {
			_outline_subtitles->Destroy ();
			_outline_subtitles = 0;
			clear_outline_subtitles ();
		}
		break;
	default:
		break;
	}

	_grid->Layout ();
}

void
TextPanel::add_to_grid ()
{
	int r = 0;

	wxBoxSizer* reference_sizer = new wxBoxSizer (wxVERTICAL);
	reference_sizer->Add (_reference, 0);
	reference_sizer->Add (_reference_note, 0);
	_grid->Add (reference_sizer, wxGBPosition(r, 0), wxGBSpan(1, 4));
	++r;

	wxBoxSizer* use = new wxBoxSizer (wxHORIZONTAL);
	use->Add (_use, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
	use->Add (_type, 1, wxEXPAND, 0);
	_grid->Add (use, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_grid->Add (_burn, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_outline_subtitles_row = r;
	++r;

	add_label_to_sizer (_grid, _offset_label, true, wxGBPosition (r, 0));
	wxBoxSizer* offset = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (offset, _x_offset_label, true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	offset->Add (_x_offset, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	offset->Add (_x_offset_pc_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP * 2);
#ifdef __WXGTK3__
	_grid->Add (offset, wxGBPosition(r, 1));
	++r;
	offset = new wxBoxSizer (wxHORIZONTAL);
#endif
	add_label_to_sizer (offset, _y_offset_label, true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	offset->Add (_y_offset, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	add_label_to_sizer (offset, _y_offset_pc_label, false, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_grid->Add (offset, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_grid, _scale_label, true, wxGBPosition (r, 0));
	wxBoxSizer* scale = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (scale, _x_scale_label, true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	scale->Add (_x_scale, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	scale->Add (_x_scale_pc_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP * 2);
#ifdef __WXGTK3__
	_grid->Add (scale, wxGBPosition(r, 1));
	++r;
	scale = new wxBoxSizer (wxHORIZONTAL);
#endif
	add_label_to_sizer (scale, _y_scale_label, true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
	scale->Add (_y_scale, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	add_label_to_sizer (scale, _y_scale_pc_label, false, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL);
	_grid->Add (scale, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_sizer (_grid, _line_spacing_label, true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_line_spacing);
		add_label_to_sizer (s, _line_spacing_pc_label, false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
		_grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	_ccap_track_row = r;
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

	setup_visibility ();
}

void
TextPanel::update_dcp_track_selection ()
{
	DCPOMATIC_ASSERT (_dcp_track);

	optional<DCPTextTrack> selected;
	bool many = false;
	for (auto i: _parent->selected_text()) {
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
	for (auto i: _parent->film()->closed_caption_tracks()) {
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
	DCPOMATIC_ASSERT (_dcp_track);

	_dcp_track->Clear ();
	for (auto i: _parent->film()->closed_caption_tracks()) {
		/* XXX: don't display the "magic" track which has empty name and language;
		   this is a nasty hack (see also Film::closed_caption_tracks)
		*/
		if (!i.name.empty() || !i.language.empty()) {
			_dcp_track->Append (std_to_wx(i.summary()));
		}
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
		for (auto i: _parent->film()->closed_caption_tracks()) {
			if (i.summary() == wx_to_std(_dcp_track->GetStringSelection())) {
				track = i;
			}
		}
	}

	if (track) {
		for (auto i: _parent->selected_text()) {
			shared_ptr<TextContent> t = i->text_of_original_type(_original_type);
			if (t && t->type() == TextType::CLOSED_CAPTION) {
				t->set_dcp_track(*track);
			}
		}
	}

	update_dcp_tracks ();
}

void
TextPanel::film_changed (Film::Property property)
{
	if (property == Film::Property::CONTENT || property == Film::Property::REEL_TYPE || property == Film::Property::INTEROP) {
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
		clear_outline_subtitles ();
	} else if (property == TextContentProperty::USE) {
		checked_set (_use, text ? text->use() : false);
		setup_sensitivity ();
		clear_outline_subtitles ();
	} else if (property == TextContentProperty::TYPE) {
		if (text) {
			switch (text->type()) {
			case TextType::OPEN_SUBTITLE:
				_type->SetSelection (0);
				break;
			case TextType::CLOSED_CAPTION:
				_type->SetSelection (1);
				break;
			default:
				DCPOMATIC_ASSERT (false);
			}
		} else {
			_type->SetSelection (0);
		}
		setup_sensitivity ();
		setup_visibility ();
	} else if (property == TextContentProperty::BURN) {
		checked_set (_burn, text ? text->burn() : false);
	} else if (property == TextContentProperty::X_OFFSET) {
		checked_set (_x_offset, text ? lrint (text->x_offset() * 100) : 0);
		update_outline_subtitles_in_viewer ();
	} else if (property == TextContentProperty::Y_OFFSET) {
		checked_set (_y_offset, text ? lrint (text->y_offset() * 100) : 0);
		update_outline_subtitles_in_viewer ();
	} else if (property == TextContentProperty::X_SCALE) {
		checked_set (_x_scale, text ? lrint (text->x_scale() * 100) : 100);
		clear_outline_subtitles ();
	} else if (property == TextContentProperty::Y_SCALE) {
		checked_set (_y_scale, text ? lrint (text->y_scale() * 100) : 100);
		clear_outline_subtitles ();
	} else if (property == TextContentProperty::LINE_SPACING) {
		checked_set (_line_spacing, text ? lrint (text->line_spacing() * 100) : 100);
		clear_outline_subtitles ();
	} else if (property == TextContentProperty::DCP_TRACK) {
		if (_dcp_track) {
			update_dcp_track_selection ();
		}
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
	} else if (property == ContentProperty::TRIM_START) {
		setup_sensitivity ();
	}
}

void
TextPanel::use_toggled ()
{
	for (auto i: _parent->selected_text()) {
		i->text_of_original_type(_original_type)->set_use (_use->GetValue());
	}
}

/** @return the text type that is currently selected in the drop-down */
TextType
TextPanel::current_type () const
{
	switch (_type->GetSelection()) {
	case 0:
		return TextType::OPEN_SUBTITLE;
	case 1:
		return TextType::CLOSED_CAPTION;
	}

	return TextType::UNKNOWN;
}

void
TextPanel::type_changed ()
{
	for (auto i: _parent->selected_text()) {
		i->text_of_original_type(_original_type)->set_type (current_type ());
	}

	setup_visibility ();
}

void
TextPanel::burn_toggled ()
{
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_burn (_burn->GetValue());
	}
}

void
TextPanel::setup_sensitivity ()
{
	int any_subs = 0;
	/* We currently assume that FFmpeg subtitles are bitmapped */
	int ffmpeg_subs = 0;
	/* DCP subs can't have their line spacing changed */
	int dcp_subs = 0;
	ContentList sel = _parent->selected_text ();
	for (auto i: sel) {
		/* These are the content types that could include subtitles */
		shared_ptr<const FFmpegContent> fc = std::dynamic_pointer_cast<const FFmpegContent> (i);
		shared_ptr<const StringTextFileContent> sc = std::dynamic_pointer_cast<const StringTextFileContent> (i);
		shared_ptr<const DCPContent> dc = std::dynamic_pointer_cast<const DCPContent> (i);
		shared_ptr<const DCPSubtitleContent> dsc = std::dynamic_pointer_cast<const DCPSubtitleContent> (i);
		if (fc) {
			if (!fc->text.empty()) {
				++ffmpeg_subs;
				++any_subs;
			}
		} else if (dc || dsc) {
			++dcp_subs;
			++any_subs;
		} else if (sc) {
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
	wxString cannot;
	if (why_not.empty()) {
		cannot = _("Cannot reference this DCP's subtitles or captions.");
	} else {
		cannot = _("Cannot reference this DCP's subtitles or captions: ") + std_to_wx(why_not);
	}
	setup_refer_button (_reference, _reference_note, dcp, can_reference, cannot);

	bool const reference = _reference->GetValue ();

	TextType const type = current_type ();

	/* Set up _type */
	_type->Clear ();
	_type->Append (_("open subtitles"));
	if (ffmpeg_subs == 0) {
		_type->Append (_("closed captions"));
	}

	switch (type) {
	case TextType::OPEN_SUBTITLE:
		_type->SetSelection (0);
		break;
	case TextType::CLOSED_CAPTION:
		if (_type->GetCount() > 1) {
			_type->SetSelection (1);
		}
		break;
	default:
		break;
	}

	/* Set up sensitivity */
	_use->Enable (!reference && any_subs > 0);
	bool const use = _use->GetValue ();
	if (_outline_subtitles) {
		_outline_subtitles->Enable (!_loading_analysis && any_subs && use && type == TextType::OPEN_SUBTITLE);
	}
	_type->Enable (!reference && any_subs > 0 && use);
	_burn->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
	_x_offset->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
	_y_offset->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
	_x_scale->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
	_y_scale->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
	_line_spacing->Enable (!reference && use && type == TextType::OPEN_SUBTITLE && dcp_subs < any_subs);
	_stream->Enable (!reference && ffmpeg_subs == 1);
	/* Ideally we would check here to see if the FFmpeg content has "string" subs (i.e. not bitmaps) */
	_text_view_button->Enable (!reference && any_subs > 0 && ffmpeg_subs == 0);
	_fonts_dialog_button->Enable (!reference && any_subs > 0 && ffmpeg_subs == 0 && type == TextType::OPEN_SUBTITLE);
	_appearance_dialog_button->Enable (!reference && any_subs > 0 && use && type == TextType::OPEN_SUBTITLE);
}

void
TextPanel::stream_changed ()
{
	auto fc = _parent->selected_ffmpeg ();
	if (fc.size() != 1) {
		return;
	}

	auto fcs = fc.front ();

	auto a = fcs->subtitle_streams ();
	auto i = a.begin ();
	auto const s = string_client_data (_stream->GetClientObject (_stream->GetSelection ()));
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
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
TextPanel::y_offset_changed ()
{
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
TextPanel::x_scale_changed ()
{
	auto c = _parent->selected_text ();
	if (c.size() == 1) {
		c.front()->text_of_original_type(_original_type)->set_x_scale (_x_scale->GetValue() / 100.0);
	}
}

void
TextPanel::y_scale_changed ()
{
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_y_scale (_y_scale->GetValue() / 100.0);
	}
}

void
TextPanel::line_spacing_changed ()
{
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_line_spacing (_line_spacing->GetValue() / 100.0);
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

	shared_ptr<Decoder> decoder = decoder_factory (_parent->film(), c.front(), false, false, shared_ptr<Decoder>());

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
		_fonts_dialog = nullptr;
	}

	auto c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	_fonts_dialog = new FontsDialog (this, c.front(), c.front()->text_of_original_type(_original_type));
	_fonts_dialog->Show ();
}

void
TextPanel::reference_clicked ()
{
	auto c = _parent->selected ();
	if (c.size() != 1) {
		return;
	}

	auto d = dynamic_pointer_cast<DCPContent> (c.front ());
	if (!d) {
		return;
	}

	d->set_reference_text (_original_type, _reference->GetValue ());
}

void
TextPanel::appearance_dialog_clicked ()
{
	auto c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	auto d = new SubtitleAppearanceDialog (this, _parent->film(), c.front(), c.front()->text_of_original_type(_original_type));
	if (d->ShowModal () == wxID_OK) {
		d->apply ();
	}
	d->Destroy ();
}



/** The user has clicked on the outline subtitles check box */
void
TextPanel::outline_subtitles_changed ()
{
	if (_outline_subtitles->GetValue()) {
		_analysis_content = _parent->selected_text().front();
		try_to_load_analysis ();
	} else {
		clear_outline_subtitles ();
	}
}


void
TextPanel::try_to_load_analysis ()
{
	if (_loading_analysis) {
		return;
	}

	_loading_analysis = true;
	setup_sensitivity ();
	_analysis.reset ();

	auto content = _analysis_content.lock ();
	if (!content) {
		_loading_analysis = false;
		setup_sensitivity ();
		return;
	}

	auto const path = _parent->film()->subtitle_analysis_path(content);

	if (!boost::filesystem::exists(path)) {
		for (auto i: JobManager::instance()->get()) {
			if (dynamic_pointer_cast<AnalyseSubtitlesJob>(i)) {
				i->cancel ();
			}
		}

		JobManager::instance()->analyse_subtitles (
			_parent->film(), content, _analysis_finished_connection, bind(&TextPanel::analysis_finished, this)
			);
		return;
	}

	try {
		_analysis.reset (new SubtitleAnalysis(path));
	} catch (OldFormatError& e) {
		/* An old analysis file: recreate it */
		JobManager::instance()->analyse_subtitles (
			_parent->film(), content, _analysis_finished_connection, bind(&TextPanel::analysis_finished, this)
			);
		return;
        }

	update_outline_subtitles_in_viewer ();
	_loading_analysis = false;
	setup_sensitivity ();
}


void
TextPanel::update_outline_subtitles_in_viewer ()
{
	auto fv = _parent->film_viewer().lock();
	if (!fv) {
		return;
	}

	if (_analysis) {
		auto rect = _analysis->bounding_box ();
		if (rect) {
			auto content = _analysis_content.lock ();
			DCPOMATIC_ASSERT (content);
			rect->x += content->text.front()->x_offset();
			rect->y += content->text.front()->y_offset();
		}
		fv->set_outline_subtitles (rect);
	} else {
		fv->set_outline_subtitles (optional<dcpomatic::Rect<double> >());
	}
}


/** Remove any current subtitle outline display */
void
TextPanel::clear_outline_subtitles ()
{
	_analysis.reset ();
	update_outline_subtitles_in_viewer ();
	if (_outline_subtitles) {
		_outline_subtitles->SetValue (false);
	}
}


void
TextPanel::analysis_finished ()
{
	auto content = _analysis_content.lock ();
	if (!content) {
		_loading_analysis = false;
		setup_sensitivity ();
		return;
	}

	if (!boost::filesystem::exists(_parent->film()->subtitle_analysis_path(content))) {
		/* We analysed and still nothing showed up, so maybe it was cancelled or it failed.
		   Give up.
		*/
		error_dialog (_parent->window(), _("Could not analyse subtitles."));
		clear_outline_subtitles ();
		_loading_analysis = false;
		setup_sensitivity ();
		return;
	}

	_loading_analysis = false;
	try_to_load_analysis ();
}

