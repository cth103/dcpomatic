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

#include "dcp_panel.h"
#include "wx_util.h"
#include "key_dialog.h"
#include "isdcf_metadata_dialog.h"
#include "audio_dialog.h"
#include "focus_manager.h"
#include "lib/ratio.h"
#include "lib/config.h"
#include "lib/dcp_content_type.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/audio_processor.h"
#include "lib/video_content.h"
#include "lib/subtitle_content.h"
#include "lib/dcp_content.h"
#include "lib/audio_content.h"
#include <dcp/locale_convert.h>
#include <dcp/key.h>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::list;
using std::string;
using std::vector;
using std::pair;
using std::max;
using std::make_pair;
using boost::lexical_cast;
using boost::shared_ptr;
using dcp::locale_convert;

DCPPanel::DCPPanel (wxNotebook* n, boost::shared_ptr<Film> film)
	: _audio_dialog (0)
	, _film (film)
	, _generally_sensitive (true)
{
	_panel = new wxPanel (n);
	_sizer = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (_sizer);

	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxEXPAND | wxALL, 8);

	int r = 0;

	add_label_to_sizer (grid, _panel, _("Name"), true, wxGBPosition (r, 0));
	_name = new wxTextCtrl (_panel, wxID_ANY);
	grid->Add (_name, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND | wxLEFT | wxRIGHT);
	++r;

	FocusManager::instance()->add(_name);

	int flags = wxALIGN_CENTER_VERTICAL;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
#endif

	_use_isdcf_name = new wxCheckBox (_panel, wxID_ANY, _("Use ISDCF name"));
	grid->Add (_use_isdcf_name, wxGBPosition (r, 0), wxDefaultSpan, flags);

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_edit_isdcf_button = new wxButton (_panel, wxID_ANY, _("Details..."));
		s->Add (_edit_isdcf_button, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		_copy_isdcf_name_button = new wxButton (_panel, wxID_ANY, _("Copy as name"));
		s->Add (_copy_isdcf_name_button, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_X_GAP);
		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxEXPAND);
		++r;
	}

	/* wxST_ELLIPSIZE_MIDDLE works around a bug in GTK2 and/or wxWidgets, see
	   http://trac.wxwidgets.org/ticket/12539
	*/
	_dcp_name = new wxStaticText (
		_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize,
		wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_MIDDLE
		);

	grid->Add (_dcp_name, wxGBPosition(r, 0), wxGBSpan (1, 2), wxALIGN_CENTER_VERTICAL | wxEXPAND);
	++r;

	add_label_to_sizer (grid, _panel, _("Content Type"), true, wxGBPosition (r, 0));
	_dcp_content_type = new wxChoice (_panel, wxID_ANY);
	grid->Add (_dcp_content_type, wxGBPosition (r, 1));
	++r;

	_notebook = new wxNotebook (_panel, wxID_ANY);
	_sizer->Add (_notebook, 1, wxEXPAND | wxTOP, 6);

	_notebook->AddPage (make_video_panel (), _("Video"), false);
	_notebook->AddPage (make_audio_panel (), _("Audio"), false);

	_signed = new wxCheckBox (_panel, wxID_ANY, _("Signed"));
	grid->Add (_signed, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_encrypted = new wxCheckBox (_panel, wxID_ANY, _("Encrypted"));
	grid->Add (_encrypted, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

        wxClientDC dc (_panel);
        wxSize size = dc.GetTextExtent (wxT ("GGGGGGGG..."));
        size.SetHeight (-1);

	{
               add_label_to_sizer (grid, _panel, _("Key"), true, wxGBPosition (r, 0));
               wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
               _key = new wxStaticText (_panel, wxID_ANY, "", wxDefaultPosition, size);
               s->Add (_key, 1, wxALIGN_CENTER_VERTICAL);
               _edit_key = new wxButton (_panel, wxID_ANY, _("Edit..."));
               s->Add (_edit_key);
               grid->Add (s, wxGBPosition (r, 1));
               ++r;
	}

	add_label_to_sizer (grid, _panel, _("Reels"), true, wxGBPosition (r, 0));
	_reel_type = new wxChoice (_panel, wxID_ANY);
	grid->Add (_reel_type, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (grid, _panel, _("Reel length"), true, wxGBPosition (r, 0));

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_reel_length = new wxSpinCtrl (_panel, wxID_ANY);
		s->Add (_reel_length);
		add_label_to_sizer (s, _panel, _("GB"), false);
		grid->Add (s, wxGBPosition (r, 1));
		++r;
	}

	add_label_to_sizer (grid, _panel, _("Standard"), true, wxGBPosition (r, 0));
	_standard = new wxChoice (_panel, wxID_ANY);
	grid->Add (_standard, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_upload_after_make_dcp = new wxCheckBox (_panel, wxID_ANY, _("Upload DCP to TMS after it is made"));
	grid->Add (_upload_after_make_dcp, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_name->Bind		     (wxEVT_TEXT,     boost::bind (&DCPPanel::name_changed, this));
	_use_isdcf_name->Bind	     (wxEVT_CHECKBOX, boost::bind (&DCPPanel::use_isdcf_name_toggled, this));
	_edit_isdcf_button->Bind     (wxEVT_BUTTON,   boost::bind (&DCPPanel::edit_isdcf_button_clicked, this));
	_copy_isdcf_name_button->Bind(wxEVT_BUTTON,   boost::bind (&DCPPanel::copy_isdcf_name_button_clicked, this));
	_dcp_content_type->Bind	     (wxEVT_CHOICE,  boost::bind (&DCPPanel::dcp_content_type_changed, this));
	_signed->Bind                (wxEVT_CHECKBOX, boost::bind (&DCPPanel::signed_toggled, this));
	_encrypted->Bind             (wxEVT_CHECKBOX, boost::bind (&DCPPanel::encrypted_toggled, this));
	_edit_key->Bind              (wxEVT_BUTTON,   boost::bind (&DCPPanel::edit_key_clicked, this));
	_reel_type->Bind             (wxEVT_CHOICE,  boost::bind (&DCPPanel::reel_type_changed, this));
	_reel_length->Bind           (wxEVT_SPINCTRL, boost::bind (&DCPPanel::reel_length_changed, this));
	_standard->Bind              (wxEVT_CHOICE,  boost::bind (&DCPPanel::standard_changed, this));
	_upload_after_make_dcp->Bind (wxEVT_CHECKBOX, boost::bind (&DCPPanel::upload_after_make_dcp_changed, this));

	BOOST_FOREACH (DCPContentType const * i, DCPContentType::all()) {
		_dcp_content_type->Append (std_to_wx (i->pretty_name ()));
	}

	_reel_type->Append (_("Single reel"));
	_reel_type->Append (_("Split by video content"));
	/// TRANSLATORS: translate the word "Custom" here; do not include the "Reel|" prefix
	_reel_type->Append (S_("Reel|Custom"));

	_reel_length->SetRange (1, 64);

	_standard->Append (_("SMPTE"));
	_standard->Append (_("Interop"));

	Config::instance()->Changed.connect (boost::bind (&DCPPanel::config_changed, this));
}

void
DCPPanel::edit_key_clicked ()
{
	KeyDialog* d = new KeyDialog (_panel, _film->key ());
	if (d->ShowModal () == wxID_OK) {
		_film->set_key (d->key ());
	}
	d->Destroy ();
}

void
DCPPanel::name_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_name (string (_name->GetValue().mb_str()));
}

void
DCPPanel::j2k_bandwidth_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1000000);
}

void
DCPPanel::signed_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_signed (_signed->GetValue ());
}

void
DCPPanel::encrypted_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_encrypted (_encrypted->GetValue ());
}

/** Called when the frame rate choice widget has been changed */
void
DCPPanel::frame_rate_choice_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (
		boost::lexical_cast<int> (
			wx_to_std (_frame_rate_choice->GetString (_frame_rate_choice->GetSelection ()))
			)
		);
}

/** Called when the frame rate spin widget has been changed */
void
DCPPanel::frame_rate_spin_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (_frame_rate_spin->GetValue ());
}

void
DCPPanel::audio_channels_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_audio_channels (locale_convert<int> (string_client_data (_audio_channels->GetClientObject (_audio_channels->GetSelection ()))));
}

void
DCPPanel::resolution_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_resolution (_resolution->GetSelection() == 0 ? RESOLUTION_2K : RESOLUTION_4K);
}

void
DCPPanel::standard_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_interop (_standard->GetSelection() == 1);
}

void
DCPPanel::upload_after_make_dcp_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_upload_after_make_dcp (_upload_after_make_dcp->GetValue ());
}

void
DCPPanel::film_changed (int p)
{
	switch (p) {
	case Film::NONE:
		break;
	case Film::CONTAINER:
		setup_container ();
		break;
	case Film::NAME:
		checked_set (_name, _film->name());
		setup_dcp_name ();
		break;
	case Film::DCP_CONTENT_TYPE:
		checked_set (_dcp_content_type, DCPContentType::as_index (_film->dcp_content_type ()));
		setup_dcp_name ();
		break;
	case Film::SIGNED:
		checked_set (_signed, _film->is_signed ());
		break;
	case Film::ENCRYPTED:
		checked_set (_encrypted, _film->encrypted ());
		if (_film->encrypted ()) {
			_film->set_signed (true);
			_signed->Enable (false);
			_key->Enable (_generally_sensitive);
			_edit_key->Enable (_generally_sensitive);
		} else {
			_signed->Enable (_generally_sensitive);
			_key->Enable (false);
			_edit_key->Enable (false);
		}
		break;
	case Film::KEY:
		checked_set (_key, _film->key().hex().substr (0, 8) + "...");
		break;
	case Film::RESOLUTION:
		checked_set (_resolution, _film->resolution() == RESOLUTION_2K ? 0 : 1);
		setup_container ();
		setup_dcp_name ();
		break;
	case Film::J2K_BANDWIDTH:
		checked_set (_j2k_bandwidth, _film->j2k_bandwidth() / 1000000);
		break;
	case Film::USE_ISDCF_NAME:
	{
		checked_set (_use_isdcf_name, _film->use_isdcf_name ());
		setup_dcp_name ();
		_edit_isdcf_button->Enable (_film->use_isdcf_name ());
		break;
	}
	case Film::ISDCF_METADATA:
		setup_dcp_name ();
		break;
	case Film::VIDEO_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _frame_rate_choice->GetCount(); ++i) {
			if (wx_to_std (_frame_rate_choice->GetString(i)) == boost::lexical_cast<string> (_film->video_frame_rate())) {
				checked_set (_frame_rate_choice, i);
				done = true;
				break;
			}
		}

		if (!done) {
			checked_set (_frame_rate_choice, -1);
		}

		checked_set (_frame_rate_spin, _film->video_frame_rate ());

		_best_frame_rate->Enable (_film->best_video_frame_rate () != _film->video_frame_rate ());
		setup_dcp_name ();
		break;
	}
	case Film::AUDIO_CHANNELS:
		if (_film->audio_channels () < minimum_allowed_audio_channels ()) {
			_film->set_audio_channels (minimum_allowed_audio_channels ());
		} else {
			checked_set (_audio_channels, locale_convert<string> (max (minimum_allowed_audio_channels(), _film->audio_channels ())));
			setup_dcp_name ();
		}
		break;
	case Film::THREE_D:
		checked_set (_three_d, _film->three_d ());
		setup_dcp_name ();
		break;
	case Film::INTEROP:
		checked_set (_standard, _film->interop() ? 1 : 0);
		setup_dcp_name ();
		break;
	case Film::AUDIO_PROCESSOR:
		if (_film->audio_processor ()) {
			checked_set (_audio_processor, _film->audio_processor()->id());
		} else {
			checked_set (_audio_processor, 0);
		}
		setup_audio_channels_choice (_audio_channels, minimum_allowed_audio_channels ());
		film_changed (Film::AUDIO_CHANNELS);
		break;
	case Film::REEL_TYPE:
		checked_set (_reel_type, _film->reel_type ());
		_reel_length->Enable (_film->reel_type() == REELTYPE_BY_LENGTH);
		break;
	case Film::REEL_LENGTH:
		checked_set (_reel_length, _film->reel_length() / 1000000000LL);
		break;
	case Film::UPLOAD_AFTER_MAKE_DCP:
		checked_set (_upload_after_make_dcp, _film->upload_after_make_dcp ());
		break;
	case Film::CONTENT:
		setup_dcp_name ();
		break;
	default:
		break;
	}
}

void
DCPPanel::film_content_changed (int property)
{
	if (property == AudioContentProperty::STREAMS ||
	    property == SubtitleContentProperty::USE ||
	    property == SubtitleContentProperty::BURN ||
	    property == VideoContentProperty::SCALE ||
	    property == DCPContentProperty::REFERENCE_VIDEO ||
	    property == DCPContentProperty::REFERENCE_AUDIO ||
	    property == DCPContentProperty::REFERENCE_SUBTITLE) {
		setup_dcp_name ();
		setup_sensitivity ();
	}
}


void
DCPPanel::setup_container ()
{
	int n = 0;
	vector<Ratio const *> ratios = Ratio::containers ();
	vector<Ratio const *>::iterator i = ratios.begin ();
	while (i != ratios.end() && *i != _film->container ()) {
		++i;
		++n;
	}

	if (i == ratios.end()) {
		checked_set (_container, -1);
		checked_set (_container_size, wxT (""));
	} else {
		checked_set (_container, n);
		dcp::Size const size = fit_ratio_within (_film->container()->ratio(), _film->full_frame ());
		checked_set (_container_size, wxString::Format ("%dx%d", size.width, size.height));
	}

	setup_dcp_name ();
}

/** Called when the container widget has been changed */
void
DCPPanel::container_changed ()
{
	if (!_film) {
		return;
	}

	int const n = _container->GetSelection ();
	if (n >= 0) {
		vector<Ratio const *> ratios = Ratio::containers ();
		DCPOMATIC_ASSERT (n < int (ratios.size()));
		_film->set_container (ratios[n]);
	}
}

/** Called when the DCP content type widget has been changed */
void
DCPPanel::dcp_content_type_changed ()
{
	if (!_film) {
		return;
	}

	int const n = _dcp_content_type->GetSelection ();
	if (n != wxNOT_FOUND) {
		_film->set_dcp_content_type (DCPContentType::from_index (n));
	}
}

void
DCPPanel::set_film (shared_ptr<Film> film)
{
	/* We are changing film, so destroy any audio dialog for the old one */
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	_film = film;

	film_changed (Film::NAME);
	film_changed (Film::USE_ISDCF_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::RESOLUTION);
	film_changed (Film::SIGNED);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::KEY);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::ISDCF_METADATA);
	film_changed (Film::VIDEO_FRAME_RATE);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::SEQUENCE);
	film_changed (Film::THREE_D);
	film_changed (Film::INTEROP);
	film_changed (Film::AUDIO_PROCESSOR);
	film_changed (Film::REEL_TYPE);
	film_changed (Film::REEL_LENGTH);
	film_changed (Film::UPLOAD_AFTER_MAKE_DCP);

	set_general_sensitivity(static_cast<bool>(_film));
}

void
DCPPanel::set_general_sensitivity (bool s)
{
	_generally_sensitive = s;
	setup_sensitivity ();
}

void
DCPPanel::setup_sensitivity ()
{
	_name->Enable                   (_generally_sensitive);
	_use_isdcf_name->Enable         (_generally_sensitive);
	_edit_isdcf_button->Enable      (_generally_sensitive);
	_dcp_content_type->Enable       (_generally_sensitive);
	_copy_isdcf_name_button->Enable (_generally_sensitive);

	bool si = _generally_sensitive;
	if (_film && _film->encrypted ()) {
		si = false;
	}
	_signed->Enable (si);

	_encrypted->Enable              (_generally_sensitive);
	_key->Enable                    (_generally_sensitive && _film && _film->encrypted ());
	_edit_key->Enable               (_generally_sensitive && _film && _film->encrypted ());
	_reel_type->Enable              (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->references_dcp_audio());
	_reel_length->Enable            (_generally_sensitive && _film && _film->reel_type() == REELTYPE_BY_LENGTH);
	_upload_after_make_dcp->Enable  (_generally_sensitive);
	_frame_rate_choice->Enable      (_generally_sensitive && _film && !_film->references_dcp_video());
	_frame_rate_spin->Enable        (_generally_sensitive && _film && !_film->references_dcp_video());
	_audio_channels->Enable         (_generally_sensitive && _film && !_film->references_dcp_audio());
	_audio_processor->Enable        (_generally_sensitive && _film && !_film->references_dcp_audio());
	_j2k_bandwidth->Enable          (_generally_sensitive && _film && !_film->references_dcp_video());
	_container->Enable              (_generally_sensitive && _film && !_film->references_dcp_video());
	_best_frame_rate->Enable        (_generally_sensitive && _film && _film->best_video_frame_rate () != _film->video_frame_rate ());
	_resolution->Enable             (_generally_sensitive && _film && !_film->references_dcp_video());
	_three_d->Enable                (_generally_sensitive && _film && !_film->references_dcp_video());
	_standard->Enable               (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->references_dcp_audio());
}

void
DCPPanel::use_isdcf_name_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_use_isdcf_name (_use_isdcf_name->GetValue ());
}

void
DCPPanel::edit_isdcf_button_clicked ()
{
	if (!_film) {
		return;
	}

	ISDCFMetadataDialog* d = new ISDCFMetadataDialog (_panel, _film->isdcf_metadata (), _film->three_d ());
	d->ShowModal ();
	_film->set_isdcf_metadata (d->isdcf_metadata ());
	d->Destroy ();
}

void
DCPPanel::setup_dcp_name ()
{
	_dcp_name->SetLabel (std_to_wx (_film->dcp_name (true)));
	_dcp_name->SetToolTip (std_to_wx (_film->dcp_name (true)));
}

void
DCPPanel::best_frame_rate_clicked ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (_film->best_video_frame_rate ());
}

void
DCPPanel::three_d_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_three_d (_three_d->GetValue ());
}

void
DCPPanel::config_changed ()
{
	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	setup_frame_rate_widget ();
}

void
DCPPanel::setup_frame_rate_widget ()
{
	if (Config::instance()->allow_any_dcp_frame_rate ()) {
		_frame_rate_choice->Hide ();
		_frame_rate_spin->Show ();
	} else {
		_frame_rate_choice->Show ();
		_frame_rate_spin->Hide ();
	}

	_frame_rate_sizer->Layout ();
}

wxPanel *
DCPPanel::make_video_panel ()
{
	wxPanel* panel = new wxPanel (_notebook);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->Add (grid, 0, wxALL, 8);
	panel->SetSizer (sizer);

	int r = 0;

	add_label_to_sizer (grid, panel, _("Container"), true, wxGBPosition (r, 0));
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_container = new wxChoice (panel, wxID_ANY);
		s->Add (_container, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		_container_size = new wxStaticText (panel, wxID_ANY, wxT (""));
		s->Add (_container_size, 1, wxLEFT | wxALIGN_CENTER_VERTICAL);
		grid->Add (s, wxGBPosition (r,1 ), wxDefaultSpan, wxEXPAND);
		++r;
	}

	add_label_to_sizer (grid, panel, _("Resolution"), true, wxGBPosition (r, 0));
	_resolution = new wxChoice (panel, wxID_ANY);
	grid->Add (_resolution, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (grid, panel, _("Frame Rate"), true, wxGBPosition (r, 0));
	{
		_frame_rate_sizer = new wxBoxSizer (wxHORIZONTAL);
		_frame_rate_choice = new wxChoice (panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_choice, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_spin = new wxSpinCtrl (panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_spin, 1, wxALIGN_CENTER_VERTICAL);
		setup_frame_rate_widget ();
		_best_frame_rate = new wxButton (panel, wxID_ANY, _("Use best"));
		_frame_rate_sizer->Add (_best_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		grid->Add (_frame_rate_sizer, wxGBPosition (r, 1));
		++r;
	}

	_three_d = new wxCheckBox (panel, wxID_ANY, _("3D"));
	grid->Add (_three_d, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	{
		add_label_to_sizer (grid, panel, _("JPEG2000 bandwidth\nfor newly-encoded data"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, panel, _("Mbit/s"), false);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	_container->Bind	(wxEVT_CHOICE,	      boost::bind (&DCPPanel::container_changed, this));
	_frame_rate_choice->Bind(wxEVT_CHOICE,	      boost::bind (&DCPPanel::frame_rate_choice_changed, this));
	_frame_rate_spin->Bind  (wxEVT_SPINCTRL,      boost::bind (&DCPPanel::frame_rate_spin_changed, this));
	_best_frame_rate->Bind	(wxEVT_BUTTON,	      boost::bind (&DCPPanel::best_frame_rate_clicked, this));
	_j2k_bandwidth->Bind	(wxEVT_SPINCTRL,      boost::bind (&DCPPanel::j2k_bandwidth_changed, this));
	/* Also listen to wxEVT_TEXT so that typing numbers directly in is always noticed */
	_j2k_bandwidth->Bind	(wxEVT_TEXT,          boost::bind (&DCPPanel::j2k_bandwidth_changed, this));
	_resolution->Bind       (wxEVT_CHOICE,        boost::bind (&DCPPanel::resolution_changed, this));
	_three_d->Bind	 	(wxEVT_CHECKBOX,      boost::bind (&DCPPanel::three_d_changed, this));

	BOOST_FOREACH (Ratio const * i, Ratio::containers()) {
		_container->Append (std_to_wx(i->container_nickname()));
	}

	BOOST_FOREACH (int i, Config::instance()->allowed_dcp_frame_rates()) {
		_frame_rate_choice->Append (std_to_wx (boost::lexical_cast<string> (i)));
	}

	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	_frame_rate_spin->SetRange (1, 480);

	_resolution->Append (_("2K"));
	_resolution->Append (_("4K"));

	return panel;
}

int
DCPPanel::minimum_allowed_audio_channels () const
{
	int min = 2;
	if (_film && _film->audio_processor ()) {
		min = _film->audio_processor()->out_channels ();
	}

	if (min % 2 == 1) {
		++min;
	}

	return min;
}

wxPanel *
DCPPanel::make_audio_panel ()
{
	wxPanel* panel = new wxPanel (_notebook);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->Add (grid, 0, wxALL, 8);
	panel->SetSizer (sizer);

	int r = 0;

	add_label_to_sizer (grid, panel, _("Channels"), true, wxGBPosition (r, 0));
	_audio_channels = new wxChoice (panel, wxID_ANY);
	setup_audio_channels_choice (_audio_channels, minimum_allowed_audio_channels ());
	grid->Add (_audio_channels, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (grid, panel, _("Processor"), true, wxGBPosition (r, 0));
	_audio_processor = new wxChoice (panel, wxID_ANY);
	_audio_processor->Append (_("None"), new wxStringClientData (N_("none")));
	BOOST_FOREACH (AudioProcessor const * ap, AudioProcessor::all ()) {
		_audio_processor->Append (std_to_wx (ap->name ()), new wxStringClientData (std_to_wx (ap->id ())));
	}
	grid->Add (_audio_processor, wxGBPosition (r, 1));
	++r;

	_show_audio = new wxButton (panel, wxID_ANY, _("Show audio..."));
	grid->Add (_show_audio, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_audio_channels->Bind (wxEVT_CHOICE, boost::bind (&DCPPanel::audio_channels_changed, this));
	_audio_processor->Bind (wxEVT_CHOICE, boost::bind (&DCPPanel::audio_processor_changed, this));
	_show_audio->Bind (wxEVT_BUTTON, boost::bind (&DCPPanel::show_audio_clicked, this));

	return panel;
}

void
DCPPanel::copy_isdcf_name_button_clicked ()
{
	_film->set_name (_film->isdcf_name (true));
	_film->set_use_isdcf_name (false);
}

void
DCPPanel::audio_processor_changed ()
{
	if (!_film) {
		return;
	}

	string const s = string_client_data (_audio_processor->GetClientObject (_audio_processor->GetSelection ()));
	_film->set_audio_processor (AudioProcessor::from_id (s));
}

void
DCPPanel::show_audio_clicked ()
{
	if (!_film) {
		return;
	}

	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	AudioDialog* d = new AudioDialog (_panel, _film);
	d->Show ();
}

void
DCPPanel::reel_type_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_reel_type (static_cast<ReelType> (_reel_type->GetSelection ()));
}

void
DCPPanel::reel_length_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_reel_length (_reel_length->GetValue() * 1000000000LL);
}
