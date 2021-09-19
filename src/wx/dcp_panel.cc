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


#include "audio_dialog.h"
#include "check_box.h"
#include "check_box.h"
#include "dcp_panel.h"
#include "dcpomatic_button.h"
#include "dcpomatic_spin_ctrl.h"
#include "focus_manager.h"
#include "interop_metadata_dialog.h"
#include "language_tag_dialog.h"
#include "markers_dialog.h"
#include "smpte_metadata_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/ratio.h"
#include "lib/config.h"
#include "lib/dcp_content_type.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/ffmpeg_content.h"
#include "lib/audio_processor.h"
#include "lib/video_content.h"
#include "lib/text_content.h"
#include "lib/dcp_content.h"
#include "lib/audio_content.h"
#include <dcp/locale_convert.h>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/gbsizer.h>
#include <wx/spinctrl.h>
#include <boost/lexical_cast.hpp>
#include <iostream>


using std::cout;
using std::list;
using std::string;
using std::vector;
using std::pair;
using std::max;
using std::make_pair;
using boost::lexical_cast;
using std::shared_ptr;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;


DCPPanel::DCPPanel (wxNotebook* n, shared_ptr<Film> film, weak_ptr<FilmViewer> viewer)
	: _film (film)
	, _viewer (viewer)
	, _generally_sensitive (true)
{
	_panel = new wxPanel (n);
	_sizer = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (_sizer);

	_grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (_grid, 0, wxEXPAND | wxALL, 8);

	_name_label = create_label (_panel, _("Name"), true);
	_name = new wxTextCtrl (_panel, wxID_ANY);
	FocusManager::instance()->add(_name);

	_use_isdcf_name = new CheckBox (_panel, _("Use ISDCF name"));
	_copy_isdcf_name_button = new Button (_panel, _("Copy as name"));

	/* wxST_ELLIPSIZE_MIDDLE works around a bug in GTK2 and/or wxWidgets, see
	   http://trac.wxwidgets.org/ticket/12539
	*/
	_dcp_name = new StaticText (
		_panel, wxT (""), wxDefaultPosition, wxDefaultSize,
		wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_MIDDLE
		);

	_enable_audio_language = new wxCheckBox (_panel, wxID_ANY, _("Audio language"));
	_audio_language = new wxStaticText (_panel, wxID_ANY, wxT(""));
	_edit_audio_language = new Button (_panel, _("Edit..."));

	_dcp_content_type_label = create_label (_panel, _("Content Type"), true);
	_dcp_content_type = new wxChoice (_panel, wxID_ANY);

	_encrypted = new CheckBox (_panel, _("Encrypted"));

        wxClientDC dc (_panel);
        auto size = dc.GetTextExtent (wxT ("GGGGGGGG..."));
        size.SetHeight (-1);

	_reels_label = create_label (_panel, _("Reels"), true);
	_reel_type = new wxChoice (_panel, wxID_ANY);

	_reel_length_label = create_label (_panel, _("Reel length"), true);
	_reel_length = new SpinCtrl (_panel, DCPOMATIC_SPIN_CTRL_WIDTH);
	_reel_length_gb_label = create_label (_panel, _("GB"), false);

	_standard_label = create_label (_panel, _("Standard"), true);
	_standard = new wxChoice (_panel, wxID_ANY);

	_markers = new Button (_panel, _("Markers..."));
	_metadata = new Button (_panel, _("Metadata..."));

	_notebook = new wxNotebook (_panel, wxID_ANY);
	_sizer->Add (_notebook, 1, wxEXPAND | wxTOP, 6);

	_notebook->AddPage (make_video_panel (), _("Video"), false);
	_notebook->AddPage (make_audio_panel (), _("Audio"), false);

	_name->Bind		     (wxEVT_TEXT,     boost::bind(&DCPPanel::name_changed, this));
	_use_isdcf_name->Bind	     (wxEVT_CHECKBOX, boost::bind(&DCPPanel::use_isdcf_name_toggled, this));
	_copy_isdcf_name_button->Bind(wxEVT_BUTTON,   boost::bind(&DCPPanel::copy_isdcf_name_button_clicked, this));
	_dcp_content_type->Bind	     (wxEVT_CHOICE,   boost::bind(&DCPPanel::dcp_content_type_changed, this));
	_encrypted->Bind             (wxEVT_CHECKBOX, boost::bind(&DCPPanel::encrypted_toggled, this));
	_reel_type->Bind             (wxEVT_CHOICE,   boost::bind(&DCPPanel::reel_type_changed, this));
	_reel_length->Bind           (wxEVT_SPINCTRL, boost::bind(&DCPPanel::reel_length_changed, this));
	_standard->Bind              (wxEVT_CHOICE,   boost::bind(&DCPPanel::standard_changed, this));
	_markers->Bind               (wxEVT_BUTTON,   boost::bind(&DCPPanel::markers_clicked, this));
	_metadata->Bind              (wxEVT_BUTTON,   boost::bind(&DCPPanel::metadata_clicked, this));
	_enable_audio_language->Bind (wxEVT_CHECKBOX, boost::bind(&DCPPanel::enable_audio_language_toggled, this));
	_edit_audio_language->Bind   (wxEVT_BUTTON,   boost::bind(&DCPPanel::edit_audio_language_clicked, this));

	for (auto i: DCPContentType::all()) {
		_dcp_content_type->Append (std_to_wx(i->pretty_name()));
	}

	_reel_type->Append (_("Single reel"));
	_reel_type->Append (_("Split by video content"));
	/// TRANSLATORS: translate the word "Custom" here; do not include the "Reel|" prefix
	_reel_type->Append (S_("Reel|Custom"));

	_reel_length->SetRange (1, 64);

	_standard->Append (_("SMPTE"));
	_standard->Append (_("Interop"));

	Config::instance()->Changed.connect (boost::bind(&DCPPanel::config_changed, this, _1));

	add_to_grid ();
}

void
DCPPanel::add_to_grid ()
{
	int r = 0;

	auto name_sizer = new wxBoxSizer (wxHORIZONTAL);
	name_sizer->Add (_name_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	name_sizer->Add (_name, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	_grid->Add (name_sizer, wxGBPosition(r, 0), wxGBSpan(1, 2), wxRIGHT | wxEXPAND, DCPOMATIC_DIALOG_BORDER);
	++r;

	int flags = wxALIGN_CENTER_VERTICAL;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
#endif

	_grid->Add (_use_isdcf_name, wxGBPosition(r, 0), wxDefaultSpan, flags);
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_copy_isdcf_name_button, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		_grid->Add (s, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
	}
	++r;

	_grid->Add (_dcp_name, wxGBPosition(r, 0), wxGBSpan(1, 2), wxALIGN_CENTER_VERTICAL | wxEXPAND);
	++r;

	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_enable_audio_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_GAP);
		s->Add (_audio_language, 1, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
		s->Add (_edit_audio_language, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
		_grid->Add (s, wxGBPosition(r, 0), wxGBSpan(1, 2), wxEXPAND | wxALIGN_CENTER_VERTICAL);
	}
	++r;

	add_label_to_sizer (_grid, _dcp_content_type_label, true, wxGBPosition(r, 0));
	_grid->Add (_dcp_content_type, wxGBPosition(r, 1));
	++r;

	_grid->Add (_encrypted, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	add_label_to_sizer (_grid, _reels_label, true, wxGBPosition(r, 0));
	_grid->Add (_reel_type, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (_grid, _reel_length_label, true, wxGBPosition(r, 0));
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_reel_length);
		add_label_to_sizer (s, _reel_length_gb_label, false, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);
		_grid->Add (s, wxGBPosition(r, 1));
	}
	++r;

	add_label_to_sizer (_grid, _standard_label, true, wxGBPosition(r, 0));
	_grid->Add (_standard, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	auto extra = new wxBoxSizer (wxHORIZONTAL);
	extra->Add (_markers, 1, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	extra->Add (_metadata, 1, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_grid->Add (extra, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;
}


void
DCPPanel::name_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_name (string(_name->GetValue().mb_str()));
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
DCPPanel::encrypted_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_encrypted (_encrypted->GetValue());
}


/** Called when the frame rate choice widget has been changed */
void
DCPPanel::frame_rate_choice_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (
		boost::lexical_cast<int>(
			wx_to_std(_frame_rate_choice->GetString(_frame_rate_choice->GetSelection()))
			),
		true
		);
}


/** Called when the frame rate spin widget has been changed */
void
DCPPanel::frame_rate_spin_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (_frame_rate_spin->GetValue());
}


void
DCPPanel::audio_channels_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_audio_channels (locale_convert<int>(string_client_data(_audio_channels->GetClientObject(_audio_channels->GetSelection()))));
}


void
DCPPanel::resolution_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_resolution (_resolution->GetSelection() == 0 ? Resolution::TWO_K : Resolution::FOUR_K);
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
DCPPanel::markers_clicked ()
{
	if (_markers_dialog) {
		_markers_dialog->Destroy ();
		_markers_dialog = nullptr;
	}

	_markers_dialog = new MarkersDialog (_panel, _film, _viewer);
	_markers_dialog->Show();
}


void
DCPPanel::metadata_clicked ()
{
	if (_film->interop()) {
		if (_interop_metadata_dialog) {
			_interop_metadata_dialog->Destroy ();
			_interop_metadata_dialog = nullptr;
		}

		_interop_metadata_dialog = new InteropMetadataDialog (_panel, _film);
		_interop_metadata_dialog->setup ();
		_interop_metadata_dialog->Show ();
	} else {
		if (_smpte_metadata_dialog) {
			_smpte_metadata_dialog->Destroy ();
			_smpte_metadata_dialog = nullptr;
		}

		_smpte_metadata_dialog = new SMPTEMetadataDialog (_panel, _film);
		_smpte_metadata_dialog->setup ();
		_smpte_metadata_dialog->Show ();
	}
}


void
DCPPanel::film_changed (Film::Property p)
{
	switch (p) {
	case Film::Property::NONE:
		break;
	case Film::Property::CONTAINER:
		setup_container ();
		break;
	case Film::Property::NAME:
		checked_set (_name, _film->name());
		setup_dcp_name ();
		break;
	case Film::Property::DCP_CONTENT_TYPE:
	{
		auto index = DCPContentType::as_index(_film->dcp_content_type());
		DCPOMATIC_ASSERT (index);
		checked_set (_dcp_content_type, *index);
		setup_dcp_name ();
		break;
	}
	case Film::Property::ENCRYPTED:
		checked_set (_encrypted, _film->encrypted ());
		break;
	case Film::Property::RESOLUTION:
		checked_set (_resolution, _film->resolution() == Resolution::TWO_K ? 0 : 1);
		setup_container ();
		setup_dcp_name ();
		break;
	case Film::Property::J2K_BANDWIDTH:
		checked_set (_j2k_bandwidth, _film->j2k_bandwidth() / 1000000);
		break;
	case Film::Property::USE_ISDCF_NAME:
	{
		checked_set (_use_isdcf_name, _film->use_isdcf_name());
		if (_film->use_isdcf_name()) {
			/* We are going back to using an ISDCF name.  Remove anything after a _ in the current name,
			   in case the user has clicked 'Copy as name' then re-ticked 'Use ISDCF name' (#1513).
			*/
			string const name = _film->name ();
			string::size_type const u = name.find("_");
			if (u != string::npos) {
				_film->set_name (name.substr(0, u));
			}
		}
		setup_dcp_name ();
		break;
	}
	case Film::Property::VIDEO_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _frame_rate_choice->GetCount(); ++i) {
			if (wx_to_std(_frame_rate_choice->GetString(i)) == boost::lexical_cast<string>(_film->video_frame_rate())) {
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
	case Film::Property::AUDIO_CHANNELS:
		if (_film->audio_channels() < minimum_allowed_audio_channels()) {
			_film->set_audio_channels (minimum_allowed_audio_channels());
		} else {
			checked_set (_audio_channels, locale_convert<string>(max(minimum_allowed_audio_channels(), _film->audio_channels())));
			setup_dcp_name ();
		}
		break;
	case Film::Property::THREE_D:
		checked_set (_three_d, _film->three_d());
		setup_dcp_name ();
		break;
	case Film::Property::REENCODE_J2K:
		checked_set (_reencode_j2k, _film->reencode_j2k());
		break;
	case Film::Property::INTEROP:
		checked_set (_standard, _film->interop() ? 1 : 0);
		setup_dcp_name ();
		_markers->Enable (!_film->interop());
		break;
	case Film::Property::AUDIO_PROCESSOR:
		if (_film->audio_processor()) {
			checked_set (_audio_processor, _film->audio_processor()->id());
		} else {
			checked_set (_audio_processor, 0);
		}
		setup_audio_channels_choice (_audio_channels, minimum_allowed_audio_channels());
		film_changed (Film::Property::AUDIO_CHANNELS);
		break;
	case Film::Property::REEL_TYPE:
		checked_set (_reel_type, static_cast<int>(_film->reel_type()));
		_reel_length->Enable (_film->reel_type() == ReelType::BY_LENGTH);
		break;
	case Film::Property::REEL_LENGTH:
		checked_set (_reel_length, _film->reel_length() / 1000000000LL);
		break;
	case Film::Property::CONTENT:
		setup_dcp_name ();
		setup_sensitivity ();
		break;
	case Film::Property::AUDIO_LANGUAGE:
	{
		auto al = _film->audio_language();
		checked_set (_enable_audio_language, static_cast<bool>(al));
		checked_set (_audio_language, al ? std_to_wx(al->to_string()) : wxT(""));
		setup_dcp_name ();
		setup_sensitivity ();
		break;
	}
	case Film::Property::CONTENT_VERSIONS:
	case Film::Property::VERSION_NUMBER:
	case Film::Property::RELEASE_TERRITORY:
	case Film::Property::RATINGS:
	case Film::Property::FACILITY:
	case Film::Property::STUDIO:
	case Film::Property::TEMP_VERSION:
	case Film::Property::PRE_RELEASE:
	case Film::Property::RED_BAND:
	case Film::Property::TWO_D_VERSION_OF_THREE_D:
	case Film::Property::CHAIN:
	case Film::Property::LUMINANCE:
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
	    property == TextContentProperty::USE ||
	    property == TextContentProperty::BURN ||
	    property == TextContentProperty::LANGUAGE ||
	    property == TextContentProperty::LANGUAGE_IS_ADDITIONAL ||
	    property == VideoContentProperty::CUSTOM_RATIO ||
	    property == VideoContentProperty::CUSTOM_SIZE ||
	    property == VideoContentProperty::BURNT_SUBTITLE_LANGUAGE ||
	    property == VideoContentProperty::CROP ||
	    property == DCPContentProperty::REFERENCE_VIDEO ||
	    property == DCPContentProperty::REFERENCE_AUDIO ||
	    property == DCPContentProperty::REFERENCE_TEXT) {
		setup_dcp_name ();
		setup_sensitivity ();
	}
}


void
DCPPanel::setup_container ()
{
	int n = 0;
	auto ratios = Ratio::containers ();
	auto i = ratios.begin ();
	while (i != ratios.end() && *i != _film->container()) {
		++i;
		++n;
	}

	if (i == ratios.end()) {
		checked_set (_container, -1);
		checked_set (_container_size, wxT(""));
	} else {
		checked_set (_container, n);
		auto const size = fit_ratio_within (_film->container()->ratio(), _film->full_frame ());
		checked_set (_container_size, wxString::Format("%dx%d", size.width, size.height));
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
		auto ratios = Ratio::containers ();
		DCPOMATIC_ASSERT (n < int(ratios.size()));
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
		_film->set_dcp_content_type (DCPContentType::from_index(n));
	}
}


void
DCPPanel::set_film (shared_ptr<Film> film)
{
	/* We are changing film, so destroy any dialogs for the old one */
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = nullptr;
	}
	if (_markers_dialog) {
		_markers_dialog->Destroy ();
		_markers_dialog = nullptr;
	}
	if (_interop_metadata_dialog) {
		_interop_metadata_dialog->Destroy ();
		_interop_metadata_dialog = nullptr;
	}
	if (_smpte_metadata_dialog) {
		_smpte_metadata_dialog->Destroy ();
		_smpte_metadata_dialog = nullptr;
	}

	_film = film;

	if (!_film) {
		/* Really should all the film_changed below but this might be enough */
		checked_set (_dcp_name, wxT(""));
		set_general_sensitivity (false);
		return;
	}

	film_changed (Film::Property::NAME);
	film_changed (Film::Property::USE_ISDCF_NAME);
	film_changed (Film::Property::CONTENT);
	film_changed (Film::Property::DCP_CONTENT_TYPE);
	film_changed (Film::Property::CONTAINER);
	film_changed (Film::Property::RESOLUTION);
	film_changed (Film::Property::ENCRYPTED);
	film_changed (Film::Property::J2K_BANDWIDTH);
	film_changed (Film::Property::VIDEO_FRAME_RATE);
	film_changed (Film::Property::AUDIO_CHANNELS);
	film_changed (Film::Property::SEQUENCE);
	film_changed (Film::Property::THREE_D);
	film_changed (Film::Property::INTEROP);
	film_changed (Film::Property::AUDIO_PROCESSOR);
	film_changed (Film::Property::REEL_TYPE);
	film_changed (Film::Property::REEL_LENGTH);
	film_changed (Film::Property::REENCODE_J2K);
	film_changed (Film::Property::AUDIO_LANGUAGE);

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
	_dcp_content_type->Enable       (_generally_sensitive);
	_copy_isdcf_name_button->Enable (_generally_sensitive);
	_enable_audio_language->Enable  (_generally_sensitive);
	_audio_language->Enable         (_enable_audio_language->GetValue());
	_edit_audio_language->Enable    (_enable_audio_language->GetValue());
	_encrypted->Enable              (_generally_sensitive);
	_reel_type->Enable              (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->references_dcp_audio());
	_reel_length->Enable            (_generally_sensitive && _film && _film->reel_type() == ReelType::BY_LENGTH);
	_markers->Enable                (_generally_sensitive && _film && !_film->interop());
	_metadata->Enable               (_generally_sensitive);
	_frame_rate_choice->Enable      (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->contains_atmos_content());
	_frame_rate_spin->Enable        (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->contains_atmos_content());
	_audio_channels->Enable         (_generally_sensitive && _film && !_film->references_dcp_audio() && !_film->contains_atmos_content());
	_audio_processor->Enable        (_generally_sensitive && _film && !_film->references_dcp_audio());
	_j2k_bandwidth->Enable          (_generally_sensitive && _film && !_film->references_dcp_video());
	_container->Enable              (_generally_sensitive && _film && !_film->references_dcp_video());
	_best_frame_rate->Enable (
		_generally_sensitive &&
		_film &&
		_film->best_video_frame_rate () != _film->video_frame_rate() &&
		!_film->references_dcp_video() &&
		!_film->contains_atmos_content()
		);
	_resolution->Enable             (_generally_sensitive && _film && !_film->references_dcp_video());
	_three_d->Enable                (_generally_sensitive && _film && !_film->references_dcp_video());

	_standard->Enable (
		_generally_sensitive &&
		_film &&
		!_film->references_dcp_video() &&
		!_film->references_dcp_audio() &&
		!_film->contains_atmos_content()
		);

	_reencode_j2k->Enable           (_generally_sensitive && _film);
	_show_audio->Enable             (_generally_sensitive && _film);
}


void
DCPPanel::use_isdcf_name_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_use_isdcf_name (_use_isdcf_name->GetValue());
}

void
DCPPanel::setup_dcp_name ()
{
	_dcp_name->SetLabel (std_to_wx(_film->dcp_name(true)));
	_dcp_name->SetToolTip (std_to_wx(_film->dcp_name(true)));
}


void
DCPPanel::best_frame_rate_clicked ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (_film->best_video_frame_rate());
}


void
DCPPanel::three_d_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_three_d (_three_d->GetValue());
}


void
DCPPanel::reencode_j2k_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_reencode_j2k (_reencode_j2k->GetValue());
}


void
DCPPanel::config_changed (Config::Property p)
{
	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	setup_frame_rate_widget ();

	if (p == Config::SHOW_EXPERIMENTAL_AUDIO_PROCESSORS) {
		_audio_processor->Clear ();
		add_audio_processors ();
		if (_film) {
			film_changed (Film::Property::AUDIO_PROCESSOR);
		}
	}
}


void
DCPPanel::setup_frame_rate_widget ()
{
	if (Config::instance()->allow_any_dcp_frame_rate()) {
		_frame_rate_choice->Hide ();
		_frame_rate_spin->Show ();
	} else {
		_frame_rate_choice->Show ();
		_frame_rate_spin->Hide ();
	}
}


wxPanel *
DCPPanel::make_video_panel ()
{
	auto panel = new wxPanel (_notebook);
	auto sizer = new wxBoxSizer (wxVERTICAL);
	_video_grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->Add (_video_grid, 0, wxALL, 8);
	panel->SetSizer (sizer);

	_container_label = create_label (panel, _("Container"), true);
	_container = new wxChoice (panel, wxID_ANY);
	_container_size = new StaticText (panel, wxT (""));

	_resolution_label = create_label (panel, _("Resolution"), true);
	_resolution = new wxChoice (panel, wxID_ANY);

	_frame_rate_label = create_label (panel, _("Frame Rate"), true);
	_frame_rate_choice = new wxChoice (panel, wxID_ANY);
	_frame_rate_spin = new SpinCtrl (panel, DCPOMATIC_SPIN_CTRL_WIDTH);
	setup_frame_rate_widget ();
	_best_frame_rate = new Button (panel, _("Use best"));

	_three_d = new CheckBox (panel, _("3D"));

	_j2k_bandwidth_label = create_label (panel, _("JPEG2000 bandwidth\nfor newly-encoded data"), true);
	_j2k_bandwidth = new SpinCtrl (panel, DCPOMATIC_SPIN_CTRL_WIDTH);
	_mbits_label = create_label (panel, _("Mbit/s"), false);

	_reencode_j2k = new CheckBox (panel, _("Re-encode JPEG2000 data from input"));

	_container->Bind	 (wxEVT_CHOICE,	  boost::bind(&DCPPanel::container_changed, this));
	_frame_rate_choice->Bind (wxEVT_CHOICE,	  boost::bind(&DCPPanel::frame_rate_choice_changed, this));
	_frame_rate_spin->Bind   (wxEVT_SPINCTRL, boost::bind(&DCPPanel::frame_rate_spin_changed, this));
	_best_frame_rate->Bind	 (wxEVT_BUTTON,	  boost::bind(&DCPPanel::best_frame_rate_clicked, this));
	_j2k_bandwidth->Bind	 (wxEVT_SPINCTRL, boost::bind(&DCPPanel::j2k_bandwidth_changed, this));
	/* Also listen to wxEVT_TEXT so that typing numbers directly in is always noticed */
	_j2k_bandwidth->Bind	 (wxEVT_TEXT,     boost::bind(&DCPPanel::j2k_bandwidth_changed, this));
	_resolution->Bind        (wxEVT_CHOICE,   boost::bind(&DCPPanel::resolution_changed, this));
	_three_d->Bind	 	 (wxEVT_CHECKBOX, boost::bind(&DCPPanel::three_d_changed, this));
	_reencode_j2k->Bind      (wxEVT_CHECKBOX, boost::bind(&DCPPanel::reencode_j2k_changed, this));

	for (auto i: Ratio::containers()) {
		_container->Append (std_to_wx(i->container_nickname()));
	}

	for (auto i: Config::instance()->allowed_dcp_frame_rates()) {
		_frame_rate_choice->Append (std_to_wx (boost::lexical_cast<string> (i)));
	}

	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	_frame_rate_spin->SetRange (1, 480);

	_resolution->Append (_("2K"));
	_resolution->Append (_("4K"));

	add_video_panel_to_grid ();

	return panel;
}


void
DCPPanel::add_video_panel_to_grid ()
{
	int r = 0;

	add_label_to_sizer (_video_grid, _container_label, true, wxGBPosition (r, 0));
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_container, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		s->Add (_container_size, 1, wxLEFT | wxALIGN_CENTER_VERTICAL);
		_video_grid->Add (s, wxGBPosition(r, 1));
		++r;
	}

	add_label_to_sizer (_video_grid, _resolution_label, true, wxGBPosition (r, 0));
	_video_grid->Add (_resolution, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_video_grid, _frame_rate_label, true, wxGBPosition (r, 0));
	{
		_frame_rate_sizer = new wxBoxSizer (wxHORIZONTAL);
		_frame_rate_sizer->Add (_frame_rate_choice, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_sizer->Add (_frame_rate_spin, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_sizer->Add (_best_frame_rate, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
		_video_grid->Add (_frame_rate_sizer, wxGBPosition (r, 1));
		++r;
	}

	_video_grid->Add (_three_d, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_sizer (_video_grid, _j2k_bandwidth_label, true, wxGBPosition (r, 0));
	auto s = new wxBoxSizer (wxHORIZONTAL);
	s->Add (_j2k_bandwidth, 0, wxALIGN_CENTER_VERTICAL);
	add_label_to_sizer (s, _mbits_label, false, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);
	_video_grid->Add (s, wxGBPosition(r, 1), wxDefaultSpan);
	++r;
	_video_grid->Add (_reencode_j2k, wxGBPosition(r, 0), wxGBSpan(1, 2));
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
	auto panel = new wxPanel (_notebook);
	_audio_panel_sizer = new wxBoxSizer (wxVERTICAL);
	_audio_grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_audio_panel_sizer->Add (_audio_grid, 0, wxALL, 8);
	panel->SetSizer (_audio_panel_sizer);

	_channels_label = create_label (panel, _("Channels"), true);
	_audio_channels = new wxChoice (panel, wxID_ANY);
	setup_audio_channels_choice (_audio_channels, minimum_allowed_audio_channels ());

	_processor_label = create_label (panel, _("Processor"), true);
	_audio_processor = new wxChoice (panel, wxID_ANY);
	add_audio_processors ();

	_show_audio = new Button (panel, _("Show graph of audio levels..."));

	_audio_channels->Bind (wxEVT_CHOICE, boost::bind (&DCPPanel::audio_channels_changed, this));
	_audio_processor->Bind (wxEVT_CHOICE, boost::bind (&DCPPanel::audio_processor_changed, this));
	_show_audio->Bind (wxEVT_BUTTON, boost::bind (&DCPPanel::show_audio_clicked, this));

	add_audio_panel_to_grid ();

	return panel;
}


void
DCPPanel::add_audio_panel_to_grid ()
{
	int r = 0;

	add_label_to_sizer (_audio_grid, _channels_label, true, wxGBPosition (r, 0));
	_audio_grid->Add (_audio_channels, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (_audio_grid, _processor_label, true, wxGBPosition (r, 0));
	_audio_grid->Add (_audio_processor, wxGBPosition (r, 1));
	++r;

	_audio_grid->Add (_show_audio, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;
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

	auto const s = string_client_data (_audio_processor->GetClientObject (_audio_processor->GetSelection ()));
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
		_audio_dialog = nullptr;
	}

	auto d = new AudioDialog (_panel, _film, _viewer);
	d->Show ();
}


void
DCPPanel::reel_type_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_reel_type (static_cast<ReelType>(_reel_type->GetSelection()));
}


void
DCPPanel::reel_length_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_reel_length (_reel_length->GetValue() * 1000000000LL);
}


void
DCPPanel::add_audio_processors ()
{
	_audio_processor->Append (_("None"), new wxStringClientData(N_("none")));
	for (auto ap: AudioProcessor::visible()) {
		_audio_processor->Append (std_to_wx(ap->name()), new wxStringClientData(std_to_wx(ap->id())));
	}
	_audio_panel_sizer->Layout();
}


void
DCPPanel::enable_audio_language_toggled ()
{
	setup_sensitivity ();
	if (_enable_audio_language->GetValue()) {
		auto al = wx_to_std (_audio_language->GetLabel());
		_film->set_audio_language (al.empty() ? dcp::LanguageTag("en-US") : dcp::LanguageTag(al));
	} else {
		_film->set_audio_language (boost::none);
	}
}


void
DCPPanel::edit_audio_language_clicked ()
{
       DCPOMATIC_ASSERT (_film->audio_language());
       auto d = new LanguageTagDialog (_panel, *_film->audio_language());
       d->ShowModal ();
       _film->set_audio_language(d->get());
       d->Destroy ();
}

