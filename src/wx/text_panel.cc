/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "check_box.h"
#include "content_panel.h"
#include "dcp_text_track_dialog.h"
#include "dcpomatic_button.h"
#include "dcpomatic_spin_ctrl.h"
#include "film_editor.h"
#include "film_viewer.h"
#include "fonts_dialog.h"
#include "language_tag_widget.h"
#include "static_text.h"
#include "subtitle_appearance_dialog.h"
#include "text_panel.h"
#include "text_view.h"
#include "wx_util.h"
#include "lib/analyse_subtitles_job.h"
#include "lib/dcp_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/decoder_factory.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/scope_guard.h"
#include "lib/string_text_file_content.h"
#include "lib/string_text_file_decoder.h"
#include "lib/subtitle_analysis.h"
#include "lib/text_content.h"
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/spinctrl.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::bind;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @param t Original text type of the content, if known */
TextPanel::TextPanel (ContentPanel* p, TextType t)
	: ContentSubPanel (p, std_to_wx(text_type_to_name(t)))
	, _original_type (t)
{

}


void
TextPanel::create ()
{
	wxString refer = _("Use this DCP's subtitle as OV and make VF");
	if (_original_type == TextType::CLOSED_CAPTION) {
		refer = _("Use this DCP's closed caption as OV and make VF");
	}

	_use = new CheckBox (this, _("Use as"));
	_type = new wxChoice (this, wxID_ANY);
	_type->Append (_("open subtitles"));
	_type->Append (_("closed captions"));

	_burn = new CheckBox (this, _("Burn subtitles into image"));

	_offset_label = create_label (this, _("Offset"), true);
	_x_offset_label = create_label (this, _("X"), true);
	_x_offset = new SpinCtrl (this, DCPOMATIC_SPIN_CTRL_WIDTH);
	_x_offset_pc_label = new StaticText (this, _("%"));
	_y_offset_label = create_label (this, _("Y"), true);
	_y_offset = new SpinCtrl (this, DCPOMATIC_SPIN_CTRL_WIDTH);
	_y_offset_pc_label = new StaticText (this, _("%"));

	_scale_label = create_label (this, _("Scale"), true);
	_x_scale_label = create_label (this, _("X"), true);
	_x_scale = new SpinCtrl (this, DCPOMATIC_SPIN_CTRL_WIDTH);
	_x_scale_pc_label = new StaticText (this, _("%"));
	_y_scale_label = create_label (this, S_("Coord|Y"), true);
	_y_scale = new SpinCtrl (this, DCPOMATIC_SPIN_CTRL_WIDTH);
	_y_scale_pc_label = new StaticText (this, _("%"));

	_line_spacing_label = create_label (this, _("Line spacing"), true);
	_line_spacing = new SpinCtrl (this, DCPOMATIC_SPIN_CTRL_WIDTH);
	_line_spacing_pc_label = new StaticText (this, _("%"));

	_stream_label = create_label (this, _("Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);

	_text_view_button = new Button (this, _("View..."));
	_fonts_dialog_button = new Button (this, _("Fonts..."));
	_appearance_dialog_button = new Button (this, _("Appearance..."));

	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_x_scale->SetRange (0, 1000);
	_y_scale->SetRange (0, 1000);
	_line_spacing->SetRange (0, 1000);

	_use->bind(&TextPanel::use_toggled, this);
	_type->Bind                     (wxEVT_CHOICE,   boost::bind (&TextPanel::type_changed, this));
	_burn->bind(&TextPanel::burn_toggled, this);
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

	_sizer->Layout ();
}


void
TextPanel::setup_visibility ()
{
	switch (current_type()) {
	case TextType::OPEN_SUBTITLE:
		if (_dcp_track_label) {
			_dcp_track_label->Destroy ();
			_dcp_track_label = nullptr;
		}
		if (_dcp_track) {
			_dcp_track->Destroy ();
			_dcp_track = nullptr;
		}
		if (!_outline_subtitles) {
			_outline_subtitles = new CheckBox (this, _("Show subtitle area"));
			_outline_subtitles->bind(&TextPanel::outline_subtitles_changed, this);
			_grid->Add (_outline_subtitles, wxGBPosition(_outline_subtitles_row, 0), wxGBSpan(1, 2));
		}
		if (!_language) {
			_language_label = create_label (this, _("Language"), true);
			add_label_to_sizer (_grid, _language_label, true, wxGBPosition(_ccap_track_or_language_row, 0));
			_language_sizer = new wxBoxSizer (wxHORIZONTAL);
			_language = new LanguageTagWidget (this, _("Language of these subtitles"), boost::none, wxString("en-US-"));
			_language->Changed.connect (boost::bind(&TextPanel::language_changed, this));
			_language_sizer->Add (_language->sizer(), 1, wxRIGHT, DCPOMATIC_SIZER_GAP);
			_language_type = new wxChoice (this, wxID_ANY);
			/// TRANSLATORS: Main and Additional here are a choice for whether a set of subtitles is in the "main" language of the
			/// film or an "additional" language.
			_language_type->Append (_("Main"));
			_language_type->Append (_("Additional"));
			_language_type->Bind (wxEVT_CHOICE, boost::bind(&TextPanel::language_is_additional_changed, this));
			_language_sizer->Add (_language_type, 0, wxALIGN_CENTER_VERTICAL | wxTOP, DCPOMATIC_CHOICE_TOP_PAD);
			_grid->Add (_language_sizer, wxGBPosition(_ccap_track_or_language_row, 1), wxGBSpan(1, 2));
			film_content_changed (TextContentProperty::LANGUAGE);
			film_content_changed (TextContentProperty::LANGUAGE_IS_ADDITIONAL);
		}
		break;
	case TextType::CLOSED_CAPTION:
		if (_language_label) {
			_language_label->Destroy ();
			_language_label = nullptr;
			_grid->Remove (_language->sizer());
			delete _language;
			_grid->Remove (_language_sizer);
			_language_sizer = nullptr;
			_language = nullptr;
			_language_type->Destroy ();
			_language_type = nullptr;
		}
		if (!_dcp_track_label) {
			_dcp_track_label = create_label (this, _("CCAP track"), true);
			add_label_to_sizer (_grid, _dcp_track_label, true, wxGBPosition(_ccap_track_or_language_row, 0));
		}
		if (!_dcp_track) {
			_dcp_track = new wxChoice (this, wxID_ANY);
			_dcp_track->Bind (wxEVT_CHOICE, boost::bind(&TextPanel::dcp_track_changed, this));
			_grid->Add (_dcp_track, wxGBPosition(_ccap_track_or_language_row, 1), wxDefaultSpan, wxEXPAND);
			update_dcp_tracks ();
			film_content_changed (TextContentProperty::DCP_TRACK);
		}
		if (_outline_subtitles) {
			_outline_subtitles->Destroy ();
			_outline_subtitles = nullptr;
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

	auto use = new wxBoxSizer (wxHORIZONTAL);
	use->Add (_use, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
	use->Add (_type, 1, wxEXPAND, 0);
	_grid->Add (use, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_grid->Add (_burn, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_outline_subtitles_row = r;
	++r;

	add_label_to_sizer (_grid, _offset_label, true, wxGBPosition (r, 0));
	auto offset = new wxBoxSizer (wxHORIZONTAL);
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
	auto scale = new wxBoxSizer (wxHORIZONTAL);
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
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_line_spacing);
		add_label_to_sizer (s, _line_spacing_pc_label, false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL);
		_grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	_ccap_track_or_language_row = r;
	++r;

	add_label_to_sizer (_grid, _stream_label, true, wxGBPosition (r, 0));
	_grid->Add (_stream, wxGBPosition (r, 1));
	++r;

	{
		auto s = new wxBoxSizer (wxHORIZONTAL);

		s->Add (_text_view_button, 0, wxALL, DCPOMATIC_SIZER_GAP);
		s->Add (_fonts_dialog_button, 0, wxALL, DCPOMATIC_SIZER_GAP);
		s->Add (_appearance_dialog_button, 0, wxALL, DCPOMATIC_SIZER_GAP);

		_grid->Add (s, wxGBPosition(r, 0), wxGBSpan(1, 2));
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
		auto t = i->text_of_original_type(_original_type);
		if (t) {
			auto dt = t->dcp_track();
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
		if (!i.name.empty() || i.language) {
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
		auto d = make_wx<DCPTextTrackDialog>(this);
		if (d->ShowModal() == wxID_OK) {
			track = d->get();
		}
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
			auto t = i->text_of_original_type(_original_type);
			if (t && t->type() == TextType::CLOSED_CAPTION) {
				t->set_dcp_track(*track);
			}
		}
	}

	update_dcp_tracks ();
}


void
TextPanel::film_changed(FilmProperty property)
{
	if (property == FilmProperty::CONTENT || property == FilmProperty::REEL_TYPE || property == FilmProperty::INTEROP) {
		setup_sensitivity ();
	}
}


void
TextPanel::film_content_changed (int property)
{
	auto fc = _parent->selected_ffmpeg ();
	auto sc = _parent->selected_text ();

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
			for (auto i: fcs->subtitle_streams()) {
				_stream->Append (std_to_wx(i->name), new wxStringClientData(std_to_wx(i->identifier())));
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
	} else if (property == TextContentProperty::LANGUAGE) {
		if (_language) {
			_language->set (text ? text->language() : boost::none);
		}
	} else if (property == TextContentProperty::LANGUAGE_IS_ADDITIONAL) {
		if (_language_type) {
			_language_type->SetSelection (text ? (text->language_is_additional() ? 1 : 0) : 0);
		}
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
	auto sel = _parent->selected_text ();
	for (auto i: sel) {
		/* These are the content types that could include subtitles */
		auto fc = std::dynamic_pointer_cast<const FFmpegContent>(i);
		auto sc = std::dynamic_pointer_cast<const StringTextFileContent>(i);
		auto dc = std::dynamic_pointer_cast<const DCPContent>(i);
		auto dsc = std::dynamic_pointer_cast<const DCPSubtitleContent>(i);
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
		dcp = dynamic_pointer_cast<DCPContent>(sel.front());
	}

	auto const reference = dcp && dcp->reference_text(_original_type);

	auto const type = current_type ();

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
	auto const s = string_client_data (_stream->GetClientObject(_stream->GetSelection()));
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
	for (auto i: _parent->selected_text ()) {
		i->text_of_original_type(_original_type)->set_x_scale (_x_scale->GetValue() / 100.0);
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
	film_content_changed (TextContentProperty::LANGUAGE);
	film_content_changed (TextContentProperty::LANGUAGE_IS_ADDITIONAL);
	film_content_changed (DCPContentProperty::REFERENCE_TEXT);
}


void
TextPanel::text_view_clicked ()
{
	auto c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	auto decoder = decoder_factory (_parent->film(), c.front(), false, false, shared_ptr<Decoder>());

	if (decoder) {
		_text_view.reset(this, _parent->film(), c.front(), c.front()->text_of_original_type(_original_type), decoder, _parent->film_viewer());
		_text_view->Show ();
	}
}


void
TextPanel::fonts_dialog_clicked ()
{
	auto c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	_fonts_dialog.reset(this, c.front(), c.front()->text_of_original_type(_original_type));
	_fonts_dialog->Show ();
}


void
TextPanel::appearance_dialog_clicked ()
{
	auto c = _parent->selected_text ();
	DCPOMATIC_ASSERT (c.size() == 1);

	SubtitleAppearanceDialog dialog(this, _parent->film(), c.front(), c.front()->text_of_original_type(_original_type));
	if (dialog.ShowModal() == wxID_OK) {
		dialog.apply();
	}
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
	ScopeGuard sg = [this]() {
		_loading_analysis = false;
		setup_sensitivity();
	};

	setup_sensitivity ();
	_analysis.reset ();

	auto content = _analysis_content.lock ();
	if (!content) {
		return;
	}

	auto const path = _parent->film()->subtitle_analysis_path(content);

	if (!dcp::filesystem::exists(path)) {
		for (auto i: JobManager::instance()->get()) {
			if (dynamic_pointer_cast<AnalyseSubtitlesJob>(i) && !i->finished()) {
				i->cancel ();
			}
		}

		JobManager::instance()->analyse_subtitles (
			_parent->film(), content, _analysis_finished_connection, bind(&TextPanel::analysis_finished, this, _1)
			);
		return;
	}

	try {
		_analysis.reset (new SubtitleAnalysis(path));
	} catch (OldFormatError& e) {
		/* An old analysis file: recreate it */
		JobManager::instance()->analyse_subtitles (
			_parent->film(), content, _analysis_finished_connection, bind(&TextPanel::analysis_finished, this, _1)
			);
		return;
	}

	update_outline_subtitles_in_viewer ();
}


void
TextPanel::update_outline_subtitles_in_viewer ()
{
	auto& fv = _parent->film_viewer();

	if (_analysis) {
		auto rect = _analysis->bounding_box ();
		if (rect) {
			auto content = _analysis_content.lock ();
			DCPOMATIC_ASSERT (content);
			rect->x += content->text.front()->x_offset() - _analysis->analysis_x_offset();
			rect->y += content->text.front()->y_offset() - _analysis->analysis_y_offset();
		}
		fv.set_outline_subtitles(rect);
	} else {
		fv.set_outline_subtitles({});
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
TextPanel::analysis_finished(Job::Result result)
{
	_loading_analysis = false;

	auto content = _analysis_content.lock ();
	if (!content || result == Job::Result::RESULT_CANCELLED) {
		clear_outline_subtitles();
		setup_sensitivity();
		return;
	}

	if (!dcp::filesystem::exists(_parent->film()->subtitle_analysis_path(content))) {
		/* We analysed and still nothing showed up, so maybe it failed.  Give up. */
		error_dialog (_parent->window(), _("Could not analyse subtitles."));
		clear_outline_subtitles ();
		setup_sensitivity ();
		return;
	}

	try_to_load_analysis ();
}


void
TextPanel::language_changed ()
{
	for (auto i: _parent->selected_text()) {
		auto t = i->text_of_original_type(_original_type);
		if (t) {
			t->set_language (_language->get());
		}
	}
}


void
TextPanel::language_is_additional_changed ()
{
	for (auto i: _parent->selected_text()) {
		auto t = i->text_of_original_type(_original_type);
		if (t) {
			t->set_language_is_additional (_language_type->GetSelection() == 1);
		}
	}
}

