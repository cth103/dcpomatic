/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "dcp_panel.h"
#include "wx_util.h"
#include "key_dialog.h"
#include "isdcf_metadata_dialog.h"
#include "lib/ratio.h"
#include "lib/config.h"
#include "lib/dcp_content_type.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include <dcp/key.h>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>
#include <boost/lexical_cast.hpp>

using std::cout;
using std::list;
using std::string;
using std::vector;
using boost::lexical_cast;
using boost::shared_ptr;

DCPPanel::DCPPanel (wxNotebook* n, boost::shared_ptr<Film> f)
	: _film (f)
	, _generally_sensitive (true)
{
	_panel = new wxPanel (n);
	_sizer = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (_sizer);

	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxEXPAND | wxALL, 8);

	int r = 0;
	
	add_label_to_grid_bag_sizer (grid, _panel, _("Name"), true, wxGBPosition (r, 0));
	_name = new wxTextCtrl (_panel, wxID_ANY);
	grid->Add (_name, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND | wxLEFT | wxRIGHT);
	++r;
	
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

	add_label_to_grid_bag_sizer (grid, _panel, _("DCP Name"), true, wxGBPosition (r, 0));
	_dcp_name = new wxStaticText (_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
	grid->Add (_dcp_name, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxEXPAND);
	++r;

	add_label_to_grid_bag_sizer (grid, _panel, _("Content Type"), true, wxGBPosition (r, 0));
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
               add_label_to_grid_bag_sizer (grid, _panel, _("Key"), true, wxGBPosition (r, 0));
               wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
               _key = new wxStaticText (_panel, wxID_ANY, "", wxDefaultPosition, size);
               s->Add (_key, 1, wxALIGN_CENTER_VERTICAL);
               _edit_key = new wxButton (_panel, wxID_ANY, _("Edit..."));
               s->Add (_edit_key);
               grid->Add (s, wxGBPosition (r, 1));
               ++r;
	}
	
	add_label_to_grid_bag_sizer (grid, _panel, _("Standard"), true, wxGBPosition (r, 0));
	_standard = new wxChoice (_panel, wxID_ANY);
	grid->Add (_standard, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_name->Bind		 (wxEVT_COMMAND_TEXT_UPDATED, 	      boost::bind (&DCPPanel::name_changed, this));
	_use_isdcf_name->Bind	 (wxEVT_COMMAND_CHECKBOX_CLICKED,     boost::bind (&DCPPanel::use_isdcf_name_toggled, this));
	_edit_isdcf_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&DCPPanel::edit_isdcf_button_clicked, this));
	_copy_isdcf_name_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&DCPPanel::copy_isdcf_name_button_clicked, this));
	_dcp_content_type->Bind	 (wxEVT_COMMAND_CHOICE_SELECTED,      boost::bind (&DCPPanel::dcp_content_type_changed, this));
	_signed->Bind            (wxEVT_COMMAND_CHECKBOX_CLICKED,     boost::bind (&DCPPanel::signed_toggled, this));
	_encrypted->Bind         (wxEVT_COMMAND_CHECKBOX_CLICKED,     boost::bind (&DCPPanel::encrypted_toggled, this));
	_edit_key->Bind          (wxEVT_COMMAND_BUTTON_CLICKED,       boost::bind (&DCPPanel::edit_key_clicked, this));
	_standard->Bind          (wxEVT_COMMAND_CHOICE_SELECTED,      boost::bind (&DCPPanel::standard_changed, this));

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
	}

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
DCPPanel::burn_subtitles_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_burn_subtitles (_burn_subtitles->GetValue ());
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

	_film->set_audio_channels ((_audio_channels->GetSelection () + 1) * 2);
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
	case Film::BURN_SUBTITLES:
		checked_set (_burn_subtitles, _film->burn_subtitles ());
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

		_frame_rate_spin->SetValue (_film->video_frame_rate ());

		_best_frame_rate->Enable (_film->best_video_frame_rate () != _film->video_frame_rate ());
		break;
	}
	case Film::AUDIO_CHANNELS:
		checked_set (_audio_channels, (_film->audio_channels () / 2) - 1);
		setup_dcp_name ();
		break;
	case Film::THREE_D:
		checked_set (_three_d, _film->three_d ());
		setup_dcp_name ();
		break;
	case Film::INTEROP:
		checked_set (_standard, _film->interop() ? 1 : 0);
		break;
	default:
		break;
	}
}

void
DCPPanel::film_content_changed (int property)
{
	if (property == FFmpegContentProperty::AUDIO_STREAM ||
	    property == SubtitleContentProperty::USE_SUBTITLES ||
	    property == VideoContentProperty::VIDEO_SCALE) {
		setup_dcp_name ();
	}
}


void
DCPPanel::setup_container ()
{
	int n = 0;
	vector<Ratio const *> ratios = Ratio::all ();
	vector<Ratio const *>::iterator i = ratios.begin ();
	while (i != ratios.end() && *i != _film->container ()) {
		++i;
		++n;
	}
	
	if (i == ratios.end()) {
		checked_set (_container, -1);
	} else {
		checked_set (_container, n);
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
		vector<Ratio const *> ratios = Ratio::all ();
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
	_film = film;
	
	film_changed (Film::NAME);
	film_changed (Film::USE_ISDCF_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::RESOLUTION);
	film_changed (Film::SIGNED);
	film_changed (Film::BURN_SUBTITLES);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::KEY);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::ISDCF_METADATA);
	film_changed (Film::VIDEO_FRAME_RATE);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::SEQUENCE_VIDEO);
	film_changed (Film::THREE_D);
	film_changed (Film::INTEROP);
}

void
DCPPanel::set_general_sensitivity (bool s)
{
	_name->Enable (s);
	_use_isdcf_name->Enable (s);
	_edit_isdcf_button->Enable (s);
	_dcp_content_type->Enable (s);
	_copy_isdcf_name_button->Enable (s);

	bool si = s;
	if (_film && _film->encrypted ()) {
		si = false;
	}
	_burn_subtitles->Enable (s);
	_signed->Enable (si);
	
	_encrypted->Enable (s);
	_key->Enable (s && _film && _film->encrypted ());
	_edit_key->Enable (s && _film && _film->encrypted ());
	_frame_rate_choice->Enable (s);
	_frame_rate_spin->Enable (s);
	_audio_channels->Enable (s);
	_j2k_bandwidth->Enable (s);
	_container->Enable (s);
	_best_frame_rate->Enable (s && _film && _film->best_video_frame_rate () != _film->video_frame_rate ());
	_resolution->Enable (s);
	_three_d->Enable (s);
	_standard->Enable (s);
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

	ISDCFMetadataDialog* d = new ISDCFMetadataDialog (_panel, _film->isdcf_metadata ());
	d->ShowModal ();
	_film->set_isdcf_metadata (d->isdcf_metadata ());
	d->Destroy ();
}

void
DCPPanel::setup_dcp_name ()
{
	string s = _film->dcp_name (true);
	if (s.length() > 28) {
		_dcp_name->SetLabel (std_to_wx (s.substr (0, 28)) + N_("..."));
		_dcp_name->SetToolTip (std_to_wx (s));
	} else {
		_dcp_name->SetLabel (std_to_wx (s));
	}
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
	
	add_label_to_grid_bag_sizer (grid, panel, _("Container"), true, wxGBPosition (r, 0));
	_container = new wxChoice (panel, wxID_ANY);
	grid->Add (_container, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, panel, _("Frame Rate"), true, wxGBPosition (r, 0));
		_frame_rate_sizer = new wxBoxSizer (wxHORIZONTAL);
		_frame_rate_choice = new wxChoice (panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_choice, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_spin = new wxSpinCtrl (panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_spin, 1, wxALIGN_CENTER_VERTICAL);
		setup_frame_rate_widget ();
		_best_frame_rate = new wxButton (panel, wxID_ANY, _("Use best"));
		_frame_rate_sizer->Add (_best_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		grid->Add (_frame_rate_sizer, wxGBPosition (r, 1));
	}
	++r;

	_burn_subtitles = new wxCheckBox (panel, wxID_ANY, _("Burn subtitles into image"));
	grid->Add (_burn_subtitles, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	_three_d = new wxCheckBox (panel, wxID_ANY, _("3D"));
	grid->Add (_three_d, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_grid_bag_sizer (grid, panel, _("Resolution"), true, wxGBPosition (r, 0));
	_resolution = new wxChoice (panel, wxID_ANY);
	grid->Add (_resolution, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, panel, _("JPEG2000 bandwidth"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, panel, _("Mbit/s"), false);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	_container->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&DCPPanel::container_changed, this));
	_frame_rate_choice->Bind(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&DCPPanel::frame_rate_choice_changed, this));
	_frame_rate_spin->Bind  (wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&DCPPanel::frame_rate_spin_changed, this));
	_best_frame_rate->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&DCPPanel::best_frame_rate_clicked, this));
	_burn_subtitles->Bind   (wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&DCPPanel::burn_subtitles_toggled, this));
	_j2k_bandwidth->Bind	(wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&DCPPanel::j2k_bandwidth_changed, this));
	/* Also listen to wxEVT_COMMAND_TEXT_UPDATED so that typing numbers directly in is always noticed */
	_j2k_bandwidth->Bind	(wxEVT_COMMAND_TEXT_UPDATED,          boost::bind (&DCPPanel::j2k_bandwidth_changed, this));
	_resolution->Bind       (wxEVT_COMMAND_CHOICE_SELECTED,       boost::bind (&DCPPanel::resolution_changed, this));
	_three_d->Bind	 	(wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&DCPPanel::three_d_changed, this));

	vector<Ratio const *> const ratio = Ratio::all ();
	for (vector<Ratio const *>::const_iterator i = ratio.begin(); i != ratio.end(); ++i) {
		_container->Append (std_to_wx ((*i)->nickname ()));
	}

	list<int> const dfr = Config::instance()->allowed_dcp_frame_rates ();
	for (list<int>::const_iterator i = dfr.begin(); i != dfr.end(); ++i) {
		_frame_rate_choice->Append (std_to_wx (boost::lexical_cast<string> (*i)));
	}

	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	_frame_rate_spin->SetRange (1, 480);

	_resolution->Append (_("2K"));
	_resolution->Append (_("4K"));

	return panel;
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
	add_label_to_grid_bag_sizer (grid, panel, _("Channels"), true, wxGBPosition (r, 0));
	_audio_channels = new wxChoice (panel, wxID_ANY);
	for (int i = 2; i <= 12; i += 2) {
		_audio_channels->Append (wxString::Format ("%d", i));
	}
	grid->Add (_audio_channels, wxGBPosition (r, 1));
	++r;

	_audio_channels->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&DCPPanel::audio_channels_changed, this));

	return panel;
}

void
DCPPanel::copy_isdcf_name_button_clicked ()
{
	_film->set_name (_film->isdcf_name (false));
	_film->set_use_isdcf_name (false);
}
