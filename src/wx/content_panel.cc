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

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include "lib/audio_content.h"
#include "lib/subtitle_content.h"
#include "lib/video_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/image_content.h"
#include "lib/dcp_content.h"
#include "lib/playlist.h"
#include "content_panel.h"
#include "wx_util.h"
#include "video_panel.h"
#include "audio_panel.h"
#include "subtitle_panel.h"
#include "timing_panel.h"
#include "timeline_dialog.h"

using std::list;
using std::string;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

ContentPanel::ContentPanel (wxNotebook* n, boost::shared_ptr<Film> f)
	: _timeline_dialog (0)
	, _film (f)
	, _generally_sensitive (true)
{
	_panel = new wxPanel (n);
	_sizer = new wxBoxSizer (wxVERTICAL);
	_panel->SetSizer (_sizer);

	_menu = new ContentMenu (_panel);

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		
		_content = new wxListCtrl (_panel, wxID_ANY, wxDefaultPosition, wxSize (320, 160), wxLC_REPORT | wxLC_NO_HEADER);
		s->Add (_content, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);

		_content->InsertColumn (0, wxT(""));
		_content->SetColumnWidth (0, 512);

		wxBoxSizer* b = new wxBoxSizer (wxVERTICAL);
		
		_add_file = new wxButton (_panel, wxID_ANY, _("Add file(s)..."));
		_add_file->SetToolTip (_("Add video, image or sound files to the film."));
		b->Add (_add_file, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		
		_add_folder = new wxButton (_panel, wxID_ANY, _("Add folder..."));
		_add_folder->SetToolTip (_("Add a folder of image files (which will be used as a moving image sequence) or a DCP."));
		b->Add (_add_folder, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		
		_remove = new wxButton (_panel, wxID_ANY, _("Remove"));
		_remove->SetToolTip (_("Remove the selected piece of content from the film."));
		b->Add (_remove, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		
		_earlier = new wxButton (_panel, wxID_ANY, _("Up"));
		_earlier->SetToolTip (_("Move the selected piece of content earlier in the film."));
		b->Add (_earlier, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		
		_later = new wxButton (_panel, wxID_ANY, _("Down"));
		_later->SetToolTip (_("Move the selected piece of content later in the film."));
		b->Add (_later, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);
		
		_timeline = new wxButton (_panel, wxID_ANY, _("Timeline..."));
		_timeline->SetToolTip (_("Open the timeline for the film."));
		b->Add (_timeline, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		s->Add (b, 0, wxALL, 4);

		_sizer->Add (s, 0, wxEXPAND | wxALL, 6);
	}

	_notebook = new wxNotebook (_panel, wxID_ANY);
	_sizer->Add (_notebook, 1, wxEXPAND | wxTOP, 6);

	_video_panel = new VideoPanel (this);
	_panels.push_back (_video_panel);
	_audio_panel = new AudioPanel (this);
	_panels.push_back (_audio_panel);
	_subtitle_panel = new SubtitlePanel (this);
	_panels.push_back (_subtitle_panel);
	_timing_panel = new TimingPanel (this);
	_panels.push_back (_timing_panel);

	_content->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&ContentPanel::selection_changed, this));
	_content->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&ContentPanel::selection_changed, this));
	_content->Bind (wxEVT_COMMAND_LIST_ITEM_RIGHT_CLICK, boost::bind (&ContentPanel::right_click, this, _1));
	_content->Bind (wxEVT_DROP_FILES, boost::bind (&ContentPanel::files_dropped, this, _1));
	_add_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::add_file_clicked, this));
	_add_folder->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::add_folder_clicked, this));
	_remove->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::remove_clicked, this));
	_earlier->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::earlier_clicked, this));
	_later->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::later_clicked, this));
	_timeline->Bind	(wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&ContentPanel::timeline_clicked, this));
}

ContentList
ContentPanel::selected ()
{
	ContentList sel;
	long int s = -1;
	while (true) {
		s = _content->GetNextItem (s, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (s == -1) {
			break;
		}

		if (s < int (_film->content().size ())) {
			sel.push_back (_film->content()[s]);
		}
	}

	return sel;
}

VideoContentList
ContentPanel::selected_video ()
{
	ContentList c = selected ();
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
ContentPanel::selected_audio ()
{
	ContentList c = selected ();
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
ContentPanel::selected_subtitle ()
{
	ContentList c = selected ();
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
ContentPanel::selected_ffmpeg ()
{
	ContentList c = selected ();
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
ContentPanel::film_changed (Film::Property p)
{
	switch (p) {
	case Film::CONTENT:
		setup ();
		break;
	default:
		break;
	}

	for (list<ContentSubPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->film_changed (p);
	}
}	

void
ContentPanel::selection_changed ()
{
	setup_sensitivity ();

	for (list<ContentSubPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->content_selection_changed ();
	}
}

void
ContentPanel::add_file_clicked ()
{
	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	wxFileDialog* d = new wxFileDialog (_panel, _("Choose a file or files"), wxT (""), wxT (""), wxT ("*.*"), wxFD_MULTIPLE | wxFD_CHANGE_DIR);
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
ContentPanel::add_folder_clicked ()
{
	wxDirDialog* d = new wxDirDialog (_panel, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
	int const r = d->ShowModal ();
	boost::filesystem::path const path (wx_to_std (d->GetPath ()));
	d->Destroy ();
	
	if (r != wxID_OK) {
		return;
	}

	shared_ptr<Content> content;
	
	try {
		content.reset (new ImageContent (_film, path));
	} catch (...) {
		try {
			content.reset (new DCPContent (_film, path));
		} catch (...) {
			error_dialog (_panel, _("Could not find any images nor a DCP in that folder"));
			return;
		}
	}

	if (content) {
		_film->examine_and_add_content (content);
	}
}

void
ContentPanel::remove_clicked ()
{
	ContentList c = selected ();
	if (c.size() == 1) {
		_film->remove_content (c.front ());
	}

	selection_changed ();
}

void
ContentPanel::timeline_clicked ()
{
	if (_timeline_dialog) {
		_timeline_dialog->Destroy ();
		_timeline_dialog = 0;
	}
	
	_timeline_dialog = new TimelineDialog (this, _film);
	_timeline_dialog->Show ();
}

void
ContentPanel::right_click (wxListEvent& ev)
{
	_menu->popup (_film, selected (), ev.GetPoint ());
}

/** Set up broad sensitivity based on the type of content that is selected */
void
ContentPanel::setup_sensitivity ()
{
	_add_file->Enable (_generally_sensitive);
	_add_folder->Enable (_generally_sensitive);

	ContentList selection = selected ();
	VideoContentList video_selection = selected_video ();
	AudioContentList audio_selection = selected_audio ();

	_remove->Enable   (selection.size() == 1 && _generally_sensitive);
	_earlier->Enable  (selection.size() == 1 && _generally_sensitive);
	_later->Enable    (selection.size() == 1 && _generally_sensitive);
	_timeline->Enable (!_film->content().empty() && _generally_sensitive);

	_video_panel->Enable	(video_selection.size() > 0 && _generally_sensitive);
	_audio_panel->Enable	(audio_selection.size() > 0 && _generally_sensitive);
	_subtitle_panel->Enable (selection.size() == 1 && dynamic_pointer_cast<SubtitleContent> (selection.front()) && _generally_sensitive);
	_timing_panel->Enable	(selection.size() == 1 && _generally_sensitive);
}

void
ContentPanel::set_film (shared_ptr<Film> f)
{
	_film = f;

	film_changed (Film::CONTENT);
	film_changed (Film::AUDIO_CHANNELS);
	selection_changed ();
}

void
ContentPanel::set_general_sensitivity (bool s)
{
	_generally_sensitive = s;

	_content->Enable (s);
	_add_file->Enable (s);
	_add_folder->Enable (s);
	_remove->Enable (s);
	_earlier->Enable (s);
	_later->Enable (s);
	_timeline->Enable (s);

	/* Set the panels in the content notebook */
	for (list<ContentSubPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->Enable (s);
	}
}

void
ContentPanel::earlier_clicked ()
{
	ContentList sel = selected ();
	if (sel.size() == 1) {
		_film->move_content_earlier (sel.front ());
		selection_changed ();
	}
}

void
ContentPanel::later_clicked ()
{
	ContentList sel = selected ();
	if (sel.size() == 1) {
		_film->move_content_later (sel.front ());
		selection_changed ();
	}
}

void
ContentPanel::set_selection (weak_ptr<Content> wc)
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
ContentPanel::film_content_changed (int property)
{
	if (property == ContentProperty::PATH || property == ContentProperty::POSITION || property == DCPContentProperty::CAN_BE_PLAYED) {
		setup ();
	}
		
	for (list<ContentSubPanel*>::iterator i = _panels.begin(); i != _panels.end(); ++i) {
		(*i)->film_content_changed (property);
	}
}

void
ContentPanel::setup ()
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
		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (*i);
		bool const needs_kdm = dcp && !dcp->can_be_played ();

		string s = (*i)->summary ();
		
		if (!valid) {
			s = _("MISSING: ") + s;
		}

		if (needs_kdm) {
			s = _("NEEDS KDM: ") + s;
		}

		_content->InsertItem (t, std_to_wx (s));

		if ((*i)->summary() == selected_summary) {
			_content->SetItemState (t, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}

		if (!valid || needs_kdm) {
			_content->SetItemTextColour (t, *wxRED);
		}
	}

	if (selected_summary.empty () && !content.empty ()) {
		/* Select the item of content if none was selected before */
		_content->SetItemState (0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
}

void
ContentPanel::files_dropped (wxDropFilesEvent& event)
{
	if (!_film) {
		return;
	}
	
	wxString* paths = event.GetFiles ();
	for (int i = 0; i < event.GetNumberOfFiles(); i++) {
		_film->examine_and_add_content (content_factory (_film, wx_to_std (paths[i])));
	}
}
