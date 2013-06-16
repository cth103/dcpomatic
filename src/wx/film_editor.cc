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
#include "lib/film.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/ab_transcode_job.h"
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/ratio.h"
#include "lib/config.h"
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
#include "timeline_dialog.h"
#include "audio_mapping_view.h"
#include "timecode.h"

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
	, _timeline_dialog (0)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);

	_main_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_main_notebook, 1);

	make_content_panel ();
	_main_notebook->AddPage (_content_panel, _("Content"), true);
	make_dcp_panel ();
	_main_notebook->AddPage (_dcp_panel, _("DCP"), false);
	
	setup_ratios ();

	set_film (f);
	connect_to_widgets ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmEditor::active_jobs_changed, this, _1)
		);
	
	SetSizerAndFit (s);
}

void
FilmEditor::make_dcp_panel ()
{
	_dcp_panel = new wxPanel (_main_notebook);
	_dcp_sizer = new wxBoxSizer (wxVERTICAL);
	_dcp_panel->SetSizer (_dcp_sizer);

	wxGridBagSizer* grid = new wxGridBagSizer (4, 4);
	_dcp_sizer->Add (grid, 0, wxEXPAND | wxALL, 8);

	int r = 0;
	
	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Name"), wxGBPosition (r, 0));
	_name = new wxTextCtrl (_dcp_panel, wxID_ANY);
	grid->Add (_name, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND);
	++r;
	
	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("DCP Name"), wxGBPosition (r, 0));
	_dcp_name = new wxStaticText (_dcp_panel, wxID_ANY, wxT (""));
	grid->Add (_dcp_name, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	_use_dci_name = new wxCheckBox (_dcp_panel, wxID_ANY, _("Use DCI name"));
	grid->Add (_use_dci_name, wxGBPosition (r, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	_edit_dci_button = new wxButton (_dcp_panel, wxID_ANY, _("Details..."));
	grid->Add (_edit_dci_button, wxGBPosition (r, 1), wxDefaultSpan);
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Container"), wxGBPosition (r, 0));
	_container = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_container, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Content Type"), wxGBPosition (r, 0));
	_dcp_content_type = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_dcp_content_type, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _dcp_panel, _("DCP Frame Rate"), wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_dcp_frame_rate = new wxChoice (_dcp_panel, wxID_ANY);
		s->Add (_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		_best_dcp_frame_rate = new wxButton (_dcp_panel, wxID_ANY, _("Use best"));
		s->Add (_best_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _dcp_panel, _("JPEG2000 bandwidth"), wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (_dcp_panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, _dcp_panel, _("MBps"));
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Scaler"), wxGBPosition (r, 0));
	_scaler = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_scaler, wxGBPosition (r, 1));
	++r;

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler->Append (std_to_wx ((*i)->name()));
	}

	vector<Ratio const *> const ratio = Ratio::all ();
	for (vector<Ratio const *>::const_iterator i = ratio.begin(); i != ratio.end(); ++i) {
		_container->Append (std_to_wx ((*i)->nickname ()));
	}

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
	}

	list<int> const dfr = Config::instance()->allowed_dcp_frame_rates ();
	for (list<int>::const_iterator i = dfr.begin(); i != dfr.end(); ++i) {
		_dcp_frame_rate->Append (std_to_wx (boost::lexical_cast<string> (*i)));
	}

	_j2k_bandwidth->SetRange (50, 250);
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Connect                   (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED,         wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_use_dci_name->Connect           (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::use_dci_name_toggled), 0, this);
	_edit_dci_button->Connect        (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::edit_dci_button_clicked), 0, this);
	_container->Connect              (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::container_changed), 0, this);
	_ratio->Connect                  (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::ratio_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_SELECTED,   wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content_add->Connect            (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_add_clicked), 0, this);
	_content_remove->Connect         (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_remove_clicked), 0, this);
	_content_timeline->Connect       (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,       wxCommandEventHandler (FilmEditor::content_timeline_clicked), 0, this);
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
	_audio_stream->Connect           (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::audio_stream_changed), 0, this);
	_subtitle_stream->Connect        (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::subtitle_stream_changed), 0, this);
	_audio_mapping->Changed.connect  (boost::bind (&FilmEditor::audio_mapping_changed, this, _1));
	_start->Changed.connect          (boost::bind (&FilmEditor::start_changed, this));
	_length->Changed.connect         (boost::bind (&FilmEditor::length_changed, this));
}

void
FilmEditor::make_video_panel ()
{
	_video_panel = new wxPanel (_content_notebook);
	wxBoxSizer* video_sizer = new wxBoxSizer (wxVERTICAL);
	_video_panel->SetSizer (video_sizer);
	
	wxGridBagSizer* grid = new wxGridBagSizer (4, 4);
	video_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;
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

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Scale to"), wxGBPosition (r, 0));
	_ratio = new wxChoice (_video_panel, wxID_ANY);
	grid->Add (_ratio, wxGBPosition (r, 1));
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

	add_label_to_grid_bag_sizer (grid, _video_panel, _("Colour look-up table"), wxGBPosition (r, 0));
	_colour_lut = new wxChoice (_video_panel, wxID_ANY);
	for (int i = 0; i < 2; ++i) {
		_colour_lut->Append (std_to_wx (colour_lut_index_to_name (i)));
	}
	_colour_lut->SetSelection (0);
	grid->Add (_colour_lut, wxGBPosition (r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	_left_crop->SetRange (0, 1024);
	_top_crop->SetRange (0, 1024);
	_right_crop->SetRange (0, 1024);
	_bottom_crop->SetRange (0, 1024);
}

void
FilmEditor::make_content_panel ()
{
	_content_panel = new wxPanel (_main_notebook);
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
                b->Add (_content_add, 1, wxEXPAND | wxLEFT | wxRIGHT);
                _content_remove = new wxButton (_content_panel, wxID_ANY, _("Remove"));
                b->Add (_content_remove, 1, wxEXPAND | wxLEFT | wxRIGHT);
		_content_timeline = new wxButton (_content_panel, wxID_ANY, _("Timeline..."));
		b->Add (_content_timeline, 1, wxEXPAND | wxLEFT | wxRIGHT);

                s->Add (b, 0, wxALL, 4);

                _content_sizer->Add (s, 0.75, wxEXPAND | wxALL, 6);
        }

	wxBoxSizer* h = new wxBoxSizer (wxHORIZONTAL);
	_loop_content = new wxCheckBox (_content_panel, wxID_ANY, _("Loop everything"));
	h->Add (_loop_content, 0, wxALL, 6);
	_loop_count = new wxSpinCtrl (_content_panel, wxID_ANY);
	h->Add (_loop_count, 0, wxALL, 6);
	add_label_to_sizer (h, _content_panel, _("times"));
	_content_sizer->Add (h, 0, wxALL, 6);

	_content_notebook = new wxNotebook (_content_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
	_content_sizer->Add (_content_notebook, 1, wxEXPAND | wxTOP, 6);

	make_video_panel ();
	_content_notebook->AddPage (_video_panel, _("Video"), false);
	make_audio_panel ();
	_content_notebook->AddPage (_audio_panel, _("Audio"), false);
	make_subtitle_panel ();
	_content_notebook->AddPage (_subtitle_panel, _("Subtitles"), false);
	make_timing_panel ();
	_content_notebook->AddPage (_timing_panel, _("Timing"), false);

	_loop_count->SetRange (2, 1024);
}

void
FilmEditor::make_audio_panel ()
{
	_audio_panel = new wxPanel (_content_notebook);
	wxBoxSizer* audio_sizer = new wxBoxSizer (wxVERTICAL);
	_audio_panel->SetSizer (audio_sizer);
	
	wxFlexGridSizer* grid = new wxFlexGridSizer (3, 4, 4);
	audio_sizer->Add (grid, 0, wxALL, 8);

	_show_audio = new wxButton (_audio_panel, wxID_ANY, _("Show Audio..."));
	grid->Add (_show_audio, 1);
	grid->AddSpacer (0);
	grid->AddSpacer (0);

	add_label_to_sizer (grid, _audio_panel, _("Audio Gain"));
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_gain = new wxSpinCtrl (_audio_panel);
		s->Add (_audio_gain, 1);
		add_label_to_sizer (s, _audio_panel, _("dB"));
		grid->Add (s, 1);
	}
	
	_audio_gain_calculate_button = new wxButton (_audio_panel, wxID_ANY, _("Calculate..."));
	grid->Add (_audio_gain_calculate_button);

	add_label_to_sizer (grid, _audio_panel, _("Audio Delay"));
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_delay = new wxSpinCtrl (_audio_panel);
		s->Add (_audio_delay, 1);
		/// TRANSLATORS: this is an abbreviation for milliseconds, the unit of time
		add_label_to_sizer (s, _audio_panel, _("ms"));
		grid->Add (s);
	}

	grid->AddSpacer (0);

	add_label_to_sizer (grid, _audio_panel, _("Audio Stream"));
	_audio_stream = new wxChoice (_audio_panel, wxID_ANY);
	grid->Add (_audio_stream, 1);
	_audio_description = new wxStaticText (_audio_panel, wxID_ANY, wxT (""));
	grid->AddSpacer (0);
	
	grid->Add (_audio_description, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
	grid->AddSpacer (0);
	grid->AddSpacer (0);
	
	_audio_mapping = new AudioMappingView (_audio_panel);
	audio_sizer->Add (_audio_mapping, 1, wxEXPAND | wxALL, 6);

	_audio_gain->SetRange (-60, 60);
	_audio_delay->SetRange (-1000, 1000);
}

void
FilmEditor::make_subtitle_panel ()
{
	_subtitle_panel = new wxPanel (_content_notebook);
	wxBoxSizer* subtitle_sizer = new wxBoxSizer (wxVERTICAL);
	_subtitle_panel->SetSizer (subtitle_sizer);
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	subtitle_sizer->Add (grid, 0, wxALL, 8);

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

	add_label_to_sizer (grid, _subtitle_panel, _("Subtitle Stream"));
	_subtitle_stream = new wxChoice (_subtitle_panel, wxID_ANY);
	grid->Add (_subtitle_stream, 1, wxEXPAND | wxALL, 6);
	grid->AddSpacer (0);
	
	_subtitle_offset->SetRange (-1024, 1024);
	_subtitle_scale->SetRange (1, 1000);
}

void
FilmEditor::make_timing_panel ()
{
	_timing_panel = new wxPanel (_content_notebook);
	wxBoxSizer* timing_sizer = new wxBoxSizer (wxVERTICAL);
	_timing_panel->SetSizer (timing_sizer);
	wxFlexGridSizer* grid = new wxFlexGridSizer (2, 4, 4);
	timing_sizer->Add (grid, 0, wxALL, 8);

	add_label_to_sizer (grid, _timing_panel, _("Start time"));
	_start = new Timecode (_timing_panel);
	grid->Add (_start);
	add_label_to_sizer (grid, _timing_panel, _("Length"));
	_length = new Timecode (_timing_panel);
	grid->Add (_length);
}


/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed (wxCommandEvent &)
{
	shared_ptr<VideoContent> c = selected_video_content ();
	if (!c) {
		return;
	}

	c->set_left_crop (_left_crop->GetValue ());
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed (wxCommandEvent &)
{
	shared_ptr<VideoContent> c = selected_video_content ();
	if (!c) {
		return;
	}

	c->set_right_crop (_right_crop->GetValue ());
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed (wxCommandEvent &)
{
	shared_ptr<VideoContent> c = selected_video_content ();
	if (!c) {
		return;
	}

	c->set_top_crop (_top_crop->GetValue ());
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed (wxCommandEvent &)
{
	shared_ptr<VideoContent> c = selected_video_content ();
	if (!c) {
		return;
	}

	c->set_bottom_crop (_bottom_crop->GetValue ());
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

	_film->set_dcp_video_frame_rate (
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
		setup_subtitle_control_sensitivity ();
		setup_show_audio_sensitivity ();
		break;
	case Film::LOOP:
		checked_set (_loop_content, _film->loop() > 1);
		checked_set (_loop_count, _film->loop());
		setup_loop_sensitivity ();
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
	case Film::SCALER:
		checked_set (_scaler, Scaler::as_index (_film->scaler ()));
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
	case Film::DCP_VIDEO_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _dcp_frame_rate->GetCount(); ++i) {
			if (wx_to_std (_dcp_frame_rate->GetString(i)) == boost::lexical_cast<string> (_film->dcp_video_frame_rate())) {
				checked_set (_dcp_frame_rate, i);
				done = true;
				break;
			}
		}

		if (!done) {
			checked_set (_dcp_frame_rate, -1);
		}

		_best_dcp_frame_rate->Enable (_film->best_dcp_video_frame_rate () != _film->dcp_video_frame_rate ());
		break;
	}
	}
}

void
FilmEditor::film_content_changed (weak_ptr<Content> weak_content, int property)
{
	if (!_film) {
		/* We call this method ourselves (as well as using it as a signal handler)
		   so _film can be 0.
		*/
		return;
	}

	shared_ptr<Content> content = weak_content.lock ();
	shared_ptr<VideoContent> video_content;
	shared_ptr<AudioContent> audio_content;
	shared_ptr<FFmpegContent> ffmpeg_content;
	if (content) {
		video_content = dynamic_pointer_cast<VideoContent> (content);
		audio_content = dynamic_pointer_cast<AudioContent> (content);
		ffmpeg_content = dynamic_pointer_cast<FFmpegContent> (content);
	}

	/* We can't use case {} here */
	
	if (property == ContentProperty::START) {
		if (content) {
			_start->set (content->start (), _film->dcp_video_frame_rate ());
		} else {
			_start->set (0, 24);
		}
	} else if (property == ContentProperty::LENGTH) {
		if (content) {
			_length->set (content->length (), _film->dcp_video_frame_rate ());
		} else {
			_length->set (0, 24);
		}
	} else if (property == VideoContentProperty::VIDEO_CROP) {
		checked_set (_left_crop,   video_content ? video_content->crop().left :   0);
		checked_set (_right_crop,  video_content ? video_content->crop().right :  0);
		checked_set (_top_crop,    video_content ? video_content->crop().top :    0);
		checked_set (_bottom_crop, video_content ? video_content->crop().bottom : 0);
		setup_scaling_description ();
	} else if (property == VideoContentProperty::VIDEO_RATIO) {
		if (video_content) {
			int n = 0;
			vector<Ratio const *> ratios = Ratio::all ();
			vector<Ratio const *>::iterator i = ratios.begin ();
			while (i != ratios.end() && *i != video_content->ratio()) {
				++i;
				++n;
			}

			if (i == ratios.end()) {
				checked_set (_ratio, -1);
			} else {
				checked_set (_ratio, n);
			}
		} else {
			checked_set (_ratio, -1);
		}
	} else if (property == AudioContentProperty::AUDIO_GAIN) {
		checked_set (_audio_gain, audio_content ? audio_content->audio_gain() : 0);
	} else if (property == AudioContentProperty::AUDIO_DELAY) {
		checked_set (_audio_delay, audio_content ? audio_content->audio_delay() : 0);
	} else if (property == AudioContentProperty::AUDIO_MAPPING) {
		_audio_mapping->set (audio_content ? audio_content->audio_mapping () : AudioMapping ());
	} else if (property == FFmpegContentProperty::SUBTITLE_STREAMS) {
		_subtitle_stream->Clear ();
		if (ffmpeg_content) {
			vector<shared_ptr<FFmpegSubtitleStream> > s = ffmpeg_content->subtitle_streams ();
			if (s.empty ()) {
				_subtitle_stream->Enable (false);
			}
			for (vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = s.begin(); i != s.end(); ++i) {
				_subtitle_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx (lexical_cast<string> ((*i)->id))));
			}
			
			if (ffmpeg_content->subtitle_stream()) {
				checked_set (_subtitle_stream, lexical_cast<string> (ffmpeg_content->subtitle_stream()->id));
			} else {
				_subtitle_stream->SetSelection (wxNOT_FOUND);
			}
		}
		setup_subtitle_control_sensitivity ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAMS) {
		_audio_stream->Clear ();
		if (ffmpeg_content) {
			vector<shared_ptr<FFmpegAudioStream> > a = ffmpeg_content->audio_streams ();
			for (vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin(); i != a.end(); ++i) {
				_audio_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx (lexical_cast<string> ((*i)->id))));
			}
			
			if (ffmpeg_content->audio_stream()) {
				checked_set (_audio_stream, lexical_cast<string> (ffmpeg_content->audio_stream()->id));
			}
		}
		setup_show_audio_sensitivity ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_dcp_name ();
		setup_show_audio_sensitivity ();
	} else if (property == FFmpegContentProperty::FILTERS) {
		if (ffmpeg_content) {
			pair<string, string> p = Filter::ffmpeg_strings (ffmpeg_content->filters ());
			if (p.first.empty () && p.second.empty ()) {
				_filters->SetLabel (_("None"));
			} else {
				string const b = p.first + " " + p.second;
				_filters->SetLabel (std_to_wx (b));
			}
			_dcp_sizer->Layout ();
		}
	}
}

void
FilmEditor::setup_container ()
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
	setup_scaling_description ();
}	

/** Called when the container widget has been changed */
void
FilmEditor::container_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	int const n = _container->GetSelection ();
	if (n >= 0) {
		vector<Ratio const *> ratios = Ratio::all ();
		assert (n < int (ratios.size()));
		_film->set_container (ratios[n]);
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
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::SCALER);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
	film_changed (Film::COLOUR_LUT);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::DCI_METADATA);
	film_changed (Film::DCP_VIDEO_FRAME_RATE);

	shared_ptr<Content> s = selected_content ();
	film_content_changed (s, ContentProperty::START);
	film_content_changed (s, ContentProperty::LENGTH);
	film_content_changed (s, VideoContentProperty::VIDEO_CROP);
	film_content_changed (s, VideoContentProperty::VIDEO_RATIO);
	film_content_changed (s, AudioContentProperty::AUDIO_GAIN);
	film_content_changed (s, AudioContentProperty::AUDIO_DELAY);
	film_content_changed (s, AudioContentProperty::AUDIO_MAPPING);
	film_content_changed (s, FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (s, FFmpegContentProperty::SUBTITLE_STREAM);
	film_content_changed (s, FFmpegContentProperty::AUDIO_STREAMS);
	film_content_changed (s, FFmpegContentProperty::AUDIO_STREAM);
	film_content_changed (s, FFmpegContentProperty::FILTERS);
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
	_ratio->Enable (s);
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
	_colour_lut->Enable (s);
	_j2k_bandwidth->Enable (s);
	_audio_gain->Enable (s);
	_audio_gain_calculate_button->Enable (s);
	_show_audio->Enable (s);
	_audio_delay->Enable (s);
	_container->Enable (s);
	_loop_content->Enable (s);
	_loop_count->Enable (s);

	setup_subtitle_control_sensitivity ();
	setup_show_audio_sensitivity ();
	setup_content_sensitivity ();
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}
	
	FilterDialog* d = new FilterDialog (this, fc->filters());
	d->ActiveChanged.connect (bind (&FFmpegContent::set_filters, fc, _1));
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
	shared_ptr<AudioContent> ac = selected_audio_content ();
	if (!ac) {
		return;
	}

	ac->set_audio_gain (_audio_gain->GetValue ());
}

void
FilmEditor::audio_delay_changed (wxCommandEvent &)
{
	shared_ptr<AudioContent> ac = selected_audio_content ();
	if (!ac) {
		return;
	}

	ac->set_audio_delay (_audio_delay->GetValue ());
}

void
FilmEditor::setup_main_notebook_size ()
{
	_main_notebook->InvalidateBestSize ();

	_content_sizer->Layout ();
	_content_sizer->SetSizeHints (_content_panel);
	_dcp_sizer->Layout ();
	_dcp_sizer->SetSizeHints (_dcp_panel);

	_main_notebook->Fit ();
	Fit ();
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
FilmEditor::setup_ratios ()
{
	_ratios = Ratio::all ();

	_ratio->Clear ();
	for (vector<Ratio const *>::iterator i = _ratios.begin(); i != _ratios.end(); ++i) {
		_ratio->Append (std_to_wx ((*i)->nickname ()));
	}

	_dcp_sizer->Layout ();
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
		h = _film->has_subtitles ();
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
	
	_film->set_dcp_video_frame_rate (_film->best_dcp_video_frame_rate ());
}

void
FilmEditor::setup_show_audio_sensitivity ()
{
	_show_audio->Enable (_film);
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

	Playlist::ContentList content = _film->content ();
	for (Playlist::ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		int const t = _content->GetItemCount ();
		_content->InsertItem (t, std_to_wx ((*i)->summary ()));
		if ((*i)->summary() == selected_summary) {
			_content->SetItemState (t, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}
	}

	if (selected_summary.empty () && !content.empty ()) {
		/* Select the item of content if none was selected before */
		_content->SetItemState (0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
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

		shared_ptr<Content> c;

		if (ImageMagickContent::valid_file (p)) {
			c.reset (new ImageMagickContent (_film, p));
		} else if (SndfileContent::valid_file (p)) {
			c.reset (new SndfileContent (_film, p));
		} else {
			c.reset (new FFmpegContent (_film, p));
		}

		_film->examine_and_add_content (c);
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
FilmEditor::content_selection_changed (wxListEvent &)
{
        setup_content_sensitivity ();
	shared_ptr<Content> s = selected_content ();
	film_content_changed (s, ContentProperty::START);
	film_content_changed (s, ContentProperty::LENGTH);
	film_content_changed (s, VideoContentProperty::VIDEO_CROP);
	film_content_changed (s, VideoContentProperty::VIDEO_RATIO);
	film_content_changed (s, AudioContentProperty::AUDIO_GAIN);
	film_content_changed (s, AudioContentProperty::AUDIO_DELAY);
	film_content_changed (s, AudioContentProperty::AUDIO_MAPPING);
	film_content_changed (s, FFmpegContentProperty::AUDIO_STREAM);
	film_content_changed (s, FFmpegContentProperty::AUDIO_STREAMS);
	film_content_changed (s, FFmpegContentProperty::SUBTITLE_STREAM);
}

void
FilmEditor::setup_content_sensitivity ()
{
        _content_add->Enable (_generally_sensitive);

	shared_ptr<Content> selection = selected_content ();

        _content_remove->Enable (selection && _generally_sensitive);
	_content_timeline->Enable (_generally_sensitive);

	_video_panel->Enable    (selection && dynamic_pointer_cast<VideoContent>  (selection) && _generally_sensitive);
	_audio_panel->Enable    (selection && dynamic_pointer_cast<AudioContent>  (selection) && _generally_sensitive);
	_subtitle_panel->Enable (selection && dynamic_pointer_cast<FFmpegContent> (selection) && _generally_sensitive);
	_timing_panel->Enable   (selection && _generally_sensitive);
}

shared_ptr<Content>
FilmEditor::selected_content ()
{
	int const s = _content->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s == -1) {
		return shared_ptr<Content> ();
	}

	Playlist::ContentList c = _film->content ();
	if (s < 0 || size_t (s) >= c.size ()) {
		return shared_ptr<Content> ();
	}
	
	return c[s];
}

shared_ptr<VideoContent>
FilmEditor::selected_video_content ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return shared_ptr<VideoContent> ();
	}

	return dynamic_pointer_cast<VideoContent> (c);
}

shared_ptr<AudioContent>
FilmEditor::selected_audio_content ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return shared_ptr<AudioContent> ();
	}

	return dynamic_pointer_cast<AudioContent> (c);
}

void
FilmEditor::setup_scaling_description ()
{
	wxString d;

#if 0	
XXX
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

#endif	
	_scaling_description->SetLabel (d);
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
FilmEditor::content_timeline_clicked (wxCommandEvent &)
{
	if (_timeline_dialog) {
		_timeline_dialog->Destroy ();
		_timeline_dialog = 0;
	}
	
	_timeline_dialog = new TimelineDialog (this, _film);
	_timeline_dialog->Show ();
}

void
FilmEditor::audio_stream_changed (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}
	
	vector<shared_ptr<FFmpegAudioStream> > a = fc->audio_streams ();
	vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin ();
	string const s = string_client_data (_audio_stream->GetClientObject (_audio_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> ((*i)->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		fc->set_audio_stream (*i);
	}

	if (!fc->audio_stream ()) {
		_audio_description->SetLabel (wxT (""));
	} else {
		wxString s;
		if (fc->audio_channels() == 1) {
			s << _("1 channel");
		} else {
			s << fc->audio_channels() << wxT (" ") << _("channels");
		}
		s << wxT (", ") << fc->content_audio_frame_rate() << _("Hz");
		_audio_description->SetLabel (s);
	}
}



void
FilmEditor::subtitle_stream_changed (wxCommandEvent &)
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}
	
	vector<shared_ptr<FFmpegSubtitleStream> > a = fc->subtitle_streams ();
	vector<shared_ptr<FFmpegSubtitleStream> >::iterator i = a.begin ();
	string const s = string_client_data (_subtitle_stream->GetClientObject (_subtitle_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> ((*i)->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		fc->set_subtitle_stream (*i);
	}
}

void
FilmEditor::audio_mapping_changed (AudioMapping m)
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (c);
	if (!ac) {
		return;
	}

	ac->set_audio_mapping (m);
}

void
FilmEditor::start_changed ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}

	c->set_start (_start->get (_film->dcp_video_frame_rate ()));
}

void
FilmEditor::length_changed ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<ImageMagickContent> ic = dynamic_pointer_cast<ImageMagickContent> (c);
	if (ic) {
		ic->set_video_length (_length->get(_film->dcp_video_frame_rate()) * ic->video_frame_rate() / TIME_HZ);
	}
}

void
FilmEditor::set_selection (weak_ptr<Content> wc)
{
	Playlist::ContentList content = _film->content ();
	for (size_t i = 0; i < content.size(); ++i) {
		if (content[i] == wc.lock ()) {
			_content->SetItemState (i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		} else {
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		}
	}
}

void
FilmEditor::ratio_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (c);
	if (!vc) {
		return;
	}
	
	int const n = _ratio->GetSelection ();
	if (n >= 0) {
		vector<Ratio const *> ratios = Ratio::all ();
		assert (n < int (ratios.size()));
		vc->set_ratio (ratios[n]);
	}
}
