/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/ratio.h"
#include "lib/config.h"
#include "lib/still_image_content.h"
#include "lib/sndfile_content.h"
#include "lib/dcp_content_type.h"
#include "lib/sound_processor.h"
#include "lib/scaler.h"
#include "timecode.h"
#include "wx_util.h"
#include "film_editor.h"
#include "dci_metadata_dialog.h"
#include "timeline_dialog.h"
#include "timing_panel.h"
#include "subtitle_panel.h"
#include "audio_panel.h"
#include "video_panel.h"

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
	, _menu (f, this)
	, _generally_sensitive (true)
	, _timeline_dialog (0)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);

	_main_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_main_notebook, 1);

	make_content_panel ();
	_main_notebook->AddPage (_content_panel, _("Content"), true);
	make_dcp_panel ();
	_main_notebook->AddPage (_dcp_panel, _("DCP"), false);
	
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

	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_dcp_sizer->Add (grid, 0, wxEXPAND | wxALL, 8);

	int r = 0;
	
	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Name"), true, wxGBPosition (r, 0));
	_name = new wxTextCtrl (_dcp_panel, wxID_ANY);
	grid->Add (_name, wxGBPosition(r, 1), wxDefaultSpan, wxEXPAND | wxLEFT | wxRIGHT);
	++r;
	
	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("DCP Name"), true, wxGBPosition (r, 0));
	_dcp_name = new wxStaticText (_dcp_panel, wxID_ANY, wxT (""));
	grid->Add (_dcp_name, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	int flags = wxALIGN_CENTER_VERTICAL;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
#endif	

	_use_dci_name = new wxCheckBox (_dcp_panel, wxID_ANY, _("Use DCI name"));
	grid->Add (_use_dci_name, wxGBPosition (r, 0), wxDefaultSpan, flags);
	_edit_dci_button = new wxButton (_dcp_panel, wxID_ANY, _("Details..."));
	grid->Add (_edit_dci_button, wxGBPosition (r, 1), wxDefaultSpan);
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Container"), true, wxGBPosition (r, 0));
	_container = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_container, wxGBPosition (r, 1), wxDefaultSpan, wxEXPAND);
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Content Type"), true, wxGBPosition (r, 0));
	_dcp_content_type = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_dcp_content_type, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Frame Rate"), true, wxGBPosition (r, 0));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_dcp_frame_rate = new wxChoice (_dcp_panel, wxID_ANY);
		s->Add (_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		_best_dcp_frame_rate = new wxButton (_dcp_panel, wxID_ANY, _("Use best"));
		s->Add (_best_dcp_frame_rate, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Audio channels"), true, wxGBPosition (r, 0));
	_dcp_audio_channels = new wxSpinCtrl (_dcp_panel, wxID_ANY);
	grid->Add (_dcp_audio_channels, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Resolution"), true, wxGBPosition (r, 0));
	_dcp_resolution = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_dcp_resolution, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _dcp_panel, _("JPEG2000 bandwidth"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (_dcp_panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, _dcp_panel, _("MBps"), false);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Scaler"), true, wxGBPosition (r, 0));
	_scaler = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_scaler, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
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

	_dcp_audio_channels->SetRange (0, MAX_AUDIO_CHANNELS);
	_j2k_bandwidth->SetRange (50, 250);

	_dcp_resolution->Append (_("2K"));
	_dcp_resolution->Append (_("4K"));
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Connect			 (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED,		wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_use_dci_name->Connect		 (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,	wxCommandEventHandler (FilmEditor::use_dci_name_toggled), 0, this);
	_edit_dci_button->Connect	 (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler (FilmEditor::edit_dci_button_clicked), 0, this);
	_container->Connect		 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,	wxCommandEventHandler (FilmEditor::container_changed), 0, this);
	_content->Connect		 (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_SELECTED,	wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content->Connect		 (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_DESELECTED, wxListEventHandler    (FilmEditor::content_selection_changed), 0, this);
	_content->Connect                (wxID_ANY, wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK,wxListEventHandler    (FilmEditor::content_right_click), 0, this);
	_content_add->Connect		 (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler (FilmEditor::content_add_clicked), 0, this);
	_content_remove->Connect	 (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler (FilmEditor::content_remove_clicked), 0, this);
	_content_timeline->Connect	 (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler (FilmEditor::content_timeline_clicked), 0, this);
	_scaler->Connect		 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,	wxCommandEventHandler (FilmEditor::scaler_changed), 0, this);
	_dcp_content_type->Connect	 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,	wxCommandEventHandler (FilmEditor::dcp_content_type_changed), 0, this);
	_dcp_frame_rate->Connect	 (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,	wxCommandEventHandler (FilmEditor::dcp_frame_rate_changed), 0, this);
	_best_dcp_frame_rate->Connect	 (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED,	wxCommandEventHandler (FilmEditor::best_dcp_frame_rate_clicked), 0, this);
	_dcp_audio_channels->Connect	 (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,	wxCommandEventHandler (FilmEditor::dcp_audio_channels_changed), 0, this);
	_j2k_bandwidth->Connect		 (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED,	wxCommandEventHandler (FilmEditor::j2k_bandwidth_changed), 0, this);
	_dcp_resolution->Connect         (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED,      wxCommandEventHandler (FilmEditor::dcp_resolution_changed), 0, this);
	_sequence_video->Connect         (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED,     wxCommandEventHandler (FilmEditor::sequence_video_changed), 0, this);
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

	_sequence_video = new wxCheckBox (_content_panel, wxID_ANY, _("Keep video in sequence"));
	_content_sizer->Add (_sequence_video);

	_content_notebook = new wxNotebook (_content_panel, wxID_ANY);
	_content_sizer->Add (_content_notebook, 1, wxEXPAND | wxTOP, 6);

	_video_panel = new VideoPanel (this);
	_audio_panel = new AudioPanel (this);
	_subtitle_panel = new SubtitlePanel (this);
	_timing_panel = new TimingPanel (this);
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

void
FilmEditor::dcp_audio_channels_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_dcp_audio_channels (_dcp_audio_channels->GetValue ());
}

void
FilmEditor::dcp_resolution_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_resolution (_dcp_resolution->GetSelection() == 0 ? RESOLUTION_2K : RESOLUTION_4K);
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

	_video_panel->film_changed (p);
	_audio_panel->film_changed (p);
	_subtitle_panel->film_changed (p);
	_timing_panel->film_changed (p);
		
	switch (p) {
	case Film::NONE:
		break;
	case Film::CONTENT:
		setup_content ();
		break;
	case Film::CONTAINER:
		setup_container ();
		break;
	case Film::NAME:
		checked_set (_name, _film->name());
		setup_dcp_name ();
		break;
	case Film::WITH_SUBTITLES:
		setup_dcp_name ();
		break;
	case Film::DCP_CONTENT_TYPE:
		checked_set (_dcp_content_type, DCPContentType::as_index (_film->dcp_content_type ()));
		setup_dcp_name ();
		break;
	case Film::SCALER:
		checked_set (_scaler, Scaler::as_index (_film->scaler ()));
		break;
	case Film::RESOLUTION:
		checked_set (_dcp_resolution, _film->resolution() == RESOLUTION_2K ? 0 : 1);
		setup_dcp_name ();
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
	case Film::DCP_AUDIO_CHANNELS:
		_dcp_audio_channels->SetValue (_film->dcp_audio_channels ());
		setup_dcp_name ();
		break;
	case Film::SEQUENCE_VIDEO:
		checked_set (_sequence_video, _film->sequence_video ());
		break;
	}
}

void
FilmEditor::film_content_changed (weak_ptr<Content> weak_content, int property)
{
	ensure_ui_thread ();
	
	if (!_film) {
		/* We call this method ourselves (as well as using it as a signal handler)
		   so _film can be 0.
		*/
		return;
	}

	shared_ptr<Content> content = weak_content.lock ();
	if (content != selected_content ()) {
		return;
	}
	
	shared_ptr<VideoContent> video_content;
	shared_ptr<AudioContent> audio_content;
	shared_ptr<SubtitleContent> subtitle_content;
	shared_ptr<FFmpegContent> ffmpeg_content;
	if (content) {
		video_content = dynamic_pointer_cast<VideoContent> (content);
		audio_content = dynamic_pointer_cast<AudioContent> (content);
		subtitle_content = dynamic_pointer_cast<SubtitleContent> (content);
		ffmpeg_content = dynamic_pointer_cast<FFmpegContent> (content);
	}

	_video_panel->film_content_changed    (content, property);
	_audio_panel->film_content_changed    (content, property);
	_subtitle_panel->film_content_changed (content, property);
	_timing_panel->film_content_changed   (content, property);

	if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_dcp_name ();
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
	_video_panel->setup_scaling_description ();
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

	film_changed (Film::NAME);
	film_changed (Film::USE_DCI_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::RESOLUTION);
	film_changed (Film::SCALER);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::DCI_METADATA);
	film_changed (Film::DCP_VIDEO_FRAME_RATE);
	film_changed (Film::DCP_AUDIO_CHANNELS);
	film_changed (Film::SEQUENCE_VIDEO);

	if (!_film->content().empty ()) {
		set_selection (_film->content().front ());
	}

	wxListEvent ev;
	content_selection_changed (ev);
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
	_content->Enable (s);
	_dcp_content_type->Enable (s);
	_dcp_frame_rate->Enable (s);
	_dcp_audio_channels->Enable (s);
	_j2k_bandwidth->Enable (s);
	_container->Enable (s);

	_subtitle_panel->setup_control_sensitivity ();
	_audio_panel->setup_sensitivity ();
	setup_content_sensitivity ();
	_best_dcp_frame_rate->Enable (s && _film && _film->best_dcp_video_frame_rate () != _film->dcp_video_frame_rate ());
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
FilmEditor::best_dcp_frame_rate_clicked (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_dcp_video_frame_rate (_film->best_dcp_video_frame_rate ());
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

		if (StillImageContent::valid_file (p)) {
			c.reset (new StillImageContent (_film, p));
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

	wxListEvent ev;
	content_selection_changed (ev);
}

void
FilmEditor::content_selection_changed (wxListEvent &)
{
	setup_content_sensitivity ();
	shared_ptr<Content> s = selected_content ();

	_audio_panel->content_selection_changed ();
	
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
	film_content_changed (s, FFmpegContentProperty::SUBTITLE_STREAMS);
	film_content_changed (s, FFmpegContentProperty::FILTERS);
	film_content_changed (s, SubtitleContentProperty::SUBTITLE_OFFSET);
	film_content_changed (s, SubtitleContentProperty::SUBTITLE_SCALE);
}

void
FilmEditor::setup_content_sensitivity ()
{
	_content_add->Enable (_generally_sensitive);

	shared_ptr<Content> selection = selected_content ();

	_content_remove->Enable (selection && _generally_sensitive);
	_content_timeline->Enable (_generally_sensitive);

	_video_panel->Enable	(selection && dynamic_pointer_cast<VideoContent>  (selection) && _generally_sensitive);
	_audio_panel->Enable	(selection && dynamic_pointer_cast<AudioContent>  (selection) && _generally_sensitive);
	_subtitle_panel->Enable (selection && dynamic_pointer_cast<FFmpegContent> (selection) && _generally_sensitive);
	_timing_panel->Enable	(selection && _generally_sensitive);
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

shared_ptr<SubtitleContent>
FilmEditor::selected_subtitle_content ()
{
	shared_ptr<Content> c = selected_content ();
	if (!c) {
		return shared_ptr<SubtitleContent> ();
	}

	return dynamic_pointer_cast<SubtitleContent> (c);
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
FilmEditor::set_selection (weak_ptr<Content> wc)
{
	ContentList content = _film->content ();
	for (size_t i = 0; i < content.size(); ++i) {
		if (content[i] == wc.lock ()) {
			_content->SetItemState (i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		} else {
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		}
	}
}

void
FilmEditor::sequence_video_changed (wxCommandEvent &)
{
	_film->set_sequence_video (_sequence_video->GetValue ());
}

void
FilmEditor::content_right_click (wxListEvent& ev)
{
	ContentList cl;
	cl.push_back (selected_content ());
	_menu.popup (cl, ev.GetPoint ());
}
