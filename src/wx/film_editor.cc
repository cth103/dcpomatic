/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/film_editor.cc
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */

#include <iostream>
#include <iomanip>
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "lib/format.h"
#include "lib/film.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/ab_transcode_job.h"
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/config.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/imagemagick_content.h"
#include "lib/sndfile_content.h"
#include "lib/dcp_content_type.h"
#include "filter_dialog.h"
#include "wx_util.h"
#include "film_editor.h"
#include "gain_calculator_dialog.h"
#include "sound_processor.h"
#include "dci_metadata_dialog.h"
#include "scaler.h"
#include "audio_dialog.h"
#include "imagemagick_content_dialog.h"
#include "ffmpeg_content_dialog.h"
#include "audio_mapping_view.h"

using std::string;
using std::cout;
using std::stringstream;
using std::pair;
using std::fixed;
using std::setprecision;
using std::list;
using std::vector;
using std::max;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

/** @param f Film to edit */
FilmEditor::FilmEditor (shared_ptr<Film> f, wxWindow* parent)
	: wxPanel (parent)
	, _generally_sensitive (true)
	, _audio_dialog (0)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_notebook, 1);

	make_film_panel ();
	_notebook->AddPage (_film_panel, _("Film"), true);
	make_content_panel ();
	_notebook->AddPage (_content_panel, _("Content"), false);
	make_video_panel ();
	_notebook->AddPage (_video_panel, _("Video"), false);
	make_audio_panel ();
	_notebook->AddPage (_audio_panel, _("Audio"), false);
	make_subtitle_panel ();
	_notebook->AddPage (_subtitle_panel, _("Subtitles"), false);

	setup_formats ();

	set_film (f);
	connect_to_widgets ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmEditor::active_jobs_changed, this, _1)
		);
	
	SetSizerAndFit (s);
}

void
FilmEditor::make_film_panel ()
{
	_film_panel = new wxPanel (_notebook);
	_film_sizer = new wxBoxSizer (wxVERTICAL);
	_film_panel->SetSizer (_film_sizer);

	wxGridBagSizer* grid = new wxGridBagSizer (4, 4);
	_film_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;
	
	add_label_to_grid_bag_sizer (grid, _film_panel, _("Name"), wxGBPosition (r, 0));
	_name = new wxTextCtrl (_film_panel, wxID_ANY);
	grid->Add (_name, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;
	
	add_label_to_grid_bag_sizer (grid, _film_panel, _("DCP Name"), wxGBPosition (r, 0));
	_dcp_name = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	grid->Add (_dcp_name, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_use_dci_name = new wxCheckBox (_film_panel, wxID_ANY, _("Use DCI name"));
	grid->Add (_use_dci_name, wxGBPosition (r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_edit_dci_button = new wxButton (_film_panel, wxID_ANY, _("Details..."));
	grid->Add (_edit_dci_button, wxGBPosition (r, 1), wxDefaultSpan);
	++r;

	_trust_content_headers = new wxCheckBox (_film_panel, wxID_ANY, _("Trust content's header"));
	grid->Add (_trust_content_headers, wxGBPosition (r, 0), wxGBSpan(1, 2));
	++r;

	add_label_to_grid_bag_sizer (grid, _film_panel, _("Content Type"), wxGBPosition (r, 0));
	_dcp_content_type = new wxChoice (_film_panel, wxID_ANY);
	grid->Add (_dcp_content_type, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _film_panel, _("DCP Frame Rate"), wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_dcp_frame_rate = new wxChoice (_film_panel, wxID_ANY);
		s->Add (_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		_best_dcp_frame_rate = new wxButton (_film_panel, wxID_ANY, _("Use best"));
		s->Add (_best_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL | wxALL | wxEXPAND, 6);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	_frame_rate_description = new wxStaticText (_film_panel, wxID_ANY, wxT ("\n \n "), wxDefaultPosition, wxDefaultSize);
	grid->Add (_frame_rate_description, wxGBPosition (r, 0), wxGBSpan (1, 2), wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	wxFont font = _frame_rate_description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_frame_rate_description->SetFont(font);
	++r;
	
	add_label_to_grid_bag_sizer (grid, _film_panel, _("Length"), wxGBPosition (r, 0));
	_length = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	grid->Add (_length, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;


	{
		add_label_to_grid_bag_sizer (grid, _film_panel, _("Trim frames"), wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		add_label_to_sizer (s, _film_panel, _("Start"));
		_trim_start = new wxSpinCtrl (_film_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_trim_start);
		add_label_to_sizer (s, _film_panel, _("End"));
		_trim_end = new wxSpinCtrl (_film_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_trim_end);

		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _film_panel, _("Trim method"), wxGBPosition (r, 0));
	_trim_type = new wxChoice (_film_panel, wxID_ANY);
	grid->Add (_trim_type, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_ab = new wxCheckBox (_film_panel, wxID_ANY, _("A/B"));
	grid->Add (_ab, wxGBPosition (r, 0));
	++r;

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
	}

	list<int> const dfr = Config::instance()->allowed_dcp_frame_rates ();
	for (list<int>::const_iterator i = dfr.begin(); i != dfr.end(); ++i) {
		_dcp_frame_rate->Append (std_to_wx (boost::lexical_cast<string> (*i)));
	}

	_trim_type->Append (_("encode all frames and play the subset"));
	_trim_type->Append (_("encode only the subset"));
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Connect                   (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED,         wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_use_dci_name->Connect           (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::use_dci_name_toggled), 0, this);
	_edit_dci_button->Connect        (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::edit_dci_button_clicked), 0, this);
	_format->Connect                 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::format_changed), 0, this);
	_trust_content_headers->Connect  (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::trust_content_headers_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_SELECTED,   wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,  wxListEventHandler    (FilmEditor::content_activated), 0, this);
	_content_add->Connect            (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_add_clicked), 0, this);
	_content_remove->Connect         (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_remove_clicked), 0, this);
	_content_edit->Connect           (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_edit_clicked), 0, this);
	_content_earlier->Connect        (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_earlier_clicked), 0, this);
	_content_later->Connect          (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_later_clicked), 0, this);
	_loop_content->Connect           (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::loop_content_toggled), 0, this);
	_loop_count->Connect             (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::loop_count_changed), 0, this);
	_left_crop->Connect              (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::left_crop_changed), 0, this);
	_right_crop->Connect             (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::right_crop_changed), 0, this);
	_top_crop->Connect               (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::top_crop_changed), 0, this);
	_bottom_crop->Connect            (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::bottom_crop_changed), 0, this);
	_filters_button->Connect         (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::edit_filters_clicked), 0, this);
	_scaler->Connect                 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::scaler_changed), 0, this);
	_dcp_content_type->Connect       (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::dcp_content_type_changed), 0, this);
	_dcp_frame_rate->Connect         (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::dcp_frame_rate_changed), 0, this);
	_best_dcp_frame_rate->Connect    (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::best_dcp_frame_rate_clicked), 0, this);
	_ab->Connect                     (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::ab_toggled), 0, this);
	_trim_start->Connect             (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::trim_start_changed), 0, this);
	_trim_end->Connect               (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::trim_end_changed), 0, this);
	_trim_type->Connect              (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::trim_type_changed), 0, this);
	_with_subtitles->Connect         (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::with_subtitles_toggled), 0, this);
	_subtitle_offset->Connect        (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::subtitle_offset_changed), 0, this);
	_subtitle_scale->Connect         (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::subtitle_scale_changed), 0, this);
	_colour_lut->Connect             (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::colour_lut_changed), 0, this);
	_j2k_bandwidth->Connect          (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::j2k_bandwidth_changed), 0, this);
	_audio_gain->Connect             (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::audio_gain_changed), 0, this);
	_audio_gain_calculate_button->Connect (
		wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::audio_gain_calculate_button_clicked), 0, this
		);
	_show_audio->Connect             (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::show_audio_clicked), 0, this);
	_audio_delay->Connect            (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,     wxCommandEventHandler (FilmEditor::audio_delay_changed), 0, this);
}

void
FilmEditor::make_video_panel ()
{
	_video_panel = new wxPanel (_notebook);
	_video_sizer = new wxBoxSizer (wxVERTICAL);
	_video_panel->SetSizer (_video_sizer);
	
	wxGridBagSizer* grid = new wxGridBagSizer (4, 4);
	_video_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;
	add_label_to_grid_bag_sizer (grid, _video_panel, _("Format"), wxGBPosition (r, 0));
	_format = new wxChoice (_video_panel, wxID_ANY);
	grid->Add (_format, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Left crop"), wxGBPosition (r, 0));
	_left_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_left_crop, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Right crop"), wxGBPosition (r, 0));
	_right_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_right_crop, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, _video_panel, _("Top crop"), wxGBPosition (r, 0));
	_top_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_top_crop, wxGBPosition (r, 1));
	++r;
	
	add_label_to_grid_bag_sizer (grid, _video_panel, _("Bottom crop"), wxGBPosition (r, 0));
	_bottom_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	grid->Add (_bottom_crop, wxGBPosition (r, 1));
	++r;

	_scaling_description = new wxStaticText (_video_panel, wxID_ANY, wxT ("\n \n \n \n"), wxDefaultPosition, wxDefaultSize);
	grid->Add (_scaling_description, wxGBPosition (r, 0), wxGBSpan (1, 2), wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	wxFont font = _scaling_description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_scaling_description->SetFont(font);
	++r;

	/* VIDEO-only stuff */
	{
		add_label_to_grid_bag_sizer (grid, _video_panel, _("Filters"), wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_filters = new wxStaticText (_video_panel, wxID_ANY, _("None"));
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (_video_panel, wxID_ANY, _("Edit..."));
		s->Add (_filters_button, 0);
		grid->Add (s, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Scaler"), wxGBPosition (r, 0));
	_scaler = new wxChoice (_video_panel, wxID_ANY);
	grid->Add (_scaler, wxGBPosition (r, 1));
	++r;

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler->Append (std_to_wx ((*i)->name()));
	}

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Colour look-up table"), wxGBPosition (r, 0));
	_colour_lut = new wxChoice (_video_panel, wxID_ANY);
	for (int i = 0; i < 2; ++i) {
		_colour_lut->Append (std_to_wx (colour_lut_index_to_name (i)));
	}
	_colour_lut->SetSelection (0);
	grid->Add (_colour_lut, wxGBPosition (r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _video_panel, _("JPEG2000 bandwidth"), wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (_video_panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, _video_panel, _("MBps"));
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	_left_crop->SetRange (0, 1024);
	_top_crop->SetRange (0, 1024);
	_right_crop->SetRange (0, 1024);
	_bottom_crop->SetRange (0, 1024);
	_trim_start->SetRange (0, 100);
	_trim_end->SetRange (0, 100);
	_j2k_bandwidth->SetRange (50, 250);
}

void
FilmEditor::make_content_panel ()
{
	_content_panel = new wxPanel (_notebook);
	_content_sizer = new wxBoxSizer (wxVERTICAL);
	_content_panel->SetSizer (_content_sizer);
	
        {
                wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
                
                _content = new wxListCtrl (_content_panel, wxID_ANY, wxDefaultPosition, wxSize (320, 160), wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL);
                s->Add (_content, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);

                _content->InsertColumn (0, wxT(""));
		_content->SetColumnWidth (0, 512);

                wxBoxSizer* b = new wxBoxSizer (wxVERTICAL);
                _content_add = new wxButton (_content_panel, wxID_ANY, _("Add..."));
                b->Add (_content_add);
                _content_remove = new wxButton (_content_panel, wxID_ANY, _("Remove"));
                b->Add (_content_remove);
                _content_edit = new wxButton (_content_panel, wxID_ANY, _("Edit..."));
                b->Add (_content_edit);
                _content_earlier = new wxButton (_content_panel, wxID_ANY, _("Earlier"));
                b->Add (_content_earlier);
                _content_later = new wxButton (_content_panel, wxID_ANY, _("Later"));
                b->Add (_content_later);

                s->Add (b, 0, wxALL, 4);

                _content_sizer->Add (s, 0.75, wxEXPAND | wxALL, 6);
        }

	_content_information = new wxTextCtrl (_content_panel, wxID_ANY, wxT ("\n \n "), wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);
	_content_sizer->Add (_content_information, 1, wxEXPAND | wxALL, 6);

	wxBoxSizer* h = new wxBoxSizer (wxHORIZONTAL);
	_loop_content = new wxCheckBox (_content_panel, wxID_ANY, _("Loop everything"));
	h->Add (_loop_content, 0, wxALL, 6);
	_loop_count = new wxSpinCtrl (_content_panel, wxID_ANY);
	h->Add (_loop_count, 0, wxALL, 6);
	add_label_to_sizer (h, _content_panel, _("times"));
	_content_sizer->Add (h, 0, wxALL, 6);

	_playlist_description = new wxStaticText (_content_panel, wxID_ANY, wxT ("\n \n \n \n "));
	_content_sizer->Add (_playlist_description, 0.25, wxEXPAND | wxALL, 6);
	wxFont font = _playlist_description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_playlist_description->SetFont(font);

	_loop_count->SetRange (2, 1024);
}

void
FilmEditor::make_audio_panel ()
{
	_audio_panel = new wxPanel (_notebook);
	_audio_sizer = new wxBoxSizer (wxVERTICAL);
	_audio_panel->SetSizer (_audio_sizer);
	
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	_audio_sizer->Add (grid, 0, wxALL, 8);

	_show_audio = new wxButton (_audio_panel, wxID_ANY, _("Show Audio..."));
	grid->Add (_show_audio, 1);
	grid->AddSpacer (0);

	{
		add_label_to_sizer (grid, _audio_panel, _("Audio Gain"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_gain = new wxSpinCtrl (_audio_panel);
		s->Add (_audio_gain, 1);
		add_label_to_sizer (s, _audio_panel, _("dB"));
		_audio_gain_calculate_button = new wxButton (_audio_panel, wxID_ANY, _("Calculate..."));
		s->Add (_audio_gain_calculate_button, 1, wxEXPAND);
		grid->Add (s);
	}

	{
		add_label_to_sizer (grid, _audio_panel, _("Audio Delay"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_delay = new wxSpinCtrl (_audio_panel);
		s->Add (_audio_delay, 1);
		/// TRANSLATORS: this is an abbreviation for milliseconds, the unit of time
		add_label_to_sizer (s, _audio_panel, _("ms"));
		grid->Add (s);
	}

	_audio_mapping = new AudioMappingView (_audio_panel);
	_audio_sizer->Add (_audio_mapping, 1, wxEXPAND | wxALL, 6);
	
	_audio_gain->SetRange (-60, 60);
	_audio_delay->SetRange (-1000, 1000);
}

void
FilmEditor::make_subtitle_panel ()
{
	_subtitle_panel = new wxPanel (_notebook);
	_subtitle_sizer = new wxBoxSizer (wxVERTICAL);
	_subtitle_panel->SetSizer (_subtitle_sizer);
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	_subtitle_sizer->Add (grid, 0, wxALL, 8);

	_with_subtitles = new wxCheckBox (_subtitle_panel, wxID_ANY, _("With Subtitles"));
	grid->Add (_with_subtitles, 1);
	grid->AddSpacer (0);
	
	{
		add_label_to_sizer (grid, _subtitle_panel, _("Subtitle Offset"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_subtitle_offset = new wxSpinCtrl (_subtitle_panel);
		s->Add (_subtitle_offset);
		add_label_to_sizer (s, _subtitle_panel, _("pixels"));
		grid->Add (s);
	}

	{
		add_label_to_sizer (grid, _subtitle_panel, _("Subtitle Scale"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_subtitle_scale = new wxSpinCtrl (_subtitle_panel);
		s->Add (_subtitle_scale);
		add_label_to_sizer (s, _subtitle_panel, _("%"));
		grid->Add (s);
	}

	_subtitle_offset->SetRange (-1024, 1024);
	_subtitle_scale->SetRange (1, 1000);
}

/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_left_crop (_left_crop->GetValue ());
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_right_crop (_right_crop->GetValue ());
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_top_crop (_top_crop->GetValue ());
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_bottom_crop (_bottom_crop->GetValue ());
}

void
FilmEditor::trust_content_headers_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_trust_content_headers (_trust_content_headers->GetValue ());
}

/** Called when the DCP A/B switch has been toggled */
void
FilmEditor::ab_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_ab (_ab->GetValue ());
}

/** Called when the name widget has been changed */
void
FilmEditor::name_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_name (string (_name->GetValue().mb_str()));
}

void
FilmEditor::subtitle_offset_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_subtitle_offset (_subtitle_offset->GetValue ());
}

void
FilmEditor::subtitle_scale_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_subtitle_scale (_subtitle_scale->GetValue() / 100.0);
}

void
FilmEditor::colour_lut_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_colour_lut (_colour_lut->GetSelection ());
}

void
FilmEditor::j2k_bandwidth_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1e6);
}

void
FilmEditor::dcp_frame_rate_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_dcp_frame_rate (
		boost::lexical_cast<int> (
			wx_to_std (_dcp_frame_rate->GetString (_dcp_frame_rate->GetSelection ()))
			)
		);
}


/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_changed (Film::Property p)
{
	ensure_ui_thread ();
	
	if (!_film) {
		return;
	}

	stringstream s;
		
	switch (p) {
	case Film::NONE:
		break;
	case Film::CONTENT:
		setup_content ();
		setup_formats ();
		setup_format ();
		setup_subtitle_control_sensitivity ();
		setup_show_audio_sensitivity ();
		setup_length ();
		break;
	case Film::LOOP:
		checked_set (_loop_content, _film->loop() > 1);
		checked_set (_loop_count, _film->loop());
		setup_loop_sensitivity ();
		break;
	case Film::TRUST_CONTENT_HEADERS:
		checked_set (_trust_content_headers, _film->trust_content_headers ());
		break;
	case Film::FORMAT:
		setup_format ();
		break;
	case Film::CROP:
		checked_set (_left_crop, _film->crop().left);
		checked_set (_right_crop, _film->crop().right);
		checked_set (_top_crop, _film->crop().top);
		checked_set (_bottom_crop, _film->crop().bottom);
		setup_scaling_description ();
		break;
	case Film::FILTERS:
	{
		pair<string, string> p = Filter::ffmpeg_strings (_film->filters ());
		if (p.first.empty () && p.second.empty ()) {
			_filters->SetLabel (_("None"));
		} else {
			string const b = p.first + " " + p.second;
			_filters->SetLabel (std_to_wx (b));
		}
		_film_sizer->Layout ();
		break;
	}
	case Film::NAME:
		checked_set (_name, _film->name());
		setup_dcp_name ();
		break;
	case Film::DCP_CONTENT_TYPE:
		checked_set (_dcp_content_type, DCPContentType::as_index (_film->dcp_content_type ()));
		setup_dcp_name ();
		break;
	case Film::AB:
		checked_set (_ab, _film->ab ());
		break;
	case Film::SCALER:
		checked_set (_scaler, Scaler::as_index (_film->scaler ()));
		break;
	case Film::TRIM_START:
		checked_set (_trim_start, _film->trim_start());
		break;
	case Film::TRIM_END:
		checked_set (_trim_end, _film->trim_end());
		break;
	case Film::TRIM_TYPE:
		checked_set (_trim_type, _film->trim_type() == Film::CPL ? 0 : 1);
		break;
	case Film::AUDIO_GAIN:
		checked_set (_audio_gain, _film->audio_gain ());
		break;
	case Film::AUDIO_DELAY:
		checked_set (_audio_delay, _film->audio_delay ());
		break;
	case Film::WITH_SUBTITLES:
		checked_set (_with_subtitles, _film->with_subtitles ());
		setup_subtitle_control_sensitivity ();
		setup_dcp_name ();
		break;
	case Film::SUBTITLE_OFFSET:
		checked_set (_subtitle_offset, _film->subtitle_offset ());
		break;
	case Film::SUBTITLE_SCALE:
		checked_set (_subtitle_scale, _film->subtitle_scale() * 100);
		break;
	case Film::COLOUR_LUT:
		checked_set (_colour_lut, _film->colour_lut ());
		break;
	case Film::J2K_BANDWIDTH:
		checked_set (_j2k_bandwidth, double (_film->j2k_bandwidth()) / 1e6);
		break;
	case Film::USE_DCI_NAME:
		checked_set (_use_dci_name, _film->use_dci_name ());
		setup_dcp_name ();
		break;
	case Film::DCI_METADATA:
		setup_dcp_name ();
		break;
	case Film::DCP_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _dcp_frame_rate->GetCount(); ++i) {
			if (wx_to_std (_dcp_frame_rate->GetString(i)) == boost::lexical_cast<string> (_film->dcp_frame_rate())) {
				checked_set (_dcp_frame_rate, i);
				done = true;
				break;
			}
		}

		if (!done) {
			checked_set (_dcp_frame_rate, -1);
		}

		if (_film->video_frame_rate()) {
			_best_dcp_frame_rate->Enable (best_dcp_frame_rate (_film->video_frame_rate ()) != _film->dcp_frame_rate ());
		} else {
			_best_dcp_frame_rate->Disable ();
		}
		setup_frame_rate_description ();
		break;
	}
	case Film::AUDIO_MAPPING:
		_audio_mapping->set_mapping (_film->audio_mapping ());
		break;
	}
}

void
FilmEditor::film_content_changed (weak_ptr<Content> content, int property)
{
	if (!_film) {
		/* We call this method ourselves (as well as using it as a signal handler)
		   so _film can be 0.
		*/
		return;
	}
		
	if (property == FFmpegContentProperty::SUBTITLE_STREAMS) {
		setup_subtitle_control_sensitivity ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAMS) {
		setup_show_audio_sensitivity ();
	} else if (property == VideoContentProperty::VIDEO_LENGTH || property == AudioContentProperty::AUDIO_LENGTH) {
		setup_length ();
		boost::shared_ptr<Content> c = content.lock ();
		if (c && c == selected_content()) {
			setup_content_information ();
		}
	} else if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_dcp_name ();
		setup_show_audio_sensitivity ();
	}
}

void
FilmEditor::setup_format ()
{
	int n = 0;
	vector<Format const *>::iterator i = _formats.begin ();
	while (i != _formats.end() && *i != _film->format ()) {
		++i;
		++n;
	}
	
	if (i == _formats.end()) {
		checked_set (_format, -1);
	} else {
		checked_set (_format, n);
	}
	
	setup_dcp_name ();
	setup_scaling_description ();
}	

void
FilmEditor::setup_length ()
{
	stringstream s;
	ContentVideoFrame const frames = _film->content_length ();
	
	if (frames && _film->video_frame_rate()) {
		s << frames << " " << wx_to_std (_("frames")) << "; " << seconds_to_hms (frames / _film->video_frame_rate());
	} else if (frames) {
		s << frames << " " << wx_to_std (_("frames"));
	}

	_length->SetLabel (std_to_wx (s.str ()));
	
	if (frames) {
		_trim_start->SetRange (0, frames);
		_trim_end->SetRange (0, frames);
	}
}	

void
FilmEditor::setup_frame_rate_description ()
{
	wxString d;
	int lines = 0;
	
	if (_film->video_frame_rate()) {
		d << std_to_wx (FrameRateConversion (_film->video_frame_rate(), _film->dcp_frame_rate()).description);
		++lines;
#ifdef HAVE_SWRESAMPLE
		if (_film->audio_frame_rate() && _film->audio_frame_rate() != _film->target_audio_sample_rate ()) {
			d << wxString::Format (
				_("Audio will be resampled from %dHz to %dHz\n"),
				_film->audio_frame_rate(),
				_film->target_audio_sample_rate()
				);
			++lines;
		}
#endif		
	}

	for (int i = lines; i < 2; ++i) {
		d << wxT ("\n ");
	}

	_frame_rate_description->SetLabel (d);
}

/** Called when the format widget has been changed */
void
FilmEditor::format_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	int const n = _format->GetSelection ();
	if (n >= 0) {
		assert (n < int (_formats.size()));
		_film->set_format (_formats[n]);
	}
}

/** Called when the DCP content type widget has been changed */
void
FilmEditor::dcp_content_type_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	int const n = _dcp_content_type->GetSelection ();
	if (n != wxNOT_FOUND) {
		_film->set_dcp_content_type (DCPContentType::from_index (n));
	}
}

/** Sets the Film that we are editing */
void
FilmEditor::set_film (shared_ptr<Film> f)
{
	set_things_sensitive (f != 0);

	if (_film == f) {
		return;
	}
	
	_film = f;

	if (_film) {
		_film->Changed.connect (bind (&FilmEditor::film_changed, this, _1));
		_film->ContentChanged.connect (bind (&FilmEditor::film_content_changed, this, _1, _2));
	}

	if (_film) {
		FileChanged (_film->directory ());
	} else {
		FileChanged ("");
	}

	if (_audio_dialog) {
		_audio_dialog->set_film (_film);
	}
	
	film_changed (Film::NAME);
	film_changed (Film::USE_DCI_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::LOOP);
	film_changed (Film::TRUST_CONTENT_HEADERS);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::FORMAT);
	film_changed (Film::CROP);
	film_changed (Film::FILTERS);
	film_changed (Film::SCALER);
	film_changed (Film::TRIM_START);
	film_changed (Film::TRIM_END);
	film_changed (Film::AB);
	film_changed (Film::TRIM_TYPE);
	film_changed (Film::AUDIO_GAIN);
	film_changed (Film::AUDIO_DELAY);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
	film_changed (Film::COLOUR_LUT);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::DCI_METADATA);
	film_changed (Film::DCP_FRAME_RATE);
	film_changed (Film::AUDIO_MAPPING);

	film_content_changed (boost::shared_ptr<Content> (), FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (boost::shared_ptr<Content> (), FFmpegContentProperty::SUBTITLE_STREAM);
	film_content_changed (boost::shared_ptr<Content> (), FFmpegContentProperty::AUDIO_STREAMS);
	film_content_changed (boost::shared_ptr<Content> (), FFmpegContentProperty::AUDIO_STREAM);

	setup_content_information ();
}

/** Updates the sensitivity of lots of widgets to a given value.
 *  @param s true to make sensitive, false to make insensitive.
 */
void
FilmEditor::set_things_sensitive (bool s)
{
	_generally_sensitive = s;
	
	_name->Enable (s);
	_use_dci_name->Enable (s);
	_edit_dci_button->Enable (s);
	_format->Enable (s);
	_content->Enable (s);
	_trust_content_headers->Enable (s);
	_content->Enable (s);
	_left_crop->Enable (s);
	_right_crop->Enable (s);
	_top_crop->Enable (s);
	_bottom_crop->Enable (s);
	_filters_button->Enable (s);
	_scaler->Enable (s);
	_dcp_content_type->Enable (s);
	_best_dcp_frame_rate->Enable (s);
	_dcp_frame_rate->Enable (s);
	_trim_start->Enable (s);
	_trim_end->Enable (s);
	_ab->Enable (s);
	_trim_type->Enable (s);
	_colour_lut->Enable (s);
	_j2k_bandwidth->Enable (s);
	_audio_gain->Enable (s);
	_audio_gain_calculate_button->Enable (s);
	_show_audio->Enable (s);
	_audio_delay->Enable (s);

	setup_subtitle_control_sensitivity ();
	setup_show_audio_sensitivity ();
	setup_content_button_sensitivity ();
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked (wxCommandEvent &)
{
	FilterDialog* d = new FilterDialog (this, _film->filters());
	d->ActiveChanged.connect (bind (&Film::set_filters, _film, _1));
	d->ShowModal ();
	d->Destroy ();
}

/** Called when the scaler widget has been changed */
void
FilmEditor::scaler_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	int const n = _scaler->GetSelection ();
	if (n >= 0) {
		_film->set_scaler (Scaler::from_index (n));
	}
}

void
FilmEditor::audio_gain_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_audio_gain (_audio_gain->GetValue ());
}

void
FilmEditor::audio_delay_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_audio_delay (_audio_delay->GetValue ());
}

void
FilmEditor::setup_notebook_size ()
{
	_notebook->InvalidateBestSize ();
	
	_film_sizer->Layout ();
	_film_sizer->SetSizeHints (_film_panel);
	_video_sizer->Layout ();
	_video_sizer->SetSizeHints (_video_panel);
	_audio_sizer->Layout ();
	_audio_sizer->SetSizeHints (_audio_panel);
	_subtitle_sizer->Layout ();
	_subtitle_sizer->SetSizeHints (_subtitle_panel);

	_notebook->Fit ();
	Fit ();
}

void
FilmEditor::trim_start_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_trim_start (_trim_start->GetValue ());
}

void
FilmEditor::trim_end_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_trim_end (_trim_end->GetValue ());
}

void
FilmEditor::audio_gain_calculate_button_clicked (wxCommandEvent &)
{
	GainCalculatorDialog* d = new GainCalculatorDialog (this);
	d->ShowModal ();

	if (d->wanted_fader() == 0 || d->actual_fader() == 0) {
		d->Destroy ();
		return;
	}
	
	_audio_gain->SetValue (
		Config::instance()->sound_processor()->db_for_fader_change (
			d->wanted_fader (),
			d->actual_fader ()
			)
		);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	wxCommandEvent dummy;
	audio_gain_changed (dummy);
	
	d->Destroy ();
}

void
FilmEditor::setup_formats ()
{
	_formats = Format::all ();

	_format->Clear ();
	for (vector<Format const *>::iterator i = _formats.begin(); i != _formats.end(); ++i) {
		_format->Append (std_to_wx ((*i)->name ()));
	}

	_film_sizer->Layout ();
}

void
FilmEditor::with_subtitles_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_with_subtitles (_with_subtitles->GetValue ());
}

void
FilmEditor::setup_subtitle_control_sensitivity ()
{
	bool h = false;
	if (_generally_sensitive && _film) {
		h = !_film->has_subtitles ();
	}
	
	_with_subtitles->Enable (h);

	bool j = false;
	if (_film) {
		j = _film->with_subtitles ();
	}
	
	_subtitle_offset->Enable (j);
	_subtitle_scale->Enable (j);
}

void
FilmEditor::use_dci_name_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_use_dci_name (_use_dci_name->GetValue ());
}

void
FilmEditor::edit_dci_button_clicked (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	DCIMetadataDialog* d = new DCIMetadataDialog (this, _film->dci_metadata ());
	d->ShowModal ();
	_film->set_dci_metadata (d->dci_metadata ());
	d->Destroy ();
}

void
FilmEditor::active_jobs_changed (bool a)
{
	set_things_sensitive (!a);
}

void
FilmEditor::setup_dcp_name ()
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
FilmEditor::show_audio_clicked (wxCommandEvent &)
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}
	
	_audio_dialog = new AudioDialog (this);
	_audio_dialog->Show ();
	_audio_dialog->set_film (_film);
}

void
FilmEditor::best_dcp_frame_rate_clicked (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_dcp_frame_rate (best_dcp_frame_rate (_film->video_frame_rate ()));
}

void
FilmEditor::setup_show_audio_sensitivity ()
{
	_show_audio->Enable (_film && _film->has_audio ());
}

void
FilmEditor::setup_content ()
{
	string selected_summary;
	int const s = _content->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s != -1) {
		selected_summary = wx_to_std (_content->GetItemText (s));
	}
	
	_content->DeleteAllItems ();

	ContentList content = _film->content ();
	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		int const t = _content->GetItemCount ();
		_content->InsertItem (t, std_to_wx ((*i)->summary ()));
		if ((*i)->summary() == selected_summary) {
			_content->SetItemState (t, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}
	}

	if (selected_summary.empty () && !content.empty ()) {
		/* Select the first item of content if non was selected before */
		_content->SetItemState (0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}

	setup_playlist_description ();
}

void
FilmEditor::content_add_clicked (wxCommandEvent &)
{
	wxFileDialog* d = new wxFileDialog (this, _("Choose a file or files"), wxT (""), wxT (""), wxT ("*.*"), wxFD_MULTIPLE);
	int const r = d->ShowModal ();
	d->Destroy ();

	if (r != wxID_OK) {
		return;
	}

	wxArrayString paths;
	d->GetPaths (paths);

	for (unsigned int i = 0; i < paths.GetCount(); ++i) {
		boost::filesystem::path p (wx_to_std (paths[i]));

		if (ImageMagickContent::valid_file (p)) {
			_film->add_content (shared_ptr<ImageMagickContent> (new ImageMagickContent (p)));
		} else if (SndfileContent::valid_file (p)) {
			_film->add_content (shared_ptr<SndfileContent> (new SndfileContent (p)));
		} else {
			_film->add_content (shared_ptr<FFmpegContent> (new FFmpegContent (p)));
		}
	}
}

void
FilmEditor::content_remove_clicked (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (c) {
		_film->remove_content (c);
	}
}

void
FilmEditor::content_activated (wxListEvent& ev)
{
	ContentList c = _film->content ();
	assert (ev.GetIndex() >= 0 && size_t (ev.GetIndex()) < c.size ());

	edit_content (c[ev.GetIndex()]);
}

void
FilmEditor::content_edit_clicked (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}

	edit_content (c);
}

void
FilmEditor::edit_content (shared_ptr<Content> c)
{
	shared_ptr<ImageMagickContent> im = dynamic_pointer_cast<ImageMagickContent> (c);
	if (im) {
		ImageMagickContentDialog* d = new ImageMagickContentDialog (this, im);
		d->ShowModal ();
		d->Destroy ();
	}

	shared_ptr<FFmpegContent> ff = dynamic_pointer_cast<FFmpegContent> (c);
	if (ff) {
		FFmpegContentDialog* d = new FFmpegContentDialog (this, ff);
		d->ShowModal ();
		d->Destroy ();
	}
}

void
FilmEditor::content_earlier_clicked (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (c) {
		_film->move_content_earlier (c);
	}
}

void
FilmEditor::content_later_clicked (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (c) {
		_film->move_content_later (c);
	}
}

void
FilmEditor::content_selection_changed (wxListEvent &)
{
        setup_content_button_sensitivity ();
	setup_content_information ();
}

void
FilmEditor::setup_content_information ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		_content_information->SetValue (wxT (""));
		return;
	}

	_content_information->SetValue (std_to_wx (c->information ()));
}

void
FilmEditor::setup_content_button_sensitivity ()
{
        _content_add->Enable (_generally_sensitive);

	shared_ptr<Content> selection = selected_content ();

        _content_edit->Enable (
		selection && _generally_sensitive &&
		(dynamic_pointer_cast<ImageMagickContent> (selection) || dynamic_pointer_cast<FFmpegContent> (selection))
		);
	
        _content_remove->Enable (selection && _generally_sensitive);
        _content_earlier->Enable (selection && _generally_sensitive);
        _content_later->Enable (selection && _generally_sensitive);
}

shared_ptr<Content>
FilmEditor::selected_content ()
{
	int const s = _content->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s == -1) {
		return shared_ptr<Content> ();
	}

	ContentList c = _film->content ();
	if (s < 0 || size_t (s) >= c.size ()) {
		return shared_ptr<Content> ();
	}
	
	return c[s];
}

void
FilmEditor::setup_scaling_description ()
{
	wxString d;

	int lines = 0;

	if (_film->video_size().width && _film->video_size().height) {
		d << wxString::Format (
			_("Original video is %dx%d (%.2f:1)\n"),
			_film->video_size().width, _film->video_size().height,
			float (_film->video_size().width) / _film->video_size().height
			);
		++lines;
	}

	Crop const crop = _film->crop ();
	if (crop.left || crop.right || crop.top || crop.bottom) {
		libdcp::Size const cropped = _film->cropped_size (_film->video_size ());
		d << wxString::Format (
			_("Cropped to %dx%d (%.2f:1)\n"),
			cropped.width, cropped.height,
			float (cropped.width) / cropped.height
			);
		++lines;
	}

	Format const * format = _film->format ();
	if (format) {
		int const padding = format->dcp_padding (_film);
		libdcp::Size scaled = format->dcp_size ();
		scaled.width -= padding * 2;
		d << wxString::Format (
			_("Scaled to %dx%d (%.2f:1)\n"),
			scaled.width, scaled.height,
			float (scaled.width) / scaled.height
			);
		++lines;

		if (padding) {
			d << wxString::Format (
				_("Padded with black to %dx%d (%.2f:1)\n"),
				format->dcp_size().width, format->dcp_size().height,
				float (format->dcp_size().width) / format->dcp_size().height
				);
			++lines;
		}
	}

	for (int i = lines; i < 4; ++i) {
		d << wxT ("\n ");
	}

	_scaling_description->SetLabel (d);
}

void
FilmEditor::trim_type_changed (wxCommandEvent &)
{
	_film->set_trim_type (_trim_type->GetSelection () == 0 ? Film::CPL : Film::ENCODE);
}

void
FilmEditor::loop_content_toggled (wxCommandEvent &)
{
	if (_loop_content->GetValue ()) {
		_film->set_loop (_loop_count->GetValue ());
	} else {
		_film->set_loop (1);
	}
		
	setup_loop_sensitivity ();
}

void
FilmEditor::loop_count_changed (wxCommandEvent &)
{
	_film->set_loop (_loop_count->GetValue ());
}

void
FilmEditor::setup_loop_sensitivity ()
{
	_loop_count->Enable (_loop_content->GetValue ());
}

void
FilmEditor::setup_playlist_description ()
{
	if (!_film) {
		_playlist_description->SetLabel (wxT (""));
		return;
	}

	_playlist_description->SetLabel (std_to_wx (_film->playlist_description ()));
}
