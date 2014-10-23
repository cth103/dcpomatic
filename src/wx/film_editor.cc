/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
#include "lib/image_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/sndfile_content.h"
#include "lib/dcp_content_type.h"
#include "lib/sound_processor.h"
#include "lib/scaler.h"
#include "lib/playlist.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/safe_stringstream.h"
#include "timecode.h"
#include "wx_util.h"
#include "film_editor.h"
#include "isdcf_metadata_dialog.h"
#include "timeline_dialog.h"
#include "timing_panel.h"
#include "subtitle_panel.h"
#include "audio_panel.h"
#include "video_panel.h"

using std::string;
using std::cout;
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
FilmEditor::FilmEditor (wxWindow* parent)
	: wxPanel (parent)
	, _menu (this)
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
	
	connect_to_widgets ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmEditor::active_jobs_changed, this, _1)
		);

	Config::instance()->Changed.connect (boost::bind (&FilmEditor::config_changed, this));

	set_film (shared_ptr<Film> ());
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
	
	int flags = wxALIGN_CENTER_VERTICAL;
#ifdef __WXOSX__
	flags |= wxALIGN_RIGHT;
#endif	

	_use_isdcf_name = new wxCheckBox (_dcp_panel, wxID_ANY, _("Use ISDCF name"));
	grid->Add (_use_isdcf_name, wxGBPosition (r, 0), wxDefaultSpan, flags);
	_edit_isdcf_button = new wxButton (_dcp_panel, wxID_ANY, _("Details..."));
	grid->Add (_edit_isdcf_button, wxGBPosition (r, 1), wxDefaultSpan);
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("DCP Name"), true, wxGBPosition (r, 0));
	_dcp_name = new wxStaticText (_dcp_panel, wxID_ANY, wxT (""));
	grid->Add (_dcp_name, wxGBPosition(r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
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
		_frame_rate_sizer = new wxBoxSizer (wxHORIZONTAL);
		_frame_rate_choice = new wxChoice (_dcp_panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_choice, 1, wxALIGN_CENTER_VERTICAL);
		_frame_rate_spin = new wxSpinCtrl (_dcp_panel, wxID_ANY);
		_frame_rate_sizer->Add (_frame_rate_spin, 1, wxALIGN_CENTER_VERTICAL);
		setup_frame_rate_widget ();
		_best_frame_rate = new wxButton (_dcp_panel, wxID_ANY, _("Use best"));
		_frame_rate_sizer->Add (_best_frame_rate, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
		grid->Add (_frame_rate_sizer, wxGBPosition (r, 1));
	}
	++r;

	_signed = new wxCheckBox (_dcp_panel, wxID_ANY, _("Signed"));
	grid->Add (_signed, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;
	
	_encrypted = new wxCheckBox (_dcp_panel, wxID_ANY, _("Encrypted"));
	grid->Add (_encrypted, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Audio channels"), true, wxGBPosition (r, 0));
	_audio_channels = new wxSpinCtrl (_dcp_panel, wxID_ANY);
	grid->Add (_audio_channels, wxGBPosition (r, 1));
	++r;

	_three_d = new wxCheckBox (_dcp_panel, wxID_ANY, _("3D"));
	grid->Add (_three_d, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Resolution"), true, wxGBPosition (r, 0));
	_resolution = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_resolution, wxGBPosition (r, 1));
	++r;

	{
		add_label_to_grid_bag_sizer (grid, _dcp_panel, _("JPEG2000 bandwidth"), true, wxGBPosition (r, 0));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_j2k_bandwidth = new wxSpinCtrl (_dcp_panel, wxID_ANY);
		s->Add (_j2k_bandwidth, 1);
		add_label_to_sizer (s, _dcp_panel, _("Mbit/s"), false);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	add_label_to_grid_bag_sizer (grid, _dcp_panel, _("Standard"), true, wxGBPosition (r, 0));
	_standard = new wxChoice (_dcp_panel, wxID_ANY);
	grid->Add (_standard, wxGBPosition (r, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
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
		_frame_rate_choice->Append (std_to_wx (boost::lexical_cast<string> (*i)));
	}

	_audio_channels->SetRange (0, MAX_DCP_AUDIO_CHANNELS);
	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	_frame_rate_spin->SetRange (1, 480);

	_resolution->Append (_("2K"));
	_resolution->Append (_("4K"));

	_standard->Append (_("SMPTE"));
	_standard->Append (_("Interop"));
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Bind		(wxEVT_COMMAND_TEXT_UPDATED, 	      boost::bind (&FilmEditor::name_changed, this));
	_use_isdcf_name->Bind	(wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::use_isdcf_name_toggled, this));
	_edit_isdcf_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::edit_isdcf_button_clicked, this));
	_container->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::container_changed, this));
	_content->Bind		(wxEVT_COMMAND_LIST_ITEM_SELECTED,    boost::bind (&FilmEditor::content_selection_changed, this));
	_content->Bind		(wxEVT_COMMAND_LIST_ITEM_DESELECTED,  boost::bind (&FilmEditor::content_selection_changed, this));
	_content->Bind          (wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, boost::bind (&FilmEditor::content_right_click, this, _1));
	_content->Bind          (wxEVT_DROP_FILES,                    boost::bind (&FilmEditor::content_files_dropped, this, _1));
	_content_add_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED,        boost::bind (&FilmEditor::content_add_file_clicked, this));
	_content_add_folder->Bind (wxEVT_COMMAND_BUTTON_CLICKED,      boost::bind (&FilmEditor::content_add_folder_clicked, this));
	_content_remove->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_remove_clicked, this));
	_content_earlier->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_earlier_clicked, this));
	_content_later->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_later_clicked, this));
	_content_timeline->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_timeline_clicked, this));
	_scaler->Bind		(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::scaler_changed, this));
	_dcp_content_type->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::dcp_content_type_changed, this));
	_frame_rate_choice->Bind(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::frame_rate_choice_changed, this));
	_frame_rate_spin->Bind  (wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&FilmEditor::frame_rate_spin_changed, this));
	_best_frame_rate->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::best_frame_rate_clicked, this));
	_signed->Bind           (wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::signed_toggled, this));
	_encrypted->Bind        (wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::encrypted_toggled, this));
	_audio_channels->Bind	(wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&FilmEditor::audio_channels_changed, this));
	_j2k_bandwidth->Bind	(wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&FilmEditor::j2k_bandwidth_changed, this));
	_resolution->Bind       (wxEVT_COMMAND_CHOICE_SELECTED,       boost::bind (&FilmEditor::resolution_changed, this));
	_three_d->Bind	 	(wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::three_d_changed, this));
	_standard->Bind         (wxEVT_COMMAND_CHOICE_SELECTED,       boost::bind (&FilmEditor::standard_changed, this));
}

void
FilmEditor::make_content_panel ()
{
	_content_panel = new wxPanel (_main_notebook);
	_content_sizer = new wxBoxSizer (wxVERTICAL);
	_content_panel->SetSizer (_content_sizer);

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		
		_content = new wxListCtrl (_content_panel, wxID_ANY, wxDefaultPosition, wxSize (320, 160), wxLC_REPORT | wxLC_NO_HEADER);
		s->Add (_content, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);

		_content->InsertColumn (0, wxT(""));
		_content->SetColumnWidth (0, 512);

		wxBoxSizer* b = new wxBoxSizer (wxVERTICAL);
		_content_add_file = new wxButton (_content_panel, wxID_ANY, _("Add file(s)..."));
		b->Add (_content_add_file, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		_content_add_folder = new wxButton (_content_panel, wxID_ANY, _("Add folder..."));
		b->Add (_content_add_folder, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		_content_remove = new wxButton (_content_panel, wxID_ANY, _("Remove"));
		b->Add (_content_remove, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		_content_earlier = new wxButton (_content_panel, wxID_ANY, _("Up"));
		b->Add (_content_earlier, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		_content_later = new wxButton (_content_panel, wxID_ANY, _("Down"));
		b->Add (_content_later, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		_content_timeline = new wxButton (_content_panel, wxID_ANY, _("Timeline..."));
		b->Add (_content_timeline, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		s->Add (b, 0, wxALL, 4);

		_content_sizer->Add (s, 0, wxEXPAND | wxALL, 6);
	}

	_content_notebook = new wxNotebook (_content_panel, wxID_ANY);
	_content_sizer->Add (_content_notebook, 1, wxEXPAND | wxTOP, 6);

	_video_panel = new VideoPanel (this);
	_panels.push_back (_video_panel);
	_audio_panel = new AudioPanel (this);
	_panels.push_back (_audio_panel);
	_subtitle_panel = new SubtitlePanel (this);
	_panels.push_back (_subtitle_panel);
	_timing_panel = new TimingPanel (this);
	_panels.push_back (_timing_panel);

	_content->DragAcceptFiles (true);
}

/** Called when the name widget has been changed */
void
FilmEditor::name_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_name (string (_name->GetValue().mb_str()));
}

void
FilmEditor::j2k_bandwidth_changed ()
{
	if (!_film) {
		return;
	}
	
	_film->set_j2k_bandwidth (_j2k_bandwidth->GetValue() * 1000000);
}

void
FilmEditor::signed_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_signed (_signed->GetValue ());
}

void
FilmEditor::encrypted_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_encrypted (_encrypted->GetValue ());
}
			       
/** Called when the frame rate choice widget has been changed */
void
FilmEditor::frame_rate_choice_changed ()
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
FilmEditor::frame_rate_spin_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (_frame_rate_spin->GetValue ());
}

void
FilmEditor::audio_channels_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_audio_channels (_audio_channels->GetValue ());
}

void
FilmEditor::resolution_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_resolution (_resolution->GetSelection() == 0 ? RESOLUTION_2K : RESOLUTION_4K);
}

void
FilmEditor::standard_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_interop (_standard->GetSelection() == 1);
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

	SafeStringStream s;

	for (list<FilmEditorPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->film_changed (p);
	}
		
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
	case Film::SIGNED:
		checked_set (_signed, _film->is_signed ());
		break;
	case Film::ENCRYPTED:
		checked_set (_encrypted, _film->encrypted ());
		if (_film->encrypted ()) {
			_film->set_signed (true);
			_signed->Enable (false);
		} else {
			_signed->Enable (_generally_sensitive);
		}
		break;
	case Film::RESOLUTION:
		checked_set (_resolution, _film->resolution() == RESOLUTION_2K ? 0 : 1);
		setup_dcp_name ();
		break;
	case Film::J2K_BANDWIDTH:
		checked_set (_j2k_bandwidth, _film->j2k_bandwidth() / 1000000);
		break;
	case Film::USE_ISDCF_NAME:
		checked_set (_use_isdcf_name, _film->use_isdcf_name ());
		setup_dcp_name ();
		use_isdcf_name_changed ();
		break;
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
		checked_set (_audio_channels, _film->audio_channels ());
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
FilmEditor::film_content_changed (int property)
{
	ensure_ui_thread ();
	
	if (!_film) {
		/* We call this method ourselves (as well as using it as a signal handler)
		   so _film can be 0.
		*/
		return;
	}

	for (list<FilmEditorPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->film_content_changed (property);
	}

	if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_dcp_name ();
	} else if (property == ContentProperty::PATH) {
		setup_content ();
	} else if (property == ContentProperty::POSITION) {
		setup_content ();
	} else if (property == VideoContentProperty::VIDEO_SCALE) {
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
}	

/** Called when the container widget has been changed */
void
FilmEditor::container_changed ()
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
FilmEditor::dcp_content_type_changed ()
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
	set_general_sensitivity (f != 0);

	if (_film == f) {
		return;
	}
	
	_film = f;

	if (_film) {
		_film->Changed.connect (bind (&FilmEditor::film_changed, this, _1));
		_film->ContentChanged.connect (bind (&FilmEditor::film_content_changed, this, _2));
	}

	if (_film) {
		FileChanged (_film->directory ());
	} else {
		FileChanged ("");
	}

	film_changed (Film::NAME);
	film_changed (Film::USE_ISDCF_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::RESOLUTION);
	film_changed (Film::SCALER);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SIGNED);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::ISDCF_METADATA);
	film_changed (Film::VIDEO_FRAME_RATE);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::THREE_D);
	film_changed (Film::INTEROP);

	if (!_film->content().empty ()) {
		set_selection (_film->content().front ());
	}

	content_selection_changed ();
}

void
FilmEditor::set_general_sensitivity (bool s)
{
	_generally_sensitive = s;

	/* Stuff in the Content / DCP tabs */
	_name->Enable (s);
	_use_isdcf_name->Enable (s);
	_edit_isdcf_button->Enable (s);
	_content->Enable (s);
	_content_add_file->Enable (s);
	_content_add_folder->Enable (s);
	_content_remove->Enable (s);
	_content_earlier->Enable (s);
	_content_later->Enable (s);
	_content_timeline->Enable (s);
	_dcp_content_type->Enable (s);

	bool si = s;
	if (_film && _film->encrypted ()) {
		si = false;
	}
	_signed->Enable (si);
	
	_encrypted->Enable (s);
	_frame_rate_choice->Enable (s);
	_frame_rate_spin->Enable (s);
	_audio_channels->Enable (s);
	_j2k_bandwidth->Enable (s);
	_container->Enable (s);
	_best_frame_rate->Enable (s && _film && _film->best_video_frame_rate () != _film->video_frame_rate ());
	_resolution->Enable (s);
	_scaler->Enable (s);
	_three_d->Enable (s);
	_standard->Enable (s);

	/* Set the panels in the content notebook */
	for (list<FilmEditorPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->Enable (s);
	}
}

/** Called when the scaler widget has been changed */
void
FilmEditor::scaler_changed ()
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
FilmEditor::use_isdcf_name_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_use_isdcf_name (_use_isdcf_name->GetValue ());
}

void
FilmEditor::use_isdcf_name_changed ()
{
	bool const i = _film->use_isdcf_name ();

	if (!i) {
		_film->set_name (_film->isdcf_name (true));
	}

	_edit_isdcf_button->Enable (i);
}

void
FilmEditor::edit_isdcf_button_clicked ()
{
	if (!_film) {
		return;
	}

	ISDCFMetadataDialog* d = new ISDCFMetadataDialog (this, _film->isdcf_metadata ());
	d->ShowModal ();
	_film->set_isdcf_metadata (d->isdcf_metadata ());
	d->Destroy ();
}

void
FilmEditor::active_jobs_changed (bool a)
{
	set_general_sensitivity (!a);
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
FilmEditor::best_frame_rate_clicked ()
{
	if (!_film) {
		return;
	}
	
	_film->set_video_frame_rate (_film->best_video_frame_rate ());
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
	sort (content.begin(), content.end(), ContentSorter ());
	
	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		int const t = _content->GetItemCount ();
		bool const valid = (*i)->paths_valid ();

		string s = (*i)->summary ();
		if (!valid) {
			s = _("MISSING: ") + s;
		}

		_content->InsertItem (t, std_to_wx (s));

		if ((*i)->summary() == selected_summary) {
			_content->SetItemState (t, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}

		if (!valid) {
			_content->SetItemTextColour (t, *wxRED);
		}
	}

	if (selected_summary.empty () && !content.empty ()) {
		/* Select the item of content if none was selected before */
		_content->SetItemState (0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
}

void
FilmEditor::content_add_file_clicked ()
{
	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	wxFileDialog* d = new wxFileDialog (this, _("Choose a file or files"), wxT (""), wxT (""), wxT ("*.*"), wxFD_MULTIPLE | wxFD_CHANGE_DIR);
	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	wxArrayString paths;
	d->GetPaths (paths);

	/* XXX: check for lots of files here and do something */

	for (unsigned int i = 0; i < paths.GetCount(); ++i) {
		_film->examine_and_add_content (content_factory (_film, wx_to_std (paths[i])));
	}

	d->Destroy ();
}

void
FilmEditor::content_add_folder_clicked ()
{
	wxDirDialog* d = new wxDirDialog (this, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
	int const r = d->ShowModal ();
	d->Destroy ();
	
	if (r != wxID_OK) {
		return;
	}

	shared_ptr<ImageContent> ic;
	
	try {
		ic.reset (new ImageContent (_film, boost::filesystem::path (wx_to_std (d->GetPath ()))));
	} catch (FileError& e) {
		error_dialog (this, std_to_wx (e.what ()));
		return;
	}

	_film->examine_and_add_content (ic);
}

void
FilmEditor::content_remove_clicked ()
{
	ContentList c = selected_content ();
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		_film->remove_content (*i);
	}

	content_selection_changed ();
}

void
FilmEditor::content_selection_changed ()
{
	setup_content_sensitivity ();

	for (list<FilmEditorPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->content_selection_changed ();
	}
}

/** Set up broad sensitivity based on the type of content that is selected */
void
FilmEditor::setup_content_sensitivity ()
{
	_content_add_file->Enable (_generally_sensitive);
	_content_add_folder->Enable (_generally_sensitive);

	ContentList selection = selected_content ();
	VideoContentList video_selection = selected_video_content ();
	AudioContentList audio_selection = selected_audio_content ();

	_content_remove->Enable   (!selection.empty() && _generally_sensitive);
	_content_earlier->Enable  (selection.size() == 1 && _generally_sensitive);
	_content_later->Enable    (selection.size() == 1 && _generally_sensitive);
	_content_timeline->Enable (!_film->content().empty() && _generally_sensitive);

	_video_panel->Enable	(!video_selection.empty() && _generally_sensitive);
	_audio_panel->Enable	(!audio_selection.empty() && _generally_sensitive);
	_subtitle_panel->Enable (selection.size() == 1 && dynamic_pointer_cast<FFmpegContent> (selection.front()) && _generally_sensitive);
	_timing_panel->Enable	(!selection.empty() && _generally_sensitive);
}

ContentList
FilmEditor::selected_content ()
{
	ContentList sel;

	if (!_film) {
		return sel;
	}

	/* The list was populated using a sorted content list, so we must sort it here too
	   so that we can look up by index and get the right thing.
	*/
	ContentList content = _film->content ();
	sort (content.begin(), content.end(), ContentSorter ());
	
	long int s = -1;
	while (true) {
		s = _content->GetNextItem (s, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			break;
		}

		if (s < int (_film->content().size ())) {
			sel.push_back (content[s]);
		}
	}

	return sel;
}

VideoContentList
FilmEditor::selected_video_content ()
{
	ContentList c = selected_content ();
	VideoContentList vc;
	
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		shared_ptr<VideoContent> t = dynamic_pointer_cast<VideoContent> (*i);
		if (t) {
			vc.push_back (t);
		}
	}

	return vc;
}

AudioContentList
FilmEditor::selected_audio_content ()
{
	ContentList c = selected_content ();
	AudioContentList ac;
	
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		shared_ptr<AudioContent> t = dynamic_pointer_cast<AudioContent> (*i);
		if (t) {
			ac.push_back (t);
		}
	}

	return ac;
}

SubtitleContentList
FilmEditor::selected_subtitle_content ()
{
	ContentList c = selected_content ();
	SubtitleContentList sc;
	
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		shared_ptr<SubtitleContent> t = dynamic_pointer_cast<SubtitleContent> (*i);
		if (t) {
			sc.push_back (t);
		}
	}

	return sc;
}

FFmpegContentList
FilmEditor::selected_ffmpeg_content ()
{
	ContentList c = selected_content ();
	FFmpegContentList sc;
	
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		shared_ptr<FFmpegContent> t = dynamic_pointer_cast<FFmpegContent> (*i);
		if (t) {
			sc.push_back (t);
		}
	}

	return sc;
}

void
FilmEditor::content_timeline_clicked ()
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
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED);
		}
	}
}

void
FilmEditor::content_right_click (wxListEvent& ev)
{
	_menu.popup (_film, selected_content (), ev.GetPoint ());
}

void
FilmEditor::three_d_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_three_d (_three_d->GetValue ());
}

void
FilmEditor::content_earlier_clicked ()
{
	ContentList sel = selected_content ();
	if (sel.size() == 1) {
		_film->move_content_earlier (sel.front ());
		content_selection_changed ();
	}
}

void
FilmEditor::content_later_clicked ()
{
	ContentList sel = selected_content ();
	if (sel.size() == 1) {
		_film->move_content_later (sel.front ());
		content_selection_changed ();
	}
}

void
FilmEditor::config_changed ()
{
	_j2k_bandwidth->SetRange (1, Config::instance()->maximum_j2k_bandwidth() / 1000000);
	setup_frame_rate_widget ();
}

void
FilmEditor::setup_frame_rate_widget ()
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

void
FilmEditor::content_files_dropped (wxDropFilesEvent& event)
{
	if (!_film) {
		return;
	}
	
	wxString* paths = event.GetFiles ();
	for (int i = 0; i < event.GetNumberOfFiles(); i++) {
		_film->examine_and_add_content (content_factory (_film, wx_to_std (paths[i])));
	}
}
