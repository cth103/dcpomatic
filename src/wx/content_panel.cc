/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

#include "content_panel.h"
#include "wx_util.h"
#include "video_panel.h"
#include "audio_panel.h"
#include "text_panel.h"
#include "timing_panel.h"
#include "timeline_dialog.h"
#include "image_sequence_dialog.h"
#include "film_viewer.h"
#include "dcpomatic_button.h"
#include "lib/audio_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/content_factory.h"
#include "lib/image_content.h"
#include "lib/dcp_content.h"
#include "lib/case_insensitive_sorter.h"
#include "lib/playlist.h"
#include "lib/config.h"
#include "lib/log.h"
#include "lib/compose.hpp"
#include "lib/string_text_file_content.h"
#include "lib/string_text_file.h"
#include "lib/dcpomatic_log.h"
#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/display.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using std::cout;
using std::vector;
using std::max;
using std::exception;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

ContentPanel::ContentPanel (wxNotebook* n, shared_ptr<Film> film, weak_ptr<FilmViewer> viewer)
	: _video_panel (0)
	, _audio_panel (0)
	, _timeline_dialog (0)
	, _parent (n)
	, _last_selected_tab (0)
	, _film (film)
	, _film_viewer (viewer)
	, _generally_sensitive (true)
	, _ignore_deselect (false)
{
	for (int i = 0; i < TEXT_COUNT; ++i) {
		_text_panel[i] = 0;
	}

	_splitter = new wxSplitterWindow (n);
	wxPanel* top = new wxPanel (_splitter);

	_menu = new ContentMenu (_splitter);


	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		_content = new wxListCtrl (top, wxID_ANY, wxDefaultPosition, wxSize (320, 160), wxLC_REPORT | wxLC_NO_HEADER);
		_content->DragAcceptFiles (true);
		s->Add (_content, 1, wxEXPAND | wxTOP | wxBOTTOM, 6);

		_content->InsertColumn (0, wxT(""));
		_content->SetColumnWidth (0, 512);

		wxBoxSizer* b = new wxBoxSizer (wxVERTICAL);

		_add_file = new Button (top, _("Add file(s)..."));
		_add_file->SetToolTip (_("Add video, image, sound or subtitle files to the film."));
		b->Add (_add_file, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_add_folder = new Button (top, _("Add folder..."));
		_add_folder->SetToolTip (_("Add a folder of image files (which will be used as a moving image sequence) or a folder of sound files."));
		b->Add (_add_folder, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_add_dcp = new Button (top, _("Add DCP..."));
		_add_dcp->SetToolTip (_("Add a DCP."));
		b->Add (_add_dcp, 1, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_remove = new Button (top, _("Remove"));
		_remove->SetToolTip (_("Remove the selected piece of content from the film."));
		b->Add (_remove, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_earlier = new Button (top, _("Earlier"));
		_earlier->SetToolTip (_("Move the selected piece of content earlier in the film."));
		b->Add (_earlier, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_later = new Button (top, _("Later"));
		_later->SetToolTip (_("Move the selected piece of content later in the film."));
		b->Add (_later, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		_timeline = new Button (top, _("Timeline..."));
		_timeline->SetToolTip (_("Open the timeline for the film."));
		b->Add (_timeline, 0, wxEXPAND | wxALL, DCPOMATIC_BUTTON_STACK_GAP);

		s->Add (b, 0, wxALL, 4);
		top->SetSizer (s);
	}

	_notebook = new wxNotebook (_splitter, wxID_ANY);

	/* This is a hack to try and make the content notebook a sensible size; large on big displays but small
	   enough on small displays to leave space for the content area.
	*/
	wxDisplay display (wxDisplay::GetFromWindow(_splitter));
	wxRect screen = display.GetClientArea();
	_splitter->SplitHorizontally (top, _notebook, screen.height > 800 ? -600 : -150);

	_timing_panel = new TimingPanel (this, _film_viewer);
	_notebook->AddPage (_timing_panel, _("Timing"), false);

	_content->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind (&ContentPanel::item_selected, this));
	_content->Bind (wxEVT_LIST_ITEM_DESELECTED, boost::bind (&ContentPanel::item_deselected, this));
	_content->Bind (wxEVT_LIST_ITEM_RIGHT_CLICK, boost::bind (&ContentPanel::right_click, this, _1));
	_content->Bind (wxEVT_DROP_FILES, boost::bind (&ContentPanel::files_dropped, this, _1));
	_add_file->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::add_file_clicked, this));
	_add_folder->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::add_folder_clicked, this));
	_add_dcp->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::add_dcp_clicked, this));
	_remove->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::remove_clicked, this, false));
	_earlier->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::earlier_clicked, this));
	_later->Bind (wxEVT_BUTTON, boost::bind (&ContentPanel::later_clicked, this));
	_timeline->Bind	(wxEVT_BUTTON, boost::bind (&ContentPanel::timeline_clicked, this));
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

		ContentList cl = _film->content();
		if (s < int (cl.size())) {
			sel.push_back (cl[s]);
		}
	}

	return sel;
}

ContentList
ContentPanel::selected_video ()
{
	ContentList vc;

	BOOST_FOREACH (shared_ptr<Content> i, selected ()) {
		if (i->video) {
			vc.push_back (i);
		}
	}

	return vc;
}

ContentList
ContentPanel::selected_audio ()
{
	ContentList ac;

	BOOST_FOREACH (shared_ptr<Content> i, selected ()) {
		if (i->audio) {
			ac.push_back (i);
		}
	}

	return ac;
}

ContentList
ContentPanel::selected_text ()
{
	ContentList sc;

	BOOST_FOREACH (shared_ptr<Content> i, selected ()) {
		if (!i->text.empty()) {
			sc.push_back (i);
		}
	}

	return sc;
}

FFmpegContentList
ContentPanel::selected_ffmpeg ()
{
	FFmpegContentList sc;

	BOOST_FOREACH (shared_ptr<Content> i, selected ()) {
		shared_ptr<FFmpegContent> t = dynamic_pointer_cast<FFmpegContent> (i);
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
	case Film::CONTENT_ORDER:
		setup ();
		break;
	default:
		break;
	}

	BOOST_FOREACH (ContentSubPanel* i, panels()) {
		i->film_changed (p);
	}
}

void
ContentPanel::item_deselected ()
{
	/* Maybe this is just a re-click on the same item; if not, _ignore_deselect will stay
	   false and item_deselected_foo will handle the deselection.
	*/
	_ignore_deselect = false;
	signal_manager->when_idle (boost::bind (&ContentPanel::item_deselected_idle, this));
}

void
ContentPanel::item_deselected_idle ()
{
	if (!_ignore_deselect) {
		check_selection ();
	}
}

void
ContentPanel::item_selected ()
{
	_ignore_deselect = true;
	check_selection ();
}

void
ContentPanel::check_selection ()
{
	if (_last_selected == selected()) {
		/* This was triggered by a re-build of the view but the selection
		   did not really change.
		*/
		return;
	}

	_last_selected = selected ();

	setup_sensitivity ();

	BOOST_FOREACH (ContentSubPanel* i, panels()) {
		i->content_selection_changed ();
	}

	optional<DCPTime> go_to;
	BOOST_FOREACH (shared_ptr<Content> i, selected()) {
		DCPTime p;
		p = i->position();
		if (dynamic_pointer_cast<StringTextFileContent>(i) && i->paths_valid()) {
			/* Rather special case; if we select a text subtitle file jump to its
			   first subtitle.
			*/
			StringTextFile ts (dynamic_pointer_cast<StringTextFileContent>(i));
			if (ts.first()) {
				p += DCPTime(ts.first().get(), _film->active_frame_rate_change(i->position()));
			}
		}
		if (!go_to || p < go_to.get()) {
			go_to = p;
		}
	}

	if (go_to && Config::instance()->jump_to_selected() && signal_manager) {
		shared_ptr<FilmViewer> fv = _film_viewer.lock ();
		DCPOMATIC_ASSERT (fv);
		signal_manager->when_idle(boost::bind(&FilmViewer::seek, fv.get(), go_to.get().ceil(_film->video_frame_rate()), true));
	}

	if (_timeline_dialog) {
		_timeline_dialog->set_selection (selected());
	}

	/* Make required tabs visible */

	if (_notebook->GetPageCount() > 1) {
		/* There's more than one tab in the notebook so the current selection could be meaningful
		   to the user; store it so that we can try to restore it later.
		*/
		_last_selected_tab = 0;
		if (_notebook->GetSelection() != wxNOT_FOUND) {
			_last_selected_tab = _notebook->GetPage(_notebook->GetSelection());
		}
	}

	bool have_video = false;
	bool have_audio = false;
	bool have_text[TEXT_COUNT] = { false, false };
	BOOST_FOREACH (shared_ptr<Content> i, selected()) {
		if (i->video) {
			have_video = true;
		}
		if (i->audio) {
			have_audio = true;
		}
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			have_text[j->original_type()] = true;
		}
	}

	int off = 0;

	if (have_video && !_video_panel) {
		_video_panel = new VideoPanel (this);
		_notebook->InsertPage (off, _video_panel, _video_panel->name());
	} else if (!have_video && _video_panel) {
		_notebook->DeletePage (off);
		_video_panel = 0;
	}

	if (have_video) {
		++off;
	}

	if (have_audio && !_audio_panel) {
		_audio_panel = new AudioPanel (this);
		_notebook->InsertPage (off, _audio_panel, _audio_panel->name());
	} else if (!have_audio && _audio_panel) {
		_notebook->DeletePage (off);
		_audio_panel = 0;
	}

	if (have_audio) {
		++off;
	}

	for (int i = 0; i < TEXT_COUNT; ++i) {
		if (have_text[i] && !_text_panel[i]) {
			_text_panel[i] = new TextPanel (this, static_cast<TextType>(i));
			_notebook->InsertPage (off, _text_panel[i], _text_panel[i]->name());
		} else if (!have_text[i] && _text_panel[i]) {
			_notebook->DeletePage (off);
			_text_panel[i] = 0;
		}
		if (have_text[i]) {
			++off;
		}
	}

	/* Set up the tab selection */

	bool done = false;
	for (size_t i = 0; i < _notebook->GetPageCount(); ++i) {
		if (_notebook->GetPage(i) == _last_selected_tab) {
			_notebook->SetSelection (i);
			done = true;
		}
	}

	if (!done && _notebook->GetPageCount() > 0) {
		_notebook->SetSelection (0);
	}

	setup_sensitivity ();
	SelectionChanged ();
}

void
ContentPanel::add_file_clicked ()
{
	/* This method is also called when Ctrl-A is pressed, so check that our notebook page
	   is visible.
	*/
	if (_parent->GetCurrentPage() != _splitter || !_film) {
		return;
	}

	/* The wxFD_CHANGE_DIR here prevents a `could not set working directory' error 123 on Windows when using
	   non-Latin filenames or paths.
	*/
	wxFileDialog* d = new wxFileDialog (
		_splitter,
		_("Choose a file or files"),
		wxT (""),
		wxT (""),
		wxT ("All files|*.*|Subtitle files|*.srt;*.xml|Audio files|*.wav;*.w64;*.flac;*.aif;*.aiff"),
		wxFD_MULTIPLE | wxFD_CHANGE_DIR
		);

	int const r = d->ShowModal ();

	if (r != wxID_OK) {
		d->Destroy ();
		return;
	}

	wxArrayString paths;
	d->GetPaths (paths);
	list<boost::filesystem::path> path_list;
	for (unsigned int i = 0; i < paths.GetCount(); ++i) {
		path_list.push_back (wx_to_std (paths[i]));
	}
	add_files (path_list);

	d->Destroy ();
}

void
ContentPanel::add_folder_clicked ()
{
	wxDirDialog* d = new wxDirDialog (_splitter, _("Choose a folder"), wxT(""), wxDD_DIR_MUST_EXIST);
	int r = d->ShowModal ();
	boost::filesystem::path const path (wx_to_std (d->GetPath ()));
	d->Destroy ();

	if (r != wxID_OK) {
		return;
	}

	list<shared_ptr<Content> > content;

	try {
		content = content_factory (path);
	} catch (exception& e) {
		error_dialog (_parent, e.what());
		return;
	}

	if (content.empty ()) {
		error_dialog (_parent, _("No content found in this folder."));
		return;
	}

	BOOST_FOREACH (shared_ptr<Content> i, content) {
		shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (i);
		if (ic) {
			ImageSequenceDialog* e = new ImageSequenceDialog (_splitter);
			r = e->ShowModal ();
			float const frame_rate = e->frame_rate ();
			e->Destroy ();

			if (r != wxID_OK) {
				return;
			}

			ic->set_video_frame_rate (frame_rate);
		}

		_film->examine_and_add_content (i);
	}
}

void
ContentPanel::add_dcp_clicked ()
{
	wxDirDialog* d = new wxDirDialog (_splitter, _("Choose a DCP folder"), wxT(""), wxDD_DIR_MUST_EXIST);
	int r = d->ShowModal ();
	boost::filesystem::path const path (wx_to_std (d->GetPath ()));
	d->Destroy ();

	if (r != wxID_OK) {
		return;
	}

	try {
		_film->examine_and_add_content (shared_ptr<Content> (new DCPContent (path)));
	} catch (exception& e) {
		error_dialog (_parent, e.what());
	}
}

/** @return true if this remove "click" should be ignored */
bool
ContentPanel::remove_clicked (bool hotkey)
{
	/* If the method was called because Delete was pressed check that our notebook page
	   is visible and that the content list is focussed.
	*/
	if (hotkey && (_parent->GetCurrentPage() != _splitter || !_content->HasFocus())) {
		return true;
	}

	BOOST_FOREACH (shared_ptr<Content> i, selected ()) {
		_film->remove_content (i);
	}

	check_selection ();
	return false;
}

void
ContentPanel::timeline_clicked ()
{
	if (!_film) {
		return;
	}

	if (_timeline_dialog) {
		_timeline_dialog->Destroy ();
		_timeline_dialog = 0;
	}

	_timeline_dialog = new TimelineDialog (this, _film);
	_timeline_dialog->set_selection (selected());
	_timeline_dialog->Show ();
}

void
ContentPanel::right_click (wxListEvent& ev)
{
	_menu->popup (_film, selected (), TimelineContentViewList (), ev.GetPoint ());
}

/** Set up broad sensitivity based on the type of content that is selected */
void
ContentPanel::setup_sensitivity ()
{
	_add_file->Enable (_generally_sensitive);
	_add_folder->Enable (_generally_sensitive);
	_add_dcp->Enable (_generally_sensitive);

	ContentList selection = selected ();
	ContentList video_selection = selected_video ();
	ContentList audio_selection = selected_audio ();

	_remove->Enable   (_generally_sensitive && !selection.empty());
	_earlier->Enable  (_generally_sensitive && selection.size() == 1);
	_later->Enable    (_generally_sensitive && selection.size() == 1);
	_timeline->Enable (_generally_sensitive && _film && !_film->content().empty());

	if (_video_panel) {
		_video_panel->Enable (_generally_sensitive && video_selection.size() > 0);
	}
	if (_audio_panel) {
		_audio_panel->Enable (_generally_sensitive && audio_selection.size() > 0);
	}
	for (int i = 0; i < TEXT_COUNT; ++i) {
		if (_text_panel[i]) {
			_text_panel[i]->Enable (_generally_sensitive && selection.size() == 1 && !selection.front()->text.empty());
		}
	}
	_timing_panel->Enable (_generally_sensitive);
}

void
ContentPanel::set_film (shared_ptr<Film> film)
{
	if (_audio_panel) {
		_audio_panel->set_film (film);
	}

	_film = film;

	film_changed (Film::CONTENT);
	film_changed (Film::AUDIO_CHANNELS);

	if (_film) {
		check_selection ();
	}

	setup_sensitivity ();
}

void
ContentPanel::set_general_sensitivity (bool s)
{
	_generally_sensitive = s;
	setup_sensitivity ();
}

void
ContentPanel::earlier_clicked ()
{
	ContentList sel = selected ();
	if (sel.size() == 1) {
		_film->move_content_earlier (sel.front ());
		check_selection ();
	}
}

void
ContentPanel::later_clicked ()
{
	ContentList sel = selected ();
	if (sel.size() == 1) {
		_film->move_content_later (sel.front ());
		check_selection ();
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
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED);
		}
	}
}

void
ContentPanel::set_selection (ContentList cl)
{
	ContentList content = _film->content ();
	for (size_t i = 0; i < content.size(); ++i) {
		if (find(cl.begin(), cl.end(), content[i]) != cl.end()) {
			_content->SetItemState (i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		} else {
			_content->SetItemState (i, 0, wxLIST_STATE_SELECTED);
		}
	}
}

void
ContentPanel::film_content_changed (int property)
{
	if (
		property == ContentProperty::PATH ||
		property == DCPContentProperty::NEEDS_ASSETS ||
		property == DCPContentProperty::NEEDS_KDM ||
		property == DCPContentProperty::NAME
		) {

		setup ();
	}

	BOOST_FOREACH (ContentSubPanel* i, panels()) {
		i->film_content_changed (property);
	}
}

void
ContentPanel::setup ()
{
	if (!_film) {
		_content->DeleteAllItems ();
		setup_sensitivity ();
		return;
	}

	ContentList content = _film->content ();

	Content* selected_content = 0;
	int const s = _content->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s != -1) {
		wxListItem item;
		item.SetId (s);
		item.SetMask (wxLIST_MASK_DATA);
		_content->GetItem (item);
		selected_content = reinterpret_cast<Content*> (item.GetData ());
	}

	_content->DeleteAllItems ();

	BOOST_FOREACH (shared_ptr<Content> i, content) {
		int const t = _content->GetItemCount ();
		bool const valid = i->paths_valid ();

		/* Temporary debugging for Igor */
		BOOST_FOREACH (boost::filesystem::path j, i->paths()) {
			LOG_GENERAL ("Check %1 %2 answer %3", j.string(), boost::filesystem::exists(j) ? "yes" : "no", valid ? "yes" : "no");
		}

		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (i);
		bool const needs_kdm = dcp && dcp->needs_kdm ();
		bool const needs_assets = dcp && dcp->needs_assets ();

		wxString s = std_to_wx (i->summary ());

		if (!valid) {
			s = _("MISSING: ") + s;
		}

		if (needs_kdm) {
			s = _("NEEDS KDM: ") + s;
		}

		if (needs_assets) {
			s = _("NEEDS OV: ") + s;
		}

		wxListItem item;
		item.SetId (t);
		item.SetText (s);
		item.SetData (i.get ());
		_content->InsertItem (item);

		if (i.get() == selected_content) {
			_content->SetItemState (t, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		}

		if (!valid || needs_kdm || needs_assets) {
			_content->SetItemTextColour (t, *wxRED);
		}
	}

	if (!selected_content && !content.empty ()) {
		/* Select the item of content if none was selected before */
		_content->SetItemState (0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}

	setup_sensitivity ();
}

void
ContentPanel::files_dropped (wxDropFilesEvent& event)
{
	if (!_film) {
		return;
	}

	wxString* paths = event.GetFiles ();
	list<boost::filesystem::path> path_list;
	for (int i = 0; i < event.GetNumberOfFiles(); i++) {
		path_list.push_back (wx_to_std (paths[i]));
	}

	add_files (path_list);
}

void
ContentPanel::add_files (list<boost::filesystem::path> paths)
{
	/* It has been reported that the paths returned from e.g. wxFileDialog are not always sorted;
	   I can't reproduce that, but sort them anyway.  Don't use ImageFilenameSorter as a normal
	   alphabetical sort is expected here.
	*/

	paths.sort (CaseInsensitiveSorter ());

	/* XXX: check for lots of files here and do something */

	try {
		BOOST_FOREACH (boost::filesystem::path i, paths) {
			BOOST_FOREACH (shared_ptr<Content> j, content_factory(i)) {
				_film->examine_and_add_content (j);
			}
		}
	} catch (exception& e) {
		error_dialog (_parent, e.what());
	}
}

list<ContentSubPanel*>
ContentPanel::panels () const
{
	list<ContentSubPanel*> p;
	if (_video_panel) {
		p.push_back (_video_panel);
	}
	if (_audio_panel) {
		p.push_back (_audio_panel);
	}
	for (int i = 0; i < TEXT_COUNT; ++i) {
		if (_text_panel[i]) {
			p.push_back (_text_panel[i]);
		}
	}
	p.push_back (_timing_panel);
	return p;
}
