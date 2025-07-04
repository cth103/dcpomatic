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
#include "dcp_timeline_dialog.h"
#include "dcpomatic_button.h"
#include "dcpomatic_choice.h"
#include "dcpomatic_spin_ctrl.h"
#include "focus_manager.h"
#include "interop_metadata_dialog.h"
#include "language_tag_dialog.h"
#include "markers_dialog.h"
#include "smpte_metadata_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/audio_processor.h"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include <dcp/locale_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/lexical_cast.hpp>


using std::list;
using std::make_pair;
using std::max;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::lexical_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::locale_convert;


DCPPanel::DCPPanel(wxNotebook* n, shared_ptr<Film> film, FilmViewer& viewer)
	: _film(film)
	, _viewer(viewer)
	, _generally_sensitive(true)
{
	_panel = new wxPanel(n);
	_sizer = new wxBoxSizer(wxVERTICAL);
	_panel->SetSizer(_sizer);

	_grid = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add(_grid, 0, wxEXPAND | wxALL, 8);

	_name_label = create_label(_panel, _("Name"), true);
	_name = new wxTextCtrl(_panel, wxID_ANY);
	FocusManager::instance()->add(_name);

	_use_isdcf_name = new CheckBox(_panel, _("Use ISDCF name"));
	_copy_isdcf_name_button = new Button(_panel, _("Copy as name"));

	/* wxST_ELLIPSIZE_MIDDLE works around a bug in GTK2 and/or wxWidgets, see
	   http://trac.wxwidgets.org/ticket/12539
	*/
	_dcp_name = new StaticText(
		_panel, {}, wxDefaultPosition, wxDefaultSize,
		wxALIGN_CENTRE_HORIZONTAL | wxST_NO_AUTORESIZE | wxST_ELLIPSIZE_MIDDLE
		);

	_dcp_content_type_label = create_label(_panel, _("Content Type"), true);
	_dcp_content_type = new Choice(_panel);

	_encrypted = new CheckBox(_panel, _("Encrypted"));

        wxClientDC dc(_panel);
        auto size = dc.GetTextExtent(char_to_wx("GGGGGGGG..."));
        size.SetHeight(-1);

	_standard_label = create_label(_panel, _("Standard"), true);
	_standard = new Choice(_panel);

	_markers = new Button(_panel, _("Markers..."));
	_metadata = new Button(_panel, _("Metadata..."));
	_reels = new Button(_panel, _("Reels..."));

	_notebook = new wxNotebook(_panel, wxID_ANY);
	_sizer->Add(_notebook, 1, wxEXPAND | wxTOP, 6);

	_notebook->AddPage(make_video_panel(), _("Video"), false);
	_notebook->AddPage(make_audio_panel(), _("Audio"), false);

	_name->Bind(wxEVT_TEXT, boost::bind(&DCPPanel::name_changed, this));
	_use_isdcf_name->bind(&DCPPanel::use_isdcf_name_toggled, this);
	_copy_isdcf_name_button->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::copy_isdcf_name_button_clicked, this));
	_dcp_content_type->Bind(wxEVT_CHOICE, boost::bind(&DCPPanel::dcp_content_type_changed, this));
	_encrypted->bind(&DCPPanel::encrypted_toggled, this);
	_standard->Bind(wxEVT_CHOICE, boost::bind(&DCPPanel::standard_changed, this));
	_markers->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::markers_clicked, this));
	_metadata->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::metadata_clicked, this));
	_reels->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::reels_clicked, this));

	for (auto i: DCPContentType::all()) {
		_dcp_content_type->add_entry(i->pretty_name());
	}

	update_standards();
	_standard->SetToolTip(_("The standard that the DCP should use.  Interop is older, and SMPTE is the newer (current) standard.  If in doubt, choose 'SMPTE'"));

	Config::instance()->Changed.connect(boost::bind(&DCPPanel::config_changed, this, _1));

	add_to_grid();

	/* Allow the 3rd column to grow when the panel is made wider: the project name and
	 * ISDCF name extend into this column.
	 */
	_grid->AddGrowableCol(2, 1);
}


void
DCPPanel::update_standards()
{
	_standard->Clear();
	_standard->add_entry(_("SMPTE"), string{"smpte"});
	if (Config::instance()->allow_smpte_bv20() || (_film && _film->limit_to_smpte_bv20())) {
		_standard->add_entry(_("SMPTE (Bv2.0 only)"), string{"smpte-bv20"});
	}
	_standard->add_entry(_("Interop"), string{"interop"});
	_standard->add_entry(_("MPEG2 Interop"), string{"mpeg2-interop"});
	_sizer->Layout();
}


void
DCPPanel::set_standard()
{
	DCPOMATIC_ASSERT(_film);

	if (_film->interop()) {
		if (_film->video_encoding() == VideoEncoding::JPEG2000) {
			checked_set(_standard, "interop");
		} else {
			checked_set(_standard, "mpeg2-interop");
		}
	} else {
		checked_set(_standard, _film->limit_to_smpte_bv20() ? "smpte-bv20" : "smpte");
	}
}


void
DCPPanel::standard_changed()
{
	if (!_film || !_standard->get()) {
		return;
	}

	auto const data = _standard->get_data();
	if (!data) {
		return;
	}

	if (*data == "interop") {
		_film->set_interop(true);
		_film->set_limit_to_smpte_bv20(false);
		_film->set_video_encoding(VideoEncoding::JPEG2000);
	} else if (*data == "smpte") {
		_film->set_interop(false);
		_film->set_limit_to_smpte_bv20(false);
		_film->set_video_encoding(VideoEncoding::JPEG2000);
	} else if (*data == "smpte-bv20") {
		_film->set_interop(false);
		_film->set_limit_to_smpte_bv20(true);
		_film->set_video_encoding(VideoEncoding::JPEG2000);
	} else if (*data == "mpeg2-interop") {
		_film->set_interop(true);
		_film->set_video_encoding(VideoEncoding::MPEG2);
	}
}


void
DCPPanel::add_to_grid()
{
	int r = 0;

	auto name_sizer = new wxBoxSizer(wxHORIZONTAL);
	name_sizer->Add(_name_label, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	name_sizer->Add(_name, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
	_grid->Add(name_sizer, wxGBPosition(r, 0), wxGBSpan(1, 3), wxEXPAND);
	++r;

	int flags = wxALIGN_CENTER_VERTICAL;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
#endif

	_grid->Add(_use_isdcf_name, wxGBPosition(r, 0), wxDefaultSpan, flags);
	{
		auto s = new wxBoxSizer(wxHORIZONTAL);
		s->Add(_copy_isdcf_name_button, 0, wxLEFT, DCPOMATIC_SIZER_X_GAP);
		_grid->Add(s, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
	}
	++r;

	_grid->Add(_dcp_name, wxGBPosition(r, 0), wxGBSpan(1, 3), wxALIGN_CENTER_VERTICAL | wxEXPAND);
	++r;

	add_label_to_sizer(_grid, _dcp_content_type_label, true, wxGBPosition(r, 0));
	_grid->Add(_dcp_content_type, wxGBPosition(r, 1));
	++r;

	_grid->Add(_encrypted, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	add_label_to_sizer(_grid, _standard_label, true, wxGBPosition(r, 0));
	_grid->Add(_standard, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	auto extra = new wxBoxSizer(wxHORIZONTAL);
	extra->Add(_markers, 1, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	extra->Add(_metadata, 1, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	extra->Add(_reels, 1, wxRIGHT, DCPOMATIC_SIZER_X_GAP);
	_grid->Add(extra, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;
}


void
DCPPanel::name_changed()
{
	if (!_film) {
		return;
	}

	_film->set_name(wx_to_std(_name->GetValue()));
}


void
DCPPanel::video_bit_rate_changed()
{
	if (!_film) {
		return;
	}

	_film->set_video_bit_rate(_film->video_encoding(), _video_bit_rate->GetValue() * 1000000);
}


void
DCPPanel::encrypted_toggled()
{
	if (!_film) {
		return;
	}

	_film->set_encrypted(_encrypted->GetValue());
}


/** Called when the frame rate choice widget has been changed */
void
DCPPanel::frame_rate_choice_changed()
{
	if (!_film || !_frame_rate_choice->get()) {
		return;
	}

	_film->set_video_frame_rate(
		boost::lexical_cast<int>(
			wx_to_std(_frame_rate_choice->GetString(*_frame_rate_choice->get()))
			),
		true
		);
}


/** Called when the frame rate spin widget has been changed */
void
DCPPanel::frame_rate_spin_changed()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate(_frame_rate_spin->GetValue());
}


void
DCPPanel::audio_channels_changed()
{
	if (!_film) {
		return;
	}

	_film->set_audio_channels(locale_convert<int>(string_client_data(_audio_channels->GetClientObject(*_audio_channels->get()))));
}


void
DCPPanel::resolution_changed()
{
	if (!_film || !_resolution->get()) {
		return;
	}

	_film->set_resolution(*_resolution->get() == 0 ? Resolution::TWO_K : Resolution::FOUR_K);
}


void
DCPPanel::markers_clicked()
{
	_markers_dialog.reset(_panel, _film, _viewer);
	_markers_dialog->Show();
}


void
DCPPanel::metadata_clicked()
{
	if (_film->interop()) {
		_interop_metadata_dialog.reset(_panel, _film);
		_interop_metadata_dialog->setup();
		_interop_metadata_dialog->Show();
	} else {
		_smpte_metadata_dialog.reset(_panel, _film);
		_smpte_metadata_dialog->setup();
		_smpte_metadata_dialog->Show();
	}
}


void
DCPPanel::reels_clicked()
{
	_dcp_timeline.reset(_panel, _film);
	_dcp_timeline->Show();
}


void
DCPPanel::film_changed(FilmProperty p)
{
	switch (p) {
	case FilmProperty::NONE:
		break;
	case FilmProperty::CONTAINER:
		setup_container();
		break;
	case FilmProperty::NAME:
		checked_set(_name, _film->name());
		setup_dcp_name();
		break;
	case FilmProperty::DCP_CONTENT_TYPE:
	{
		auto index = DCPContentType::as_index(_film->dcp_content_type());
		DCPOMATIC_ASSERT(index);
		checked_set(_dcp_content_type, *index);
		setup_dcp_name();
		break;
	}
	case FilmProperty::ENCRYPTED:
		checked_set(_encrypted, _film->encrypted());
		break;
	case FilmProperty::RESOLUTION:
		checked_set(_resolution, _film->resolution() == Resolution::TWO_K ? 0 : 1);
		setup_container();
		setup_dcp_name();
		break;
	case FilmProperty::VIDEO_BIT_RATE:
		checked_set(_video_bit_rate, _film->video_bit_rate(_film->video_encoding()) / 1000000);
		break;
	case FilmProperty::USE_ISDCF_NAME:
	{
		checked_set(_use_isdcf_name, _film->use_isdcf_name());
		setup_dcp_name();
		break;
	}
	case FilmProperty::VIDEO_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _frame_rate_choice->GetCount(); ++i) {
			if (wx_to_std(_frame_rate_choice->GetString(i)) == boost::lexical_cast<string>(_film->video_frame_rate())) {
				checked_set(_frame_rate_choice, i);
				done = true;
				break;
			}
		}

		if (!done) {
			checked_set(_frame_rate_choice, -1);
		}

		checked_set(_frame_rate_spin, _film->video_frame_rate());

		_best_frame_rate->Enable(_film->best_video_frame_rate() != _film->video_frame_rate());
		setup_dcp_name();
		break;
	}
	case FilmProperty::AUDIO_CHANNELS:
		if (_film->audio_channels() < minimum_allowed_audio_channels()) {
			_film->set_audio_channels(minimum_allowed_audio_channels());
		} else {
			checked_set(_audio_channels, locale_convert<string>(max(minimum_allowed_audio_channels(), _film->audio_channels())));
			setup_dcp_name();
		}
		break;
	case FilmProperty::THREE_D:
		checked_set(_three_d, _film->three_d());
		setup_dcp_name();
		break;
	case FilmProperty::REENCODE_J2K:
		checked_set(_reencode_j2k, _film->reencode_j2k());
		break;
	case FilmProperty::INTEROP:
		update_standards();
		set_standard();
		setup_dcp_name();
		_markers->Enable(!_film->interop());
		break;
	case FilmProperty::VIDEO_ENCODING:
		set_standard();
		setup_container();
		setup_sensitivity();
		film_changed(FilmProperty::VIDEO_BIT_RATE);
		break;
	case FilmProperty::LIMIT_TO_SMPTE_BV20:
		update_standards();
		set_standard();
		break;
	case FilmProperty::AUDIO_PROCESSOR:
		if (_film->audio_processor()) {
			checked_set(_audio_processor, _film->audio_processor()->id());
		} else {
			checked_set(_audio_processor, 0);
		}
		setup_audio_channels_choice(_audio_channels, minimum_allowed_audio_channels());
		film_changed(FilmProperty::AUDIO_CHANNELS);
		break;
	case FilmProperty::CONTENT:
		setup_dcp_name();
		setup_sensitivity();
		/* Maybe we now have ATMOS content which changes our minimum_allowed_audio_channels */
		setup_audio_channels_choice(_audio_channels, minimum_allowed_audio_channels());
		film_changed(FilmProperty::AUDIO_CHANNELS);
		break;
	case FilmProperty::AUDIO_LANGUAGE:
	{
		auto al = _film->audio_language();
		checked_set(_enable_audio_language, static_cast<bool>(al));
		checked_set(_audio_language, al ? std_to_wx(al->as_string()) : wxString{});
		setup_dcp_name();
		setup_sensitivity();
		_audio_panel_sizer->Layout();
		break;
	}
	case FilmProperty::AUDIO_FRAME_RATE:
		if (_audio_sample_rate) {
			checked_set(_audio_sample_rate, _film->audio_frame_rate() == 48000 ? 0 : 1);
		}
		break;
	case FilmProperty::CONTENT_VERSIONS:
	case FilmProperty::VERSION_NUMBER:
	case FilmProperty::RELEASE_TERRITORY:
	case FilmProperty::RATINGS:
	case FilmProperty::FACILITY:
	case FilmProperty::STUDIO:
	case FilmProperty::TEMP_VERSION:
	case FilmProperty::PRE_RELEASE:
	case FilmProperty::RED_BAND:
	case FilmProperty::TWO_D_VERSION_OF_THREE_D:
	case FilmProperty::CHAIN:
	case FilmProperty::LUMINANCE:
	case FilmProperty::TERRITORY_TYPE:
		setup_dcp_name();
		break;
	default:
		break;
	}
}


void
DCPPanel::film_content_changed(int property)
{
	switch (property) {
	case AudioContentProperty::STREAMS:
	case TextContentProperty::USE:
	case TextContentProperty::BURN:
	case TextContentProperty::LANGUAGE:
	case TextContentProperty::LANGUAGE_IS_ADDITIONAL:
	case TextContentProperty::TYPE:
	case TextContentProperty::DCP_TRACK:
	case VideoContentProperty::CUSTOM_RATIO:
	case VideoContentProperty::CUSTOM_SIZE:
	case VideoContentProperty::BURNT_SUBTITLE_LANGUAGE:
	case VideoContentProperty::CROP:
	case DCPContentProperty::REFERENCE_VIDEO:
	case DCPContentProperty::REFERENCE_AUDIO:
	case DCPContentProperty::REFERENCE_TEXT:
		setup_dcp_name();
		setup_sensitivity();
		break;
	}
}


void
DCPPanel::setup_container()
{
	auto ratios = Ratio::containers();
	if (std::find(ratios.begin(), ratios.end(), _film->container()) == ratios.end()) {
		ratios.push_back(_film->container());
	}

	wxArrayString new_ratios;
	for (auto ratio: ratios) {
		new_ratios.Add(std_to_wx(ratio.container_nickname()));
	}

	_container->set_entries(new_ratios);

	auto iter = std::find_if(ratios.begin(), ratios.end(), [this](Ratio const& ratio) { return ratio == _film->container(); });
	DCPOMATIC_ASSERT(iter != ratios.end());

	checked_set(_container, iter - ratios.begin());
	auto const size = fit_ratio_within(_film->container().ratio(), _film->full_frame());
	checked_set(_container_size, wxString::Format(char_to_wx("%dx%d"), size.width, size.height));

	setup_dcp_name();

	_video_grid->Layout();
}


/** Called when the container widget has been changed */
void
DCPPanel::container_changed()
{
	if (!_film) {
		return;
	}

	if (auto const container = _container->get()) {
		auto ratios = Ratio::containers();
		DCPOMATIC_ASSERT(*container < int(ratios.size()));
		_film->set_container(ratios[*container]);
	}
}


/** Called when the DCP content type widget has been changed */
void
DCPPanel::dcp_content_type_changed()
{
	if (!_film) {
		return;
	}

	if (auto const type = _dcp_content_type->get()) {
		_film->set_dcp_content_type(DCPContentType::from_index(*type));
	}
}


void
DCPPanel::set_film(shared_ptr<Film> film)
{
	/* We are changing film, so destroy any dialogs for the old one */
	_audio_dialog.reset();
	_markers_dialog.reset();
	_interop_metadata_dialog.reset();
	_smpte_metadata_dialog.reset();

	_film = film;

	if (!_film) {
		/* Really should all the film_changed below but this might be enough */
		checked_set(_dcp_name, wxString{});
		set_general_sensitivity(false);
		return;
	}

	update_standards();

	film_changed(FilmProperty::NAME);
	film_changed(FilmProperty::USE_ISDCF_NAME);
	film_changed(FilmProperty::CONTENT);
	film_changed(FilmProperty::DCP_CONTENT_TYPE);
	film_changed(FilmProperty::CONTAINER);
	film_changed(FilmProperty::RESOLUTION);
	film_changed(FilmProperty::ENCRYPTED);
	film_changed(FilmProperty::VIDEO_BIT_RATE);
	film_changed(FilmProperty::VIDEO_FRAME_RATE);
	film_changed(FilmProperty::AUDIO_CHANNELS);
	film_changed(FilmProperty::SEQUENCE);
	film_changed(FilmProperty::THREE_D);
	film_changed(FilmProperty::INTEROP);
	film_changed(FilmProperty::AUDIO_PROCESSOR);
	film_changed(FilmProperty::REEL_TYPE);
	film_changed(FilmProperty::REEL_LENGTH);
	film_changed(FilmProperty::REENCODE_J2K);
	film_changed(FilmProperty::AUDIO_LANGUAGE);
	film_changed(FilmProperty::AUDIO_FRAME_RATE);
	film_changed(FilmProperty::LIMIT_TO_SMPTE_BV20);

	set_general_sensitivity(static_cast<bool>(_film));
}


void
DCPPanel::set_general_sensitivity(bool s)
{
	_generally_sensitive = s;
	setup_sensitivity();
}


void
DCPPanel::setup_sensitivity()
{
	auto const mpeg2 = _film && _film->video_encoding() == VideoEncoding::MPEG2;

	_name->Enable                  (_generally_sensitive);
	_use_isdcf_name->Enable        (_generally_sensitive);
	_dcp_content_type->Enable      (_generally_sensitive);
	_copy_isdcf_name_button->Enable(_generally_sensitive);
	_enable_audio_language->Enable (_generally_sensitive);
	_audio_language->Enable        (_enable_audio_language->GetValue());
	_edit_audio_language->Enable   (_enable_audio_language->GetValue());
	_encrypted->Enable             (_generally_sensitive);
	_markers->Enable               (_generally_sensitive && _film && !_film->interop());
	_metadata->Enable              (_generally_sensitive);
	_reels->Enable                 (_generally_sensitive && _film);
	_frame_rate_choice->Enable     (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->contains_atmos_content());
	_frame_rate_spin->Enable       (_generally_sensitive && _film && !_film->references_dcp_video() && !_film->contains_atmos_content());
	_audio_channels->Enable        (_generally_sensitive && _film && !_film->references_dcp_audio());
	_audio_processor->Enable       (_generally_sensitive && _film && !_film->references_dcp_audio());
	_video_bit_rate->Enable        (_generally_sensitive && _film && !_film->references_dcp_video());
	_container->Enable             (_generally_sensitive && _film && !_film->references_dcp_video() && !mpeg2);
	_best_frame_rate->Enable(
		_generally_sensitive &&
		_film &&
		_film->best_video_frame_rate() != _film->video_frame_rate() &&
		!_film->references_dcp_video() &&
		!_film->contains_atmos_content()
		);
	_resolution->Enable            (_generally_sensitive && _film && !_film->references_dcp_video() && !mpeg2);
	_three_d->Enable               (_generally_sensitive && _film && !_film->references_dcp_video() && !mpeg2);

	_standard->Enable(
		_generally_sensitive &&
		_film &&
		!_film->references_dcp_video() &&
		!_film->references_dcp_audio() &&
		!_film->contains_atmos_content()
		);

	_reencode_j2k->Enable          (_generally_sensitive && _film);
	_show_audio->Enable            (_generally_sensitive && _film);
}


void
DCPPanel::use_isdcf_name_toggled()
{
	if (!_film) {
		return;
	}

	auto const new_value = _use_isdcf_name->GetValue();

	_film->set_use_isdcf_name(new_value);

	if (new_value) {
		/* We are going back to using an ISDCF name.  Remove anything after a _ in the current name,
		   in case the user has clicked 'Copy as name' then re-ticked 'Use ISDCF name' (#1513).
		*/
		string const name = _film->name();
		_film->set_name(name.substr(0, name.find("_")));
	}
}


void
DCPPanel::setup_dcp_name()
{
	if (!_film) {
		return;
	}

	_dcp_name->SetLabel(std_to_wx(_film->dcp_name(true)));
	_dcp_name->SetToolTip(std_to_wx(_film->dcp_name(true)));
}


void
DCPPanel::best_frame_rate_clicked()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate(_film->best_video_frame_rate());
}


void
DCPPanel::three_d_changed()
{
	if (!_film) {
		return;
	}

	_film->set_three_d(_three_d->GetValue());
}


void
DCPPanel::reencode_j2k_changed()
{
	if (!_film) {
		return;
	}

	_film->set_reencode_j2k(_reencode_j2k->GetValue());
}


void
DCPPanel::config_changed(Config::Property p)
{
	VideoEncoding const encoding = _film ? _film->video_encoding() : VideoEncoding::JPEG2000;
	_video_bit_rate->SetRange(1, Config::instance()->maximum_video_bit_rate(encoding) / 1000000);
	setup_frame_rate_widget();

	if (p == Config::SHOW_EXPERIMENTAL_AUDIO_PROCESSORS) {
		_audio_processor->Clear();
		add_audio_processors();
		if (_film) {
			film_changed(FilmProperty::AUDIO_PROCESSOR);
		}
	} else if (p == Config::ALLOW_SMPTE_BV20) {
		update_standards();
		if (_film) {
			film_changed(FilmProperty::INTEROP);
			film_changed(FilmProperty::LIMIT_TO_SMPTE_BV20);
		}
	} else if (p == Config::ISDCF_NAME_PART_LENGTH) {
		setup_dcp_name();
	} else if (p == Config::ALLOW_ANY_CONTAINER) {
		setup_container();
	}
}


void
DCPPanel::setup_frame_rate_widget()
{
	if (Config::instance()->allow_any_dcp_frame_rate()) {
		_frame_rate_choice->Hide();
		_frame_rate_spin->Show();
	} else {
		_frame_rate_choice->Show();
		_frame_rate_spin->Hide();
	}
	_frame_rate_sizer->Layout();
}


wxPanel *
DCPPanel::make_video_panel()
{
	auto panel = new wxPanel(_notebook);
	auto sizer = new wxBoxSizer(wxVERTICAL);
	_video_grid = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	sizer->Add(_video_grid, 0, wxALL, 8);
	panel->SetSizer(sizer);

	_container_label = create_label(panel, _("Container"), true);
	_container = new Choice(panel);
	_container_size = new StaticText(panel, {});

	_resolution_label = create_label(panel, _("Resolution"), true);
	_resolution = new Choice(panel);

	_frame_rate_label = create_label(panel, _("Frame Rate"), true);
	_frame_rate_choice = new Choice(panel);
	_frame_rate_spin = new SpinCtrl(panel, DCPOMATIC_SPIN_CTRL_WIDTH);
	_best_frame_rate = new Button(panel, _("Use best"));

	_three_d = new CheckBox(panel, _("3D"));

	_video_bit_rate_label = create_label(panel, _("Video bit rate\nfor newly-encoded data"), true);
	_video_bit_rate = new SpinCtrl(panel, DCPOMATIC_SPIN_CTRL_WIDTH);
	_mbits_label = create_label(panel, _("Mbit/s"), false);

	_reencode_j2k = new CheckBox(panel, _("Re-encode JPEG2000 data from input"));

	_container->Bind	(wxEVT_CHOICE,	  boost::bind(&DCPPanel::container_changed, this));
	_frame_rate_choice->Bind(wxEVT_CHOICE,	  boost::bind(&DCPPanel::frame_rate_choice_changed, this));
	_frame_rate_spin->Bind  (wxEVT_SPINCTRL, boost::bind(&DCPPanel::frame_rate_spin_changed, this));
	_best_frame_rate->Bind	(wxEVT_BUTTON,	  boost::bind(&DCPPanel::best_frame_rate_clicked, this));
	_video_bit_rate->Bind	(wxEVT_SPINCTRL, boost::bind(&DCPPanel::video_bit_rate_changed, this));
	/* Also listen to wxEVT_TEXT so that typing numbers directly in is always noticed */
	_video_bit_rate->Bind	(wxEVT_TEXT,     boost::bind(&DCPPanel::video_bit_rate_changed, this));
	_resolution->Bind       (wxEVT_CHOICE,   boost::bind(&DCPPanel::resolution_changed, this));
	_three_d->bind(&DCPPanel::three_d_changed, this);
	_reencode_j2k->bind(&DCPPanel::reencode_j2k_changed, this);

	for (auto i: Config::instance()->allowed_dcp_frame_rates()) {
		_frame_rate_choice->add_entry(boost::lexical_cast<string>(i));
	}

	VideoEncoding const encoding = _film ? _film->video_encoding() : VideoEncoding::JPEG2000;
	_video_bit_rate->SetRange(1, Config::instance()->maximum_video_bit_rate(encoding) / 1000000);
	_frame_rate_spin->SetRange(1, 480);

	_resolution->add_entry(_("2K"));
	_resolution->add_entry(_("4K"));

	add_video_panel_to_grid();
	setup_frame_rate_widget();

	return panel;
}


void
DCPPanel::add_video_panel_to_grid()
{
	int r = 0;

	add_label_to_sizer(_video_grid, _container_label, true, wxGBPosition(r, 0));
	{
		auto s = new wxBoxSizer(wxHORIZONTAL);
		s->Add(_container, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		s->Add(_container_size, 1, wxLEFT | wxALIGN_CENTER_VERTICAL);
		_video_grid->Add(s, wxGBPosition(r, 1));
		++r;
	}

	add_label_to_sizer(_video_grid, _resolution_label, true, wxGBPosition(r, 0));
	_video_grid->Add(_resolution, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer(_video_grid, _frame_rate_label, true, wxGBPosition(r, 0));
	{
		_frame_rate_sizer = new wxBoxSizer(wxHORIZONTAL);
		_frame_rate_sizer->Add(_frame_rate_choice, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_sizer->Add(_frame_rate_spin, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_sizer->Add(_best_frame_rate, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_X_GAP);
		_video_grid->Add(_frame_rate_sizer, wxGBPosition(r, 1));
		++r;
	}

	_video_grid->Add(_three_d, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;

	add_label_to_sizer(_video_grid, _video_bit_rate_label, true, wxGBPosition(r, 0));
	auto s = new wxBoxSizer(wxHORIZONTAL);
	s->Add(_video_bit_rate, 0, wxALIGN_CENTER_VERTICAL);
	add_label_to_sizer(s, _mbits_label, false, 0, wxLEFT | wxALIGN_CENTER_VERTICAL);
	_video_grid->Add(s, wxGBPosition(r, 1), wxDefaultSpan);
	++r;
	_video_grid->Add(_reencode_j2k, wxGBPosition(r, 0), wxGBSpan(1, 2));
}


int
DCPPanel::minimum_allowed_audio_channels() const
{
	int min = 2;
	if (_film) {
		if (_film->audio_processor()) {
			min = _film->audio_processor()->out_channels();
		}
		if (_film->contains_atmos_content()) {
			min = std::max(min, 14);
		}
	}

	if (min % 2 == 1) {
		++min;
	}

	return min;
}


wxPanel *
DCPPanel::make_audio_panel()
{
	auto panel = new wxPanel(_notebook);
	_audio_panel_sizer = new wxBoxSizer(wxVERTICAL);
	_audio_grid = new wxGridBagSizer(DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_audio_panel_sizer->Add(_audio_grid, 0, wxALL, 8);
	panel->SetSizer(_audio_panel_sizer);

	_channels_label = create_label(panel, _("Channels"), true);
	_audio_channels = new Choice(panel);
	setup_audio_channels_choice(_audio_channels, minimum_allowed_audio_channels());

	if (Config::instance()->allow_96khz_audio()) {
		_audio_sample_rate_label = create_label(panel, _("Sample rate"), true);
		_audio_sample_rate = new wxChoice(panel, wxID_ANY);
	}

	_processor_label = create_label(panel, _("Processor"), true);
	_audio_processor = new Choice(panel);
	add_audio_processors();

	_enable_audio_language = new CheckBox(panel, _("Language"));
	_audio_language = new wxStaticText(panel, wxID_ANY, {});
	_edit_audio_language = new Button(panel, _("Edit..."));

	_show_audio = new Button(panel, _("Show graph of audio levels..."));

	_audio_channels->Bind(wxEVT_CHOICE, boost::bind(&DCPPanel::audio_channels_changed, this));
	if (_audio_sample_rate) {
		_audio_sample_rate->Bind(wxEVT_CHOICE, boost::bind(&DCPPanel::audio_sample_rate_changed, this));
	}
	_audio_processor->Bind(wxEVT_CHOICE, boost::bind(&DCPPanel::audio_processor_changed, this));

	_enable_audio_language->bind(&DCPPanel::enable_audio_language_toggled, this);
	_edit_audio_language->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::edit_audio_language_clicked, this));

	_show_audio->Bind(wxEVT_BUTTON, boost::bind(&DCPPanel::show_audio_clicked, this));

	if (_audio_sample_rate) {
		_audio_sample_rate->Append(_("48kHz"));
		_audio_sample_rate->Append(_("96kHz"));
	}

	add_audio_panel_to_grid();

	return panel;
}


void
DCPPanel::add_audio_panel_to_grid()
{
	int r = 0;

	add_label_to_sizer(_audio_grid, _channels_label, true, wxGBPosition(r, 0));
	_audio_grid->Add(_audio_channels, wxGBPosition(r, 1));
	++r;

	if (_audio_sample_rate_label && _audio_sample_rate) {
		add_label_to_sizer(_audio_grid, _audio_sample_rate_label, true, wxGBPosition(r, 0));
		_audio_grid->Add(_audio_sample_rate, wxGBPosition(r, 1));
		++r;
	}

	add_label_to_sizer(_audio_grid, _processor_label, true, wxGBPosition(r, 0));
	_audio_grid->Add(_audio_processor, wxGBPosition(r, 1));
	++r;

	{
		auto s = new wxBoxSizer(wxHORIZONTAL);
		s->Add(_enable_audio_language, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_GAP);
		s->Add(_audio_language, 1, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
		s->Add(DCPOMATIC_SIZER_X_GAP, 0);
		s->Add(_edit_audio_language, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, DCPOMATIC_CHECKBOX_BOTTOM_PAD);
		_audio_grid->Add(s, wxGBPosition(r, 0), wxGBSpan(1, 2), wxEXPAND | wxALIGN_CENTER_VERTICAL);
	}
	++r;

	_audio_grid->Add(_show_audio, wxGBPosition(r, 0), wxGBSpan(1, 2));
	++r;
}


void
DCPPanel::copy_isdcf_name_button_clicked()
{
	auto name = _film->name();
	if (name.length() > 20 && std::count(name.begin(), name.end(), '_') > 6) {
		/* At a guess, the existing film name is itself an ISDCF name, so chop
		 * off the actual name part first.
		 */
		_film->set_name(name.substr(0, name.find("_")));
	}
	_film->set_name(_film->isdcf_name(true));
	_film->set_use_isdcf_name(false);
}


void
DCPPanel::audio_processor_changed()
{
	if (!_film || !_audio_processor->get()) {
		return;
	}

	auto const s = string_client_data(_audio_processor->GetClientObject(*_audio_processor->get()));
	_film->set_audio_processor(AudioProcessor::from_id(s));
}


void
DCPPanel::show_audio_clicked()
{
	if (!_film) {
		return;
	}

	_audio_dialog.reset(_panel, _film, _viewer);
	_audio_dialog->Show();
}


void
DCPPanel::add_audio_processors()
{
	_audio_processor->add_entry(_("None"), string{"none"});
	for (auto ap: AudioProcessor::visible()) {
		_audio_processor->add_entry(std_to_wx(ap->name()), new wxStringClientData(std_to_wx(ap->id())));
	}
	_audio_panel_sizer->Layout();
}


void
DCPPanel::enable_audio_language_toggled()
{
	setup_sensitivity();
	if (_enable_audio_language->GetValue()) {
		auto al = wx_to_std(_audio_language->GetLabel());
		_film->set_audio_language(al.empty() ? dcp::LanguageTag("en-US") : dcp::LanguageTag(al));
	} else {
		_film->set_audio_language(boost::none);
	}
}


void
DCPPanel::edit_audio_language_clicked()
{
       DCPOMATIC_ASSERT(_film->audio_language());
       LanguageTagDialog dialog(_panel, *_film->audio_language());
       if (dialog.ShowModal() == wxID_OK) {
	       _film->set_audio_language(dialog.get());
       }
}


void
DCPPanel::audio_sample_rate_changed()
{
	if (_audio_sample_rate) {
		_film->set_audio_frame_rate(_audio_sample_rate->GetSelection() == 0 ? 48000 : 96000);
	}
}

