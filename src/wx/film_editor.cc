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
#include "lib/moving_image_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/sndfile_content.h"
#include "lib/dcp_content_type.h"
#include "lib/sound_processor.h"
#include "lib/scaler.h"
#include "lib/playlist.h"
#include "lib/content.h"
#include "lib/content_factory.h"
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
		_frame_rate = new wxChoice (_dcp_panel, wxID_ANY);
		s->Add (_frame_rate, 1, wxALIGN_CENTER_VERTICAL);
		_best_frame_rate = new wxButton (_dcp_panel, wxID_ANY, _("Use best"));
		s->Add (_best_frame_rate, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);
		grid->Add (s, wxGBPosition (r, 1));
	}
	++r;

	_encrypted = new wxCheckBox (_dcp_panel, wxID_ANY, wxT ("Encrypted"));
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
		add_label_to_sizer (s, _dcp_panel, _("MBps"), false);
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
		_frame_rate->Append (std_to_wx (boost::lexical_cast<string> (*i)));
	}

	_audio_channels->SetRange (0, MAX_AUDIO_CHANNELS);
	_j2k_bandwidth->SetRange (1, 250);

	_resolution->Append (_("2K"));
	_resolution->Append (_("4K"));

	_standard->Append (_("SMPTE"));
	_standard->Append (_("Interop"));
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Bind		(wxEVT_COMMAND_TEXT_UPDATED, 	      boost::bind (&FilmEditor::name_changed, this));
	_use_dci_name->Bind	(wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::use_dci_name_toggled, this));
	_edit_dci_button->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::edit_dci_button_clicked, this));
	_container->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::container_changed, this));
	_content->Bind		(wxEVT_COMMAND_LIST_ITEM_SELECTED,    boost::bind (&FilmEditor::content_selection_changed, this));
	_content->Bind		(wxEVT_COMMAND_LIST_ITEM_DESELECTED,  boost::bind (&FilmEditor::content_selection_changed, this));
	_content->Bind          (wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, boost::bind (&FilmEditor::content_right_click, this, _1));
	_content_add_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED,        boost::bind (&FilmEditor::content_add_file_clicked, this));
	_content_add_folder->Bind (wxEVT_COMMAND_BUTTON_CLICKED,      boost::bind (&FilmEditor::content_add_folder_clicked, this));
	_content_remove->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_remove_clicked, this));
	_content_earlier->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_earlier_clicked, this));
	_content_later->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_later_clicked, this));
	_content_timeline->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::content_timeline_clicked, this));
	_scaler->Bind		(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::scaler_changed, this));
	_dcp_content_type->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::dcp_content_type_changed, this));
	_frame_rate->Bind	(wxEVT_COMMAND_CHOICE_SELECTED,	      boost::bind (&FilmEditor::frame_rate_changed, this));
	_best_frame_rate->Bind	(wxEVT_COMMAND_BUTTON_CLICKED,	      boost::bind (&FilmEditor::best_frame_rate_clicked, this));
	_encrypted->Bind        (wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::encrypted_toggled, this));
	_audio_channels->Bind	(wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&FilmEditor::audio_channels_changed, this));
	_j2k_bandwidth->Bind	(wxEVT_COMMAND_SPINCTRL_UPDATED,      boost::bind (&FilmEditor::j2k_bandwidth_changed, this));
	_resolution->Bind       (wxEVT_COMMAND_CHOICE_SELECTED,       boost::bind (&FilmEditor::resolution_changed, this));
	_sequence_video->Bind   (wxEVT_COMMAND_CHECKBOX_CLICKED,      boost::bind (&FilmEditor::sequence_video_changed, this));
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
		b->Add (_content_add_file, 1, wxEXPAND | wxLEFT | wxRIGHT);
		_content_add_folder = new wxButton (_content_panel, wxID_ANY, _("Add folder..."));
		b->Add (_content_add_folder, 1, wxEXPAND | wxLEFT | wxRIGHT);
		_content_remove = new wxButton (_content_panel, wxID_ANY, _("Remove"));
		b->Add (_content_remove, 1, wxEXPAND | wxLEFT | wxRIGHT);
		_content_earlier = new wxButton (_content_panel, wxID_UP);
		b->Add (_content_earlier, 1, wxEXPAND);
		_content_later = new wxButton (_content_panel, wxID_DOWN);
		b->Add (_content_later, 1, wxEXPAND);
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
	_panels.push_back (_video_panel);
	_audio_panel = new AudioPanel (this);
	_panels.push_back (_audio_panel);
	_subtitle_panel = new SubtitlePanel (this);
	_panels.push_back (_subtitle_panel);
	_timing_panel = new TimingPanel (this);
	_panels.push_back (_timing_panel);
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
FilmEditor::encrypted_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_encrypted (_encrypted->GetValue ());
}
			       
/** Called when the name widget has been changed */
void
FilmEditor::frame_rate_changed ()
{
	if (!_film) {
		return;
	}

	_film->set_video_frame_rate (
		boost::lexical_cast<int> (
			wx_to_std (_frame_rate->GetString (_frame_rate->GetSelection ()))
			)
		);
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

	stringstream s;

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
	case Film::ENCRYPTED:
		checked_set (_encrypted, _film->encrypted ());
		break;
	case Film::RESOLUTION:
		checked_set (_resolution, _film->resolution() == RESOLUTION_2K ? 0 : 1);
		setup_dcp_name ();
		break;
	case Film::J2K_BANDWIDTH:
		checked_set (_j2k_bandwidth, _film->j2k_bandwidth() / 1000000);
		break;
	case Film::USE_DCI_NAME:
		checked_set (_use_dci_name, _film->use_dci_name ());
		setup_dcp_name ();
		break;
	case Film::DCI_METADATA:
		setup_dcp_name ();
		break;
	case Film::VIDEO_FRAME_RATE:
	{
		bool done = false;
		for (unsigned int i = 0; i < _frame_rate->GetCount(); ++i) {
			if (wx_to_std (_frame_rate->GetString(i)) == boost::lexical_cast<string> (_film->video_frame_rate())) {
				checked_set (_frame_rate, i);
				done = true;
				break;
			}
		}

		if (!done) {
			checked_set (_frame_rate, -1);
		}

		_best_frame_rate->Enable (_film->best_video_frame_rate () != _film->video_frame_rate ());
		break;
	}
	case Film::AUDIO_CHANNELS:
		checked_set (_audio_channels, _film->audio_channels ());
		setup_dcp_name ();
		break;
	case Film::SEQUENCE_VIDEO:
		checked_set (_sequence_video, _film->sequence_video ());
		break;
	case Film::THREE_D:
		checked_set (_three_d, _film->three_d ());
		setup_dcp_name ();
		break;
	case Film::INTEROP:
		checked_set (_standard, _film->interop() ? 1 : 0);
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
	film_changed (Film::USE_DCI_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::CONTAINER);
	film_changed (Film::RESOLUTION);
	film_changed (Film::SCALER);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::J2K_BANDWIDTH);
	film_changed (Film::DCI_METADATA);
	film_changed (Film::VIDEO_FRAME_RATE);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::SEQUENCE_VIDEO);
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
	_use_dci_name->Enable (s);
	_edit_dci_button->Enable (s);
	_content->Enable (s);
	_content_add_file->Enable (s);
	_content_add_folder->Enable (s);
	_content_remove->Enable (s);
	_content_earlier->Enable (s);
	_content_later->Enable (s);
	_content_timeline->Enable (s);
	_dcp_content_type->Enable (s);
	_encrypted->Enable (s);
	_frame_rate->Enable (s);
	_audio_channels->Enable (s);
	_j2k_bandwidth->Enable (s);
	_container->Enable (s);
	_best_frame_rate->Enable (s && _film && _film->best_video_frame_rate () != _film->video_frame_rate ());
	_sequence_video->Enable (s);
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
FilmEditor::use_dci_name_toggled ()
{
	if (!_film) {
		return;
	}

	_film->set_use_dci_name (_use_dci_name->GetValue ());
}

void
FilmEditor::edit_dci_button_clicked ()
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
	for (ContentList::iterator i = content.begin(); i != content.end(); ++i) {
		int const t = _content->GetItemCount ();
		bool const valid = (*i)->path_valid ();

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
		_film->examine_and_add_content (content_factory (_film, wx_to_std (d->GetPath ())));
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

	_film->examine_and_add_content (
		shared_ptr<MovingImageContent> (
			new MovingImageContent (_film, boost::filesystem::path (wx_to_std (d->GetPath ())))
			)
		);
}

void
FilmEditor::content_remove_clicked ()
{
	ContentList c = selected_content ();
	if (c.size() == 1) {
		_film->remove_content (c.front ());
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

	_content_remove->Enable   (selection.size() == 1 && _generally_sensitive);
	_content_earlier->Enable  (selection.size() == 1 && _generally_sensitive);
	_content_later->Enable    (selection.size() == 1 && _generally_sensitive);
	_content_timeline->Enable (_generally_sensitive);

	_video_panel->Enable	(video_selection.size() > 0 && _generally_sensitive);
	_audio_panel->Enable	(audio_selection.size() > 0 && _generally_sensitive);
	_subtitle_panel->Enable (selection.size() == 1 && dynamic_pointer_cast<FFmpegContent> (selection.front()) && _generally_sensitive);
	_timing_panel->Enable	(selection.size() == 1 && _generally_sensitive);
}

ContentList
FilmEditor::selected_content ()
{
	ContentList sel;
	long int s = -1;
	while (1) {
		s = _content->GetNextItem (s, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			break;
		}

		sel.push_back (_film->content()[s]);
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
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
		}
	}
}

void
FilmEditor::sequence_video_changed ()
{
	if (!_film) {
		return;
	}
	
	_film->set_sequence_video (_sequence_video->GetValue ());
}

void
FilmEditor::content_right_click (wxListEvent& ev)
{
	_menu.popup (selected_content (), ev.GetPoint ());
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
