/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_panel.h"
#include "film_editor.h"
#include "wx_util.h"
#include "subtitle_view.h"
#include "content_panel.h"
#include "fonts_dialog.h"
#include "subtitle_appearance_dialog.h"
#include "lib/ffmpeg_content.h"
#include "lib/text_subtitle_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/text_subtitle_decoder.h"
#include "lib/dcp_subtitle_decoder.h"
#include "lib/dcp_content.h"
#include "lib/subtitle_content.h"
#include "lib/decoder_factory.h"
#include <wx/spinctrl.h>
#include <boost/foreach.hpp>

using std::vector;
using std::string;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

SubtitlePanel::SubtitlePanel (ContentPanel* p)
	: ContentSubPanel (p, _("Subtitles"))
	, _subtitle_view (0)
	, _fonts_dialog (0)
{
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);
	int r = 0;

	_reference = new wxCheckBox (this, wxID_ANY, _("Refer to existing DCP"));
	grid->Add (_reference, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_use = new wxCheckBox (this, wxID_ANY, _("Use subtitles"));
	grid->Add (_use, wxGBPosition (r, 0), wxGBSpan (1, 2));
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

	_reference->Bind                (wxEVT_CHECKBOX, boost::bind (&SubtitlePanel::reference_clicked, this));
	_use->Bind                      (wxEVT_CHECKBOX, boost::bind (&SubtitlePanel::use_toggled, this));
	_burn->Bind                     (wxEVT_CHECKBOX, boost::bind (&SubtitlePanel::burn_toggled, this));
	_x_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&SubtitlePanel::x_offset_changed, this));
	_y_offset->Bind                 (wxEVT_SPINCTRL, boost::bind (&SubtitlePanel::y_offset_changed, this));
	_x_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&SubtitlePanel::x_scale_changed, this));
	_y_scale->Bind                  (wxEVT_SPINCTRL, boost::bind (&SubtitlePanel::y_scale_changed, this));
	_line_spacing->Bind             (wxEVT_SPINCTRL, boost::bind (&SubtitlePanel::line_spacing_changed, this));
	_language->Bind                 (wxEVT_TEXT,     boost::bind (&SubtitlePanel::language_changed, this));
	_stream->Bind                   (wxEVT_CHOICE,   boost::bind (&SubtitlePanel::stream_changed, this));
	_subtitle_view_button->Bind     (wxEVT_BUTTON,   boost::bind (&SubtitlePanel::subtitle_view_clicked, this));
	_fonts_dialog_button->Bind      (wxEVT_BUTTON,   boost::bind (&SubtitlePanel::fonts_dialog_clicked, this));
	_appearance_dialog_button->Bind (wxEVT_BUTTON,   boost::bind (&SubtitlePanel::appearance_dialog_clicked, this));
}

void
SubtitlePanel::film_changed (Film::Property property)
{
	if (property == Film::CONTENT || property == Film::REEL_TYPE) {
		setup_sensitivity ();
	}
}

void
SubtitlePanel::film_content_changed (int property)
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
	} else if (property == SubtitleContentProperty::USE) {
		checked_set (_use, scs ? scs->subtitle->use() : false);
		setup_sensitivity ();
	} else if (property == SubtitleContentProperty::BURN) {
		checked_set (_burn, scs ? scs->subtitle->burn() : false);
	} else if (property == SubtitleContentProperty::X_OFFSET) {
		checked_set (_x_offset, scs ? lrint (scs->subtitle->x_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::Y_OFFSET) {
		checked_set (_y_offset, scs ? lrint (scs->subtitle->y_offset() * 100) : 0);
	} else if (property == SubtitleContentProperty::X_SCALE) {
		checked_set (_x_scale, scs ? lrint (scs->subtitle->x_scale() * 100) : 100);
	} else if (property == SubtitleContentProperty::Y_SCALE) {
		checked_set (_y_scale, scs ? lrint (scs->subtitle->y_scale() * 100) : 100);
	} else if (property == SubtitleContentProperty::LINE_SPACING) {
		checked_set (_line_spacing, scs ? lrint (scs->subtitle->line_spacing() * 100) : 100);
	} else if (property == SubtitleContentProperty::LANGUAGE) {
		checked_set (_language, scs ? scs->subtitle->language() : "");
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
SubtitlePanel::use_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_use (_use->GetValue());
	}
}

void
SubtitlePanel::burn_toggled ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_burn (_burn->GetValue());
	}
}

void
SubtitlePanel::setup_sensitivity ()
{
	int any_subs = 0;
	int ffmpeg_subs = 0;
	ContentList sel = _parent->selected_subtitle ();
	BOOST_FOREACH (shared_ptr<Content> i, sel) {
		/* These are the content types that could include subtitles */
		shared_ptr<const FFmpegContent> fc = boost::dynamic_pointer_cast<const FFmpegContent> (i);
		shared_ptr<const TextSubtitleContent> sc = boost::dynamic_pointer_cast<const TextSubtitleContent> (i);
		shared_ptr<const DCPContent> dc = boost::dynamic_pointer_cast<const DCPContent> (i);
		shared_ptr<const DCPSubtitleContent> dsc = boost::dynamic_pointer_cast<const DCPSubtitleContent> (i);
		if (fc) {
			if (fc->subtitle) {
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

	list<string> why_not;
	bool const can_reference = dcp && dcp->can_reference_subtitle (why_not);
	setup_refer_button (_reference, dcp, can_reference, why_not);

	bool const reference = _reference->GetValue ();

	/* Set up sensitivity */
	_use->Enable (!reference && any_subs > 0);
	bool const use = _use->GetValue ();
	_burn->Enable (!reference && any_subs > 0 && use);
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
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_x_offset (_x_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::y_offset_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_y_offset (_y_offset->GetValue() / 100.0);
	}
}

void
SubtitlePanel::x_scale_changed ()
{
	ContentList c = _parent->selected_subtitle ();
	if (c.size() == 1) {
		c.front()->subtitle->set_x_scale (_x_scale->GetValue() / 100.0);
	}
}

void
SubtitlePanel::y_scale_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_y_scale (_y_scale->GetValue() / 100.0);
	}
}

void
SubtitlePanel::line_spacing_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_line_spacing (_line_spacing->GetValue() / 100.0);
	}
}

void
SubtitlePanel::language_changed ()
{
	BOOST_FOREACH (shared_ptr<Content> i, _parent->selected_subtitle ()) {
		i->subtitle->set_language (wx_to_std (_language->GetValue()));
	}
}

void
SubtitlePanel::content_selection_changed ()
{
	film_content_changed (FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (SubtitleContentProperty::USE);
	film_content_changed (SubtitleContentProperty::BURN);
	film_content_changed (SubtitleContentProperty::X_OFFSET);
	film_content_changed (SubtitleContentProperty::Y_OFFSET);
	film_content_changed (SubtitleContentProperty::X_SCALE);
	film_content_changed (SubtitleContentProperty::Y_SCALE);
	film_content_changed (SubtitleContentProperty::LINE_SPACING);
	film_content_changed (SubtitleContentProperty::LANGUAGE);
	film_content_changed (SubtitleContentProperty::FONTS);
	film_content_changed (DCPContentProperty::REFERENCE_SUBTITLE);
}

void
SubtitlePanel::subtitle_view_clicked ()
{
	if (_subtitle_view) {
		_subtitle_view->Destroy ();
		_subtitle_view = 0;
	}

	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	shared_ptr<Decoder> decoder = decoder_factory (c.front(), _parent->film()->log(), false);

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

	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	_fonts_dialog = new FontsDialog (this, c.front ());
	_fonts_dialog->Show ();
}

void
SubtitlePanel::reference_clicked ()
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
SubtitlePanel::appearance_dialog_clicked ()
{
	ContentList c = _parent->selected_subtitle ();
	DCPOMATIC_ASSERT (c.size() == 1);

	SubtitleAppearanceDialog* d = new SubtitleAppearanceDialog (this, c.front());
	if (d->ShowModal () == wxID_OK) {
		d->apply ();
	}
	d->Destroy ();
}
