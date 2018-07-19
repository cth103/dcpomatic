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

#include "caption_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "subtitle_view.h"
#include "content_panel.h"
#include "fonts_dialog.h"
#include "caption_appearance_dialog.h"
#include "lib/ffmpeg_content.h"
#include "lib/text_caption_file_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/text_caption_file_decoder.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/dcp_content.h"
#include "lib/caption_content.h"
#include "lib/decoder_factory.h"
#include <wx/spinctrl.h>
#include <boost/foreach.hpp>

using std::vector;
using std::string;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

CaptionPanel::CaptionPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Captions"))
	, _subtitle_view (0)
	, _fonts_dialog (0)
{
	wxBoxSizer* reference_sizer = new wxBoxSizer (wxVERTICAL);

	_reference = new wxCheckBox (this, wxID_ANY, _("Use this DCP's subtitle as OV and make VF"));
	reference_sizer->Add (_reference, 0, wxLEFT | wxRIGHT | wxTOP, DCPOMATIC_SIZER_GAP);

	_reference_note = new wxStaticText (this, wxID_ANY, _(""));
	_reference_note->Wrap (200);
	reference_sizer->Add (_reference_note, 0, wxLEFT | wxRIGHT, DCPOMATIC_SIZER_GAP);
	wxFont font = _reference_note->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_reference_note->SetFont(font);

	_sizer->Add (reference_sizer);

	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);
	int r = 0;

	wxBoxSizer* use = new wxBoxSizer (wxHORIZONTAL);
	_use = new wxCheckBox (this, wxID_ANY, _("Use as"));
	use->Add (_use, 0, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
	_type = new wxChoice (this, wxID_ANY);
	_type->Append (_("subtitles (open captions)"));
	_type->Append (_("closed captions"));
	use->Add (_type, 1, wxEXPAND, 0);
	grid->Add (use, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_burn = new wxCheckBox (this, wxID_ANY, _("Burn subtitles into image"));
	grid->Add (_burn, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	{
		add_label_to_sizer (grid, this, _("X Offset"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_x_offset = new wxSpinCtrl (this);
		s->Add (_x_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	{
		add_label_to_sizer (grid, this, _("Y Offset"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_y_offset = new wxSpinCtrl (this);
		s->Add (_y_offset);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	{
		add_label_to_sizer (grid, this, _("X Scale"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_x_scale = new wxSpinCtrl (this);
		s->Add (_x_scale);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	{
		add_label_to_sizer (grid, this, _("Y Scale"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_y_scale = new wxSpinCtrl (this);
		s->Add (_y_scale);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	{
		add_label_to_sizer (grid, this, _("Line spacing"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_line_spacing = new wxSpinCtrl (this);
		s->Add (_line_spacing);
		add_label_to_sizer (s, this, _("%"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	add_label_to_sizer (grid, this, _("Language"), true, wxGBPosition (r, 0));
	_language = new wxTextCtrl (this, wxID_ANY);
	grid->Add (_language, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (grid, this, _("Stream"), true, wxGBPosition (r, 0));
	_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_stream, wxGBPosition (r, 1));
	++r;

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		_subtitle_view_button = new wxButton (this, wxID_ANY, _("View..."));
		s->Add (_subtitle_view_button, 1, wxALL, DCPOMATIC_SIZER_GAP);
		_fonts_dialog_button = new wxButton (this, wxID_ANY, _("Fonts..."));
		s->Add (_fonts_dialog_button, 1, wxALL, DCPOMATIC_SIZER_GAP);
		_appearance_dialog_button = new wxButton (this, wxID_ANY, _("Appearance..."));
		s->Add (_appearance_dialog_button, 1, wxALL, DCPOMATIC_SIZER_GAP);

		grid->Add (s, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;
	}

	_x_offset->SetRange (-100, 100);
	_y_offset->SetRange (-100, 100);
	_x_scale->SetRange (10, 1000);
	_y_scale->SetRange (10, 1000);
	_line_spacing->SetRange (10, 1000);

	_reference->Bind                (wxEVT_CHECKBOX, boost::bind (&CaptionPanel::reference_clicked, this));
	_use->Bind                      (wxEVT_CHECKBOX, boost::bind (&CaptionPanel::use_toggled, this));
	_type->Bind                     (wxEVT_CHOICE,   boost::bind (&CaptionPanel::type_changed, this));
	_burn->Bind                     (wxEVT_CHECKBOX, boost::bind (&CaptionPanel::burn_toggled, this));
	_x_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&CaptionPanel::x_offset_changed, this));
	_y_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&CaptionPanel::y_offset_changed, this));
	_x_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&CaptionPanel::x_scale_changed, this));
	_y_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&CaptionPanel::y_scale_changed, this));
	_line_spacing->Bind             (wxEVT_SPINCTRL, boost::bind (&CaptionPanel::line_spacing_changed, this));
	_language->Bind                 (wxEVT_TEXT,     boost::bind (&CaptionPanel::language_changed, this));
	_stream->Bind                   (wxEVT_CHOICE,   boost::bind (&CaptionPanel::stream_changed, this));
	_subtitle_view_button->Bind     (wxEVT_BUTTON,   boost::bind (&CaptionPanel::subtitle_view_clicked, this));
	_fonts_dialog_button->Bind      (wxEVT_BUTTON,   boost::bind (&CaptionPanel::fonts_dialog_clicked, this));
	_appearance_dialog_button->Bind (wxEVT_BUTTON,   boost::bind (&CaptionPanel::appearance_dialog_clicked, this));
}

void
CaptionPanel::film_changed (Film::Property property)
{
	if (property == Film::CONTENT || property == Film::REEL_TYPE) {
		setup_sensitivity ();
	}
}

void
CaptionPanel::film_content_changed (int property)
{
	FFmpegContentList fc = _parent->selected_ffmpeg ();
	ContentList sc = _parent->selected_subtitle ();

	shared_ptr<FFmpegContent> fcs;
	if (fc.size() == 1) {
		fcs = fc.front ();
	}

	shared_ptr<Content> scs;
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
	} else if (property == CaptionContentProperty::USE) {
		checked_set (_use, scs ? scs->caption->use() : false);
		setup_sensitivity ();
	} else if (property == CaptionContentProperty::TYPE) {
		if (scs) {
			switch (scs->caption->type()) {
			case CAPTION_OPEN:
				_type->SetSelection (0);
				break;
			case CAPTION_CLOSED:
				_type->SetSelection (1);
				break;
			default:
				DCPOMATIC_ASSERT (false);
			}
		} else {
			_type->SetSelection (0);
		}
		setup_sensitivity ();
	} else if (property == CaptionContentProperty::BURN) {
		checked_set (_burn, scs ? scs->caption->burn() : false);
	} else if (property == CaptionContentProperty::X_OFFSET) {
		checked_set (_x_offset, scs ? lrint (scs->caption->x_offset() * 100) : 0);
	} else if (property == CaptionContentProperty::Y_OFFSET) {
		checked_set (_y_offset, scs ? lrint (scs->caption->y_offset() * 100) : 0);
	} else if (property == CaptionContentProperty::X_SCALE) {
		checked_set (_x_scale, scs ? lrint (scs->caption->x_scale() * 100) : 100);
	} else if (property == CaptionContentProperty::Y_SCALE) {
		checked_set (_y_scale, scs ? lrint (scs->caption->y_scale() * 100) : 100);
	} else if (property == CaptionContentProperty::LINE_SPACING) {
		checked_set (_line_spacing, scs ? lrint (scs->caption->line_spacing() * 100) : 100);
	} else if (property == CaptionContentProperty::LANGUAGE) {
		checked_set (_language, scs ? scs->caption->language() : "");
	} else if (property == DCPContentProperty::REFERENCE_SUBTITLE) {
		if (scs) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (scs);
			checked_set (_reference, dcp ? dcp->reference_subtitle () : false);
		} else {
			checked_set (_reference, false);
		}

		setup_sensitivity ();
	} else if (property == DCPContentProperty::HAS_SUBTITLES) {
		setup_sensitivity ();
	}
}

void
CaptionPanel::use_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_use (_use->GetValue());
	}
}

void
CaptionPanel::type_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle()) {
		switch (_type->GetSelection()) {
		case 0:
			i->caption->set_type (CAPTION_OPEN);
			break;
		case 1:
			i->caption->set_type (CAPTION_CLOSED);
			break;
		}
	}
}

void
CaptionPanel::burn_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_burn (_burn->GetValue());
	}
}

void
CaptionPanel::setup_sensitivity ()
{
	int any_subs = 0;
	int ffmpeg_subs = 0;
	ContentList sel = _parent->selected_subtitle ();
	BOOST_FOREACH (shared_ptr<Content> i, sel) {
		/* These are the content types that could include subtitles */
		shared_ptr<const FFmpegContent> fc = boost::dynamic_pointer_cast<const FFmpegContent> (i);
		shared_ptr<const TextCaptionFileContent> sc = boost::dynamic_pointer_cast<const TextCaptionFileContent> (i);
		shared_ptr<const DCPContent> dc = boost::dynamic_pointer_cast<const DCPContent> (i);
		shared_ptr<const DCPSubtitleContent> dsc = boost::dynamic_pointer_cast<const DCPSubtitleContent> (i);
		if (fc) {
			if (fc->caption) {
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
	bool const can_reference = dcp && dcp->can_reference_subtitle (why_not);
	setup_refer_button (_reference, _reference_note, dcp, can_reference, why_not);

	bool const reference = _reference->GetValue ();

	/* Set up sensitivity */
	_use->Enable (!reference && any_subs > 0);
	bool const use = _use->GetValue ();
	_type->Enable (!reference && any_subs > 0 && use);
	_burn->Enable (!reference && any_subs > 0 && use && _type->GetSelection() == 0);
	_x_offset->Enable (!reference && any_subs > 0 && use);
	_y_offset->Enable (!reference && any_subs > 0 && use);
	_x_scale->Enable (!reference && any_subs > 0 && use);
	_y_scale->Enable (!reference && any_subs > 0 && use);
	_line_spacing->Enable (!reference && use);
	_language->Enable (!reference && any_subs > 0 && use);
	_stream->Enable (!reference && ffmpeg_subs == 1);
	_subtitle_view_button->Enable (!reference);
	_fonts_dialog_button->Enable (!reference);
	_appearance_dialog_button->Enable (!reference && any_subs > 0 && use);
}

void
CaptionPanel::stream_changed ()
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
CaptionPanel::x_offset_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
CaptionPanel::y_offset_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
CaptionPanel::x_scale_changed ()
{
	ContentList c = _parent->selected_subtitle ();
	if (c.size() == 1) {
		c.front()->caption->set_x_scale (_x_scale->GetValue() / 100.0);
	}
}

void
CaptionPanel::y_scale_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_y_scale (_y_scale->GetValue() / 100.0);
	}
}

void
CaptionPanel::line_spacing_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_line_spacing (_line_spacing->GetValue() / 100.0);
	}
}

void
CaptionPanel::language_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->caption->set_language (wx_to_std (_language->GetValue()));
	}
}

void
CaptionPanel::content_selection_changed ()
{
	film_content_changed (FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (CaptionContentProperty::USE);
	film_content_changed (CaptionContentProperty::BURN);
	film_content_changed (CaptionContentProperty::X_OFFSET);
	film_content_changed (CaptionContentProperty::Y_OFFSET);
	film_content_changed (CaptionContentProperty::X_SCALE);
	film_content_changed (CaptionContentProperty::Y_SCALE);
	film_content_changed (CaptionContentProperty::LINE_SPACING);
	film_content_changed (CaptionContentProperty::LANGUAGE);
	film_content_changed (CaptionContentProperty::FONTS);
	film_content_changed (CaptionContentProperty::TYPE);
	film_content_changed (DCPContentProperty::REFERENCE_SUBTITLE);
}

void
CaptionPanel::subtitle_view_clicked ()
{
	if (_subtitle_view) {
		_subtitle_view->Destroy ();
		_subtitle_view = 0;
	}

	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	shared_ptr<Decoder> decoder = decoder_factory (c.front(), _parent->film()->log(), false);

	if (decoder) {
		_subtitle_view = new SubtitleView (this, _parent->film(), c.front(), decoder, _parent->film_viewer());
		_subtitle_view->Show ();
	}
}

void
CaptionPanel::fonts_dialog_clicked ()
{
	if (_fonts_dialog) {
		_fonts_dialog->Destroy ();
		_fonts_dialog = 0;
	}

	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	_fonts_dialog = new FontsDialog (this, c.front ());
	_fonts_dialog->Show ();
}

void
CaptionPanel::reference_clicked ()
{
	ContentList c = _parent->selected ();
	if (c.size() != 1) {
		return;
	}

	shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (c.front ());
	if (!d) {
		return;
	}

	d->set_reference_subtitle (_reference->GetValue ());
}

void
CaptionPanel::appearance_dialog_clicked ()
{
	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	CaptionAppearanceDialog* d = new CaptionAppearanceDialog (this, c.front());
	if (d->ShowModal () == wxID_OK) {
		d->apply ();
	}
	d->Destroy ();
}
