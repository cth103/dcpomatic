/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "swaroop_controls.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "content_view.h"
#include "dcpomatic_button.h"
#include "lib/player_video.h"
#include "lib/dcp_content.h"
#include <wx/listctrl.h>
#include <wx/progdlg.h>

using std::string;
using std::cout;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

SwaroopControls::SwaroopControls (wxWindow* parent, shared_ptr<FilmViewer> viewer)
	: Controls (parent, viewer, false)
	, _play_button (new Button(this, _("Play")))
	, _pause_button (new Button(this, _("Pause")))
	, _stop_button (new Button(this, _("Stop")))
	, _current_disable_timeline (false)
{
	_button_sizer->Add (_play_button, 0, wxEXPAND);
	_button_sizer->Add (_pause_button, 0, wxEXPAND);
	_button_sizer->Add (_stop_button, 0, wxEXPAND);

	_spl_view = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 740);

	wxBoxSizer* left_sizer = new wxBoxSizer (wxVERTICAL);
	wxBoxSizer* e_sizer = new wxBoxSizer (wxHORIZONTAL);

	left_sizer->Add (_spl_view, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_content_view = new ContentView (this);
	left_sizer->Add (_content_view, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_current_spl_view = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_current_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 500);
	_current_spl_view->AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 80);
	e_sizer->Add (left_sizer, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	e_sizer->Add (_current_spl_view, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_v_sizer->Add (e_sizer, 1, wxEXPAND);

	_log = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(-1, 200), wxTE_READONLY | wxTE_MULTILINE);
	_v_sizer->Add (_log, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_play_button->Bind  (wxEVT_BUTTON, boost::bind(&SwaroopControls::play_clicked,  this));
	_pause_button->Bind (wxEVT_BUTTON, boost::bind(&SwaroopControls::pause_clicked, this));
	_stop_button->Bind  (wxEVT_BUTTON, boost::bind(&SwaroopControls::stop_clicked,  this));
	_spl_view->Bind     (wxEVT_LIST_ITEM_SELECTED,   boost::bind(&SwaroopControls::spl_selection_changed, this));
	_spl_view->Bind     (wxEVT_LIST_ITEM_DESELECTED, boost::bind(&SwaroopControls::spl_selection_changed, this));
	_viewer->ImageChanged.connect (boost::bind(&SwaroopControls::image_changed, this, _1));

	_content_view->update ();
	update_playlist_directory ();
}

void
SwaroopControls::started ()
{
	Controls::started ();
	_play_button->Enable (false);
	_pause_button->Enable (true);
}

void
SwaroopControls::stopped ()
{
	Controls::stopped ();
	_play_button->Enable (true);
	_pause_button->Enable (false);
}

void
SwaroopControls::play_clicked ()
{
	_viewer->start ();
}

void
SwaroopControls::setup_sensitivity ()
{
	Controls::setup_sensitivity ();
	bool const active_job = _active_job && *_active_job != "examine_content";
	bool const c = _film && !_film->content().empty() && !active_job;
	_play_button->Enable (c && !_viewer->playing());
	_pause_button->Enable (c && (!_current_kind || _current_kind != dcp::ADVERTISEMENT) && _viewer->playing());
	_stop_button->Enable (c && (!_current_kind || _current_kind != dcp::ADVERTISEMENT));
	_slider->Enable (c && (!_current_kind || _current_kind != dcp::ADVERTISEMENT) && !_current_disable_timeline);
	_spl_view->Enable (!_viewer->playing());
}

void
SwaroopControls::pause_clicked ()
{
	_viewer->stop ();
}

void
SwaroopControls::stop_clicked ()
{
	_viewer->stop ();
	_viewer->seek (DCPTime(), true);
}

void
SwaroopControls::log (wxString s)
{
	struct timeval time;
	gettimeofday (&time, 0);
	char buffer[64];
	time_t const sec = time.tv_sec;
	struct tm* t = localtime (&sec);
	strftime (buffer, 64, "%c", t);
	wxString ts = std_to_wx(string(buffer)) + N_(": ");
	_log->SetValue(_log->GetValue() + ts + s + "\n");
}

void
SwaroopControls::image_changed (boost::weak_ptr<PlayerVideo> weak_pv)
{
	shared_ptr<PlayerVideo> pv = weak_pv.lock ();
	if (!pv) {
		return;
	}

	shared_ptr<Content> c = pv->content().lock();
	if (!c) {
		return;
	}

	if (c == _current_content.lock()) {
		return;
	}

	_current_content = c;

	if (_selected_playlist) {
		BOOST_FOREACH (SPLEntry i, _playlists[*_selected_playlist].get()) {
			if (i.content == c) {
				_current_disable_timeline = i.disable_timeline;
				setup_sensitivity ();
			}
		}
	}

	shared_ptr<DCPContent> dc = dynamic_pointer_cast<DCPContent> (c);
	if (!dc) {
		return;
	}

	if (!_current_kind || *_current_kind != dc->content_kind()) {
		_current_kind = dc->content_kind ();
		setup_sensitivity ();
	}
}

void
SwaroopControls::add_playlist_to_list (SPL spl)
{
	int const N = _spl_view->GetItemCount();

	wxListItem it;
	it.SetId(N);
	it.SetColumn(0);
	it.SetText (std_to_wx(spl.name()));
	_spl_view->InsertItem (it);
}

void
SwaroopControls::update_playlist_directory ()
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
				add_playlist_to_list (spl);
			}
		} catch (exception& e) {
			/* Never mind */
		}
	}
}

void
SwaroopControls::spl_selection_changed ()
{
	_current_spl_view->DeleteAllItems ();

	long int selected = _spl_view->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1) {
		_selected_playlist = boost::none;
		return;
	}

	wxProgressDialog progress (_("DCP-o-matic"), _("Loading playlist"));

	shared_ptr<Film> film (new Film(optional<boost::filesystem::path>()));

	int N = 0;
	BOOST_FOREACH (SPLEntry i, _playlists[selected].get()) {
		wxListItem it;
		it.SetId (N);
		it.SetColumn (0);
		it.SetText (std_to_wx(i.name));
		_current_spl_view->InsertItem (it);
		film->add_content (i.content);
		++N;
		if (!progress.Pulse()) {
			/* user pressed cancel */
			return;
		}
	}

	_selected_playlist = selected;
	ResetFilm (film);
}

void
SwaroopControls::config_changed (int property)
{
	Controls::config_changed (property);

	if (property == Config::PLAYER_CONTENT_DIRECTORY) {
		_content_view->update ();
	} else if (property == Config::PLAYER_PLAYLIST_DIRECTORY) {
		update_playlist_directory ();
	}
}

void
SwaroopControls::set_film (shared_ptr<Film> film)
{
	Controls::set_film (film);
	setup_sensitivity ();
}
