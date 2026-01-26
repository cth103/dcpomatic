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


#include "content_view.h"
#include "dcpomatic_button.h"
#include "film_viewer.h"
#include "player_frame.h"
#include "playlist_controls.h"
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/internet.h"
#include "lib/player_video.h"
#include "lib/scoped_temporary.h"
#include "lib/show_playlist.h"
#include "lib/show_playlist_list.h"
#include <dcp/exceptions.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/listctrl.h>
#include <wx/progdlg.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::shared_ptr;
using std::sort;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;
using namespace dcpomatic::ui;


PlaylistControls::PlaylistControls(wxWindow* parent, PlayerFrame* player, FilmViewer& viewer)
	: Controls(parent, viewer, false)
	, _player(player)
	, _play_button(new Button(this, _("Play")))
	, _pause_button(new Button(this, _("Pause")))
	, _stop_button(new Button(this, _("Stop")))
	, _next_button(new Button(this, _("Next")))
	, _previous_button(new Button(this, _("Previous")))
{
	_button_sizer->Add(_previous_button, 0, wxEXPAND);
	_button_sizer->Add(_play_button, 0, wxEXPAND);
	_button_sizer->Add(_pause_button, 0, wxEXPAND);
	_button_sizer->Add(_stop_button, 0, wxEXPAND);
	_button_sizer->Add(_next_button, 0, wxEXPAND);

	_playlists_view = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_playlists_view->AppendColumn({}, wxLIST_FORMAT_LEFT, 740);

	auto left_sizer = new wxBoxSizer(wxVERTICAL);
	auto h_sizer = new wxBoxSizer(wxHORIZONTAL);

	wxFont subheading_font(*wxNORMAL_FONT);
	subheading_font.SetWeight(wxFONTWEIGHT_BOLD);

	auto playlists_header = new wxBoxSizer(wxHORIZONTAL);
	{
		auto m = new StaticText(this, _("Playlists"));
		m->SetFont(subheading_font);
		playlists_header->Add(m, 1, wxALIGN_CENTER_VERTICAL);
	}
	_refresh_playlists_view = new Button(this, _("Refresh"));
	playlists_header->Add(_refresh_playlists_view, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP / 2);

	left_sizer->Add(playlists_header, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	left_sizer->Add(_playlists_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_content_view = new ContentView(this);

	auto content_header = new wxBoxSizer(wxHORIZONTAL);
	{
		auto m = new StaticText(this, _("Content"));
		m->SetFont(subheading_font);
		content_header->Add(m, 1, wxALIGN_CENTER_VERTICAL);
	}
	_refresh_content_view = new Button(this, _("Refresh"));
	content_header->Add(_refresh_content_view, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP / 2);

	left_sizer->Add(content_header, 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	left_sizer->Add(_content_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	auto right_sizer = new wxBoxSizer(wxVERTICAL);

	_next_playlist_view = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_next_playlist_view->AppendColumn({}, wxLIST_FORMAT_LEFT, 600);
	_next_playlist_view->AppendColumn({}, wxLIST_FORMAT_LEFT, 80);

	auto next_playlist_header = new wxBoxSizer(wxHORIZONTAL);
	{
		auto m = new StaticText(this, _("Next playlist"));
		m->SetFont(subheading_font);
		next_playlist_header->Add(m, 1, wxALIGN_CENTER_VERTICAL);
	}
	_clear_next_playlist = new wxButton(this, wxID_ANY, _("Clear"));
	next_playlist_header->Add(_clear_next_playlist, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP / 2);

	right_sizer->Add(next_playlist_header, 0, wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	right_sizer->Add(_next_playlist_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	_current_playlist_view = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);
	_current_playlist_view->AppendColumn({}, wxLIST_FORMAT_LEFT, 600);
	_current_playlist_view->AppendColumn({}, wxLIST_FORMAT_LEFT, 80);

	auto current_playlist_header = new wxBoxSizer(wxHORIZONTAL);
	{
		auto m = new StaticText(this, _("Current playlist"));
		m->SetFont(subheading_font);
		current_playlist_header->Add(m, 1, wxALIGN_CENTER_VERTICAL);
	}

	right_sizer->Add(current_playlist_header, 0, wxTOP | wxBOTTOM | wxLEFT | wxRIGHT | wxEXPAND, DCPOMATIC_SIZER_GAP);
	right_sizer->Add(_current_playlist_view, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, DCPOMATIC_SIZER_GAP);

	h_sizer->Add(left_sizer, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	h_sizer->Add(right_sizer, 1, wxALL | wxEXPAND, DCPOMATIC_SIZER_GAP);
	_v_sizer->Add(h_sizer, 1, wxEXPAND);

	_play_button->Bind    (wxEVT_BUTTON, boost::bind(&PlaylistControls::play_clicked,  this));
	_pause_button->Bind   (wxEVT_BUTTON, boost::bind(&PlaylistControls::pause_clicked, this));
	_stop_button->Bind    (wxEVT_BUTTON, boost::bind(&PlaylistControls::stop_clicked,  this));
	_next_button->Bind    (wxEVT_BUTTON, boost::bind(&PlaylistControls::next_clicked,  this));
	_previous_button->Bind(wxEVT_BUTTON, boost::bind(&PlaylistControls::previous_clicked,  this));
	_playlists_view->Bind       (wxEVT_LIST_ITEM_SELECTED,   boost::bind(&PlaylistControls::playlist_selection_changed, this));
	_playlists_view->Bind       (wxEVT_LIST_ITEM_DESELECTED, boost::bind(&PlaylistControls::playlist_selection_changed, this));
	_refresh_playlists_view->Bind(wxEVT_BUTTON, boost::bind(&PlaylistControls::update_playlists, this));
	_refresh_content_view->Bind(wxEVT_BUTTON, boost::bind(&ContentView::update, _content_view));
	_clear_next_playlist->Bind(wxEVT_BUTTON, boost::bind(&PlaylistControls::clear_next_playlist, this));

	_content_view->Activated.connect(boost::bind(&PlaylistControls::content_activated, this, _1));

	update_playlists();
	_content_view->update();
}


void
PlaylistControls::clear_next_playlist()
{
	_next_playlist_view->DeleteAllItems();
	_next_playlist.clear();
}


void
PlaylistControls::content_activated(weak_ptr<Content> weak_content)
{
	if (auto content = weak_content.lock()) {
		add_next_playlist_entry(ShowPlaylistEntry(content, {}));
	}
}


void
PlaylistControls::started()
{
	Controls::started();
	setup_sensitivity();
}


/** Called when the viewer finishes a single piece of content, or it is explicitly stopped */
void
PlaylistControls::stopped()
{
	Controls::stopped();
	setup_sensitivity();
}


void
PlaylistControls::play_clicked()
{
	if (_player->set_playlist(_next_playlist)) {
		_viewer.start();
	}
}


void
PlaylistControls::setup_sensitivity()
{
	Controls::setup_sensitivity();
	_play_button->Enable(!_viewer.playing() && !_paused && !_next_playlist.empty());
	_pause_button->Enable(_viewer.playing() || _paused);
	_stop_button->Enable(_viewer.playing() || _paused);
	_next_button->Enable(_player->can_do_next());
	_previous_button->Enable(_player->can_do_previous());
}


void
PlaylistControls::pause_clicked()
{
	if (_paused) {
		_viewer.start();
		_paused = false;
	} else {
		_viewer.stop();
		_paused = true;
	}
	setup_sensitivity();
}


void
PlaylistControls::stop_clicked()
{
	_paused = false;
	_viewer.stop();
	_viewer.seek(DCPTime(), true);
	_player->set_playlist({});
}


void
PlaylistControls::previous_clicked()
{
	_player->previous();
}


void
PlaylistControls::next_clicked()
{
	_player->next();
}


void
PlaylistControls::add_playlist_to_list(ShowPlaylist playlist)
{
	int const N = _playlists_view->GetItemCount();

	wxListItem it;
	it.SetId(N);
	it.SetColumn(0);
	auto id = _playlists->get_show_playlist_id(playlist.uuid());
	DCPOMATIC_ASSERT(id);
	it.SetData(id->get());
	string t = playlist.name();

	if (_playlists->missing(playlist.uuid())) {
		t += " (content missing)";
	}
	it.SetText(std_to_wx(t));
	_playlists_view->InsertItem(it);
}


void
PlaylistControls::update_playlists()
{
	using namespace boost::filesystem;

	_playlists_view->DeleteAllItems();
	_playlists.reset(new ShowPlaylistList());

	for (auto i: _playlists->show_playlists()) {
		add_playlist_to_list(i.second);
	}
}


optional<dcp::EncryptedKDM>
PlaylistControls::get_kdm_from_directory(shared_ptr<DCPContent> dcp)
{
	using namespace boost::filesystem;
	optional<path> kdm_dir = Config::instance()->player_kdm_directory();
	if (!kdm_dir) {
		return optional<dcp::EncryptedKDM>();
	}
	for (auto i: directory_iterator(*kdm_dir)) {
		try {
			if (file_size(i.path()) < MAX_KDM_SIZE) {
				dcp::EncryptedKDM kdm(dcp::file_to_string(i.path()));
				if (kdm.cpl_id() == dcp->cpl()) {
					return kdm;
				}
			}
		} catch (std::exception& e) {
			/* Hey well */
		}
	}

	return {};
}


void
PlaylistControls::add_next_playlist_entry(ShowPlaylistEntry entry)
{
	wxListItem it;
	it.SetId(_next_playlist_view->GetItemCount());
	it.SetColumn(0);
	it.SetText(std_to_wx(entry.name()));
	_next_playlist_view->InsertItem(it);
	_next_playlist.push_back(entry);

	setup_sensitivity();
}


void
PlaylistControls::playlist_selection_changed()
{
	long int selected = _playlists_view->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected == -1) {
		return;
	}

	auto const id = ShowPlaylistID(_playlists_view->GetItemData(selected));

	if (_playlists->missing(id)) {
		error_dialog(this, _("This playlist cannot be loaded as some content is missing."));
		return;
	}

	if (_playlists->entries(id).empty()) {
		error_dialog(this, _("This playlist is empty."));
		return;
	}

	_next_playlist_view->DeleteAllItems();
	_next_playlist.clear();
	for (auto const& i: _playlists->entries(id)) {
		add_next_playlist_entry(i);
	}
}


void
PlaylistControls::config_changed(int property)
{
	Controls::config_changed(property);

	if (property == Config::PLAYER_CONTENT_DIRECTORY) {
		_content_view->update();
	} else if (property == Config::SHOW_PLAYLISTS_FILE) {
		update_playlists();
	}
}


void
PlaylistControls::playlist_changed()
{
	_current_playlist_view->DeleteAllItems();

	int N = 0;
	for (auto content: _player->playlist()) {
		wxListItem it;
		it.SetId(N++);
		it.SetColumn(0);
		ShowPlaylistEntry entry(content, {});
		it.SetText(std_to_wx(entry.name()));
		_current_playlist_view->InsertItem(it);
	}
}

