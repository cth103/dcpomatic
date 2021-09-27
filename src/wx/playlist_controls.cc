/*
    Copyright (C) 2018-2020 Carl Hetherington <cth@carlh.net>

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

#include "playlist_controls.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "content_view.h"
#include "dcpomatic_button.h"
#include "static_text.h"
#include "lib/player_video.h"
#include "lib/dcp_content.h"
#include "lib/cross.h"
#include "lib/scoped_temporary.h"
#include "lib/internet.h"
#include "lib/ffmpeg_content.h"
#include "lib/compose.hpp"
#include <dcp/raw_convert.h>
#include <dcp/exceptions.h>
#include <wx/listctrl.h>
#include <wx/progdlg.h>

using std::string;
using std::cout;
using std::exception;
using std::sort;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using boost::optional;
using namespace dcpomatic;

PlaylistControls::PlaylistControls (wxWindow* parent, shared_ptr<FilmViewer> viewer)
	: Controls (parent, viewer, false)
	, _play_button (new Button(this, _("Play")))
	, _pause_button (new Button(this, _("Pause")))
	, _stop_button (new Button(this, _("Stop")))
	, _next_button (new Button(this, "Next"))
	, _previous_button (new Button(this, "Previous"))
{
	_button_sizer->Add (_previous_button, 0, wxEXPAND);
	_button_sizer->Add (_play_button, 0, wxEXPAND);
	_button_sizer->Add (_pause_button, 0, wxEXPAND);
	_button_sizer->Add (_stop_button, 0, wxEXPAND);
	_button_sizer->Add (_next_button, 0, wxEXPAND);

	_spl_view = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 740);

	wxBoxSizer* left_sizer = new wxBoxSizer (wxVERTICAL);
	wxBoxSizer* e_sizer = new wxBoxSizer (wxHORIZONTAL);

	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	wxBoxSizer* spl_header = new wxBoxSizer (wxHORIZONTAL);
	{
		wxStaticText* m = new StaticText (this, "Playlists");
		m->SetFont (subheading_font);
		spl_header->Add (m, 1, wxALIGN_CENTER_VERTICAL);
	}
	_refresh_spl_view = new Button (this, "Refresh");
	spl_header->Add (_refresh_spl_view, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP / 2);

	left_sizer->Add (spl_header, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	left_sizer->Add (_spl_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_content_view = new ContentView (this);

	wxBoxSizer* content_header = new wxBoxSizer (wxHORIZONTAL);
	{
		wxStaticText* m = new StaticText (this, "Content");
		m->SetFont (subheading_font);
		content_header->Add (m, 1, wxALIGN_CENTER_VERTICAL);
	}
	_refresh_content_view = new Button (this, "Refresh");
	content_header->Add (_refresh_content_view, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP / 2);

	left_sizer->Add (content_header, 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	left_sizer->Add (_content_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_current_spl_view = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_current_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 500);
	_current_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 80);
	e_sizer->Add (left_sizer, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	e_sizer->Add (_current_spl_view, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_v_sizer->Add (e_sizer, 1, wxEXPAND);

	_play_button->Bind     (wxEVT_BUTTON, boost::bind(&PlaylistControls::play_clicked,  this));
	_pause_button->Bind    (wxEVT_BUTTON, boost::bind(&PlaylistControls::pause_clicked, this));
	_stop_button->Bind     (wxEVT_BUTTON, boost::bind(&PlaylistControls::stop_clicked,  this));
	_next_button->Bind     (wxEVT_BUTTON, boost::bind(&PlaylistControls::next_clicked,  this));
	_previous_button->Bind (wxEVT_BUTTON, boost::bind(&PlaylistControls::previous_clicked,  this));
	_spl_view->Bind        (wxEVT_LIST_ITEM_SELECTED,   boost::bind(&PlaylistControls::spl_selection_changed, this));
	_spl_view->Bind        (wxEVT_LIST_ITEM_DESELECTED, boost::bind(&PlaylistControls::spl_selection_changed, this));
	viewer->Finished.connect (boost::bind(&PlaylistControls::viewer_finished, this));
	_refresh_spl_view->Bind (wxEVT_BUTTON, boost::bind(&PlaylistControls::update_playlist_directory, this));
	_refresh_content_view->Bind (wxEVT_BUTTON, boost::bind(&ContentView::update, _content_view));

	_content_view->update ();
	update_playlist_directory ();
}

void
PlaylistControls::started ()
{
	Controls::started ();
	_play_button->Enable (false);
	_pause_button->Enable (true);
}

/** Called when the viewer finishes a single piece of content, or it is explicitly stopped */
void
PlaylistControls::stopped ()
{
	Controls::stopped ();
	_play_button->Enable (true);
	_pause_button->Enable (false);
}

void
PlaylistControls::deselect_playlist ()
{
	long int const selected = _spl_view->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected != -1) {
		_selected_playlist = boost::none;
		_spl_view->SetItemState (selected, 0, wxLIST_STATE_SELECTED);
	}
	ResetFilm (shared_ptr<Film>(new Film(optional<boost::filesystem::path>())));
}

void
PlaylistControls::play_clicked ()
{
	auto viewer = _viewer.lock ();
	if (viewer) {
		viewer->start ();
	}
}

void
PlaylistControls::setup_sensitivity ()
{
	auto viewer = _viewer.lock ();
	if (!viewer) {
		return;
	}

	Controls::setup_sensitivity ();
	bool const active_job = _active_job && *_active_job != "examine_content";
	bool const c = _film && !_film->content().empty() && !active_job;
	_play_button->Enable (c && !viewer->playing());
	_pause_button->Enable (viewer->playing());
	_spl_view->Enable (!viewer->playing());
	_next_button->Enable (can_do_next());
	_previous_button->Enable (can_do_previous());
}

void
PlaylistControls::pause_clicked ()
{
	auto viewer = _viewer.lock ();
	if (viewer) {
		viewer->stop ();
	}
}

void
PlaylistControls::stop_clicked ()
{
	auto viewer = _viewer.lock ();
	if (!viewer) {
		return;
	}

	viewer->stop ();
	viewer->seek (DCPTime(), true);
	if (_selected_playlist) {
		_selected_playlist_position = 0;
		update_current_content ();
	}
	deselect_playlist ();
}

bool
PlaylistControls::can_do_previous ()
{
	return _selected_playlist && (_selected_playlist_position - 1) >= 0;
}

void
PlaylistControls::previous_clicked ()
{
	if (!can_do_previous ()) {
		return;
	}

	_selected_playlist_position--;
	update_current_content ();
}

bool
PlaylistControls::can_do_next ()
{
	return _selected_playlist && (_selected_playlist_position + 1) < int(_playlists[*_selected_playlist].get().size());
}

void
PlaylistControls::next_clicked ()
{
	if (!can_do_next ()) {
		return;
	}

	_selected_playlist_position++;
	update_current_content ();
}

void
PlaylistControls::log (wxString s)
{
	optional<boost::filesystem::path> log = Config::instance()->player_activity_log_file();
	if (!log) {
		return;
	}

	struct timeval time;
	gettimeofday (&time, 0);
	char buffer[64];
	time_t const sec = time.tv_sec;
	struct tm* t = localtime (&sec);
	strftime (buffer, 64, "%c", t);
	wxString ts = std_to_wx(string(buffer)) + N_(": ");
	FILE* f = fopen_boost (*log, "a");
	if (!f) {
		return;
	}
	fprintf (f, "%s%s\n", wx_to_std(ts).c_str(), wx_to_std(s).c_str());
	fclose (f);
}

void
PlaylistControls::add_playlist_to_list (SPL spl)
{
	int const N = _spl_view->GetItemCount();

	wxListItem it;
	it.SetId(N);
	it.SetColumn(0);
	string t = spl.name();
	if (spl.missing()) {
		t += " (content missing)";
	}
	it.SetText (std_to_wx(t));
	_spl_view->InsertItem (it);
}

struct SPLComparator
{
	bool operator() (SPL const & a, SPL const & b) {
		return a.name() < b.name();
	}
};

void
PlaylistControls::update_playlist_directory ()
{
	using namespace boost::filesystem;

	_spl_view->DeleteAllItems ();
	optional<path> dir = Config::instance()->player_playlist_directory();
	if (!dir) {
		return;
	}

	_playlists.clear ();

	for (directory_iterator i = directory_iterator(*dir); i != directory_iterator(); ++i) {
		try {
			if (is_regular_file(i->path()) && i->path().extension() == ".xml") {
				SPL spl;
				spl.read (i->path(), _content_view);
				_playlists.push_back (spl);
			}
		} catch (exception& e) {
			/* Never mind */
		}
	}

	sort (_playlists.begin(), _playlists.end(), SPLComparator());
	for (auto i: _playlists) {
		add_playlist_to_list (i);
	}

	_selected_playlist = boost::none;
}

optional<dcp::EncryptedKDM>
PlaylistControls::get_kdm_from_directory (shared_ptr<DCPContent> dcp)
{
	using namespace boost::filesystem;
	optional<path> kdm_dir = Config::instance()->player_kdm_directory();
	if (!kdm_dir) {
		return optional<dcp::EncryptedKDM>();
	}
	for (directory_iterator i = directory_iterator(*kdm_dir); i != directory_iterator(); ++i) {
		try {
			if (file_size(i->path()) < MAX_KDM_SIZE) {
				dcp::EncryptedKDM kdm (dcp::file_to_string(i->path()));
				if (kdm.cpl_id() == dcp->cpl()) {
					return kdm;
				}
			}
		} catch (std::exception& e) {
			/* Hey well */
		}
	}
	return optional<dcp::EncryptedKDM>();
}

void
PlaylistControls::spl_selection_changed ()
{
	long int selected = _spl_view->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1) {
		_current_spl_view->DeleteAllItems ();
		_selected_playlist = boost::none;
		return;
	}

	if (_playlists[selected].missing()) {
		error_dialog (this, "This playlist cannot be loaded as some content is missing.");
		deselect_playlist ();
		return;
	}

	if (_playlists[selected].get().empty()) {
		error_dialog (this, "This playlist is empty.");
		return;
	}

	select_playlist (selected, 0);
}

void
PlaylistControls::select_playlist (int selected, int position)
{
	log (wxString::Format("load-playlist %s", std_to_wx(_playlists[selected].name()).data()));

	wxProgressDialog dialog (_("DCP-o-matic"), "Loading playlist and KDMs");

	for (auto const& i: _playlists[selected].get()) {
		dialog.Pulse ();
		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (i.content);
		if (dcp && dcp->needs_kdm()) {
			optional<dcp::EncryptedKDM> kdm;
			kdm = get_kdm_from_directory (dcp);
			if (kdm) {
				try {
					dcp->add_kdm (*kdm);
					dcp->examine (_film, shared_ptr<Job>());
				} catch (KDMError& e) {
					error_dialog (this, "Could not load KDM.");
				}
			}
			if (dcp->needs_kdm()) {
				/* We didn't get a KDM for this */
				error_dialog (this, "This playlist cannot be loaded as a KDM is missing or incorrect.");
				deselect_playlist ();
				return;
			}
		}
	}

	_current_spl_view->DeleteAllItems ();

	int N = 0;
	for (auto i: _playlists[selected].get()) {
		wxListItem it;
		it.SetId (N);
		it.SetColumn (0);
		it.SetText (std_to_wx(i.name));
		_current_spl_view->InsertItem (it);
		++N;
	}

	_selected_playlist = selected;
	_selected_playlist_position = position;
 	dialog.Pulse ();
	reset_film ();
 	dialog.Pulse ();
	update_current_content ();
}

void
PlaylistControls::reset_film ()
{
	DCPOMATIC_ASSERT (_selected_playlist);
	shared_ptr<Film> film (new Film(optional<boost::filesystem::path>()));
	film->add_content (_playlists[*_selected_playlist].get()[_selected_playlist_position].content);
	ResetFilm (film);
}

void
PlaylistControls::config_changed (int property)
{
	Controls::config_changed (property);

	if (property == Config::PLAYER_CONTENT_DIRECTORY) {
		_content_view->update ();
	} else if (property == Config::PLAYER_PLAYLIST_DIRECTORY) {
		update_playlist_directory ();
	}
}

void
PlaylistControls::set_film (shared_ptr<Film> film)
{
	Controls::set_film (film);
	setup_sensitivity ();
}

void
PlaylistControls::update_current_content ()
{
	DCPOMATIC_ASSERT (_selected_playlist);

	wxProgressDialog dialog (_("DCP-o-matic"), "Loading content");

	setup_sensitivity ();
	dialog.Pulse ();
	reset_film ();
}

/** One piece of content in our SPL has finished playing */
void
PlaylistControls::viewer_finished ()
{
	auto viewer = _viewer.lock ();
	if (!_selected_playlist || !viewer) {
		return;
	}

	_selected_playlist_position++;
	if (_selected_playlist_position < int(_playlists[*_selected_playlist].get().size())) {
		/* Next piece of content on the SPL */
		update_current_content ();
		viewer->start ();
	} else {
		/* Finished the whole SPL */
		_selected_playlist_position = 0;
		ResetFilm (shared_ptr<Film>(new Film(optional<boost::filesystem::path>())));
		_play_button->Enable (true);
		_pause_button->Enable (false);
	}
}

void
PlaylistControls::play ()
{
	play_clicked ();
}

void
PlaylistControls::stop ()
{
	stop_clicked ();
}
