/*
    Copyright (C) 2013-2024 Carl Hetherington <cth@carlh.net>

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


#include "auto_crop_dialog.h"
#include "content_advanced_dialog.h"
#include "content_menu.h"
#include "content_properties_dialog.h"
#include "content_timeline_audio_view.h"
#include "content_timeline_video_view.h"
#include "dir_dialog.h"
#include "file_dialog.h"
#include "film_viewer.h"
#include "id.h"
#include "repeat_dialog.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/constants.h"
#include "lib/copy_dcp_details_to_film.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/examine_content_job.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/find_missing.h"
#include "lib/guess_crop.h"
#include "lib/image_content.h"
#include "lib/job_manager.h"
#include "lib/playlist.h"
#include "lib/video_content.h"
#include <dcp/cpl.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <dcp/search.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/dirdlg.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


enum {
	ID_repeat = DCPOMATIC_CONTENT_MENU,
	ID_join,
	ID_find_missing,
	ID_properties,
	ID_advanced,
	ID_re_examine,
	ID_auto_crop,
	ID_copy_settings,
	ID_kdm,
	ID_ov,
	ID_choose_cpl,
	ID_set_dcp_settings,
	ID_set_dcp_markers,
	ID_remove
};


ContentMenu::ContentMenu(wxWindow* p, FilmViewer& viewer)
	: _menu (new wxMenu)
	, _parent (p)
	, _pop_up_open (false)
	, _viewer (viewer)
{
	_repeat = _menu->Append (ID_repeat, _("Repeat..."));
	_join = _menu->Append (ID_join, _("Join"));
	_find_missing = _menu->Append (ID_find_missing, _("Find missing..."));
	_re_examine = _menu->Append (ID_re_examine, _("Re-examine..."));
	_auto_crop = _menu->Append (ID_auto_crop, _("Auto-crop..."));
	_copy_settings = _menu->Append(ID_copy_settings, _("Copy settings from another project..."));
	_properties = _menu->Append (ID_properties, _("Properties..."));
	_advanced = _menu->Append (ID_advanced, _("Advanced settings..."));
	_menu->AppendSeparator ();
	_kdm = _menu->Append (ID_kdm, _("Add KDM..."));
	_ov = _menu->Append (ID_ov, _("Add OV..."));
	_cpl_menu = new wxMenu ();
	_choose_cpl = _menu->Append (ID_choose_cpl, _("Choose CPL..."), _cpl_menu);
	_set_dcp_settings = _menu->Append (ID_set_dcp_settings, _("Set project DCP settings from this DCP"));
	_set_dcp_markers = _menu->Append(ID_set_dcp_markers, _("Set project markers from this DCP"));
	_menu->AppendSeparator ();
	_remove = _menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::repeat, this), ID_repeat);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::join, this), ID_join);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::find_missing, this), ID_find_missing);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::properties, this), ID_properties);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::advanced, this), ID_advanced);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::re_examine, this), ID_re_examine);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::auto_crop, this), ID_auto_crop);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::copy_settings, this), ID_copy_settings);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::kdm, this), ID_kdm);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::ov, this), ID_ov);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::set_dcp_settings, this), ID_set_dcp_settings);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::set_dcp_markers, this), ID_set_dcp_markers);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::remove, this), ID_remove);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::cpl_selected, this, _1), 1, ID_repeat - 1);
}

void
ContentMenu::popup (weak_ptr<Film> film, ContentList c, TimelineContentViewList v, wxPoint p)
{
	_film = film;
	_content = c;
	_views = v;

	int const N = _cpl_menu->GetMenuItemCount();
	for (int i = DCPOMATIC_CPL_MENU; i < DCPOMATIC_CPL_MENU + N; ++i) {
		_cpl_menu->Delete (i);
	}

	_repeat->Enable (!_content.empty ());

	int n = 0;
	for (auto i: _content) {
		if (dynamic_pointer_cast<FFmpegContent> (i)) {
			++n;
		}
	}

	_join->Enable (n > 1);

	_find_missing->Enable(_content.size() == 1 && (!paths_exist(_content.front()->paths()) || !paths_exist(_content.front()->font_paths())));
	_properties->Enable (_content.size() == 1);
	_advanced->Enable (_content.size() == 1);
	_re_examine->Enable (!_content.empty ());
	_auto_crop->Enable (_content.size() == 1);
	_copy_settings->Enable(_content.size() == 1);

	if (_content.size() == 1) {
		auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
		if (dcp) {
			_kdm->Enable (dcp->encrypted ());
			_ov->Enable (dcp->needs_assets ());
			_set_dcp_settings->Enable (static_cast<bool>(dcp));
			_set_dcp_markers->Enable(static_cast<bool>(dcp));
			try {
				auto cpls = dcp::find_and_resolve_cpls (dcp->directories(), true);
				_choose_cpl->Enable (cpls.size() > 1);
				int id = DCPOMATIC_CPL_MENU;
				for (auto i: cpls) {
					auto item = _cpl_menu->AppendRadioItem (
						id++,
						wxString::Format (
							char_to_wx("%s (%s)"),
							std_to_wx(i->annotation_text().get_value_or("")).data(),
							std_to_wx(i->id()).data()
							)
						);
					item->Check (dcp->cpl() && dcp->cpl() == i->id());
				}
			} catch (dcp::ReadError &) {
				/* The DCP is probably missing */
			} catch (dcp::KDMDecryptionError &) {
				/* We have an incorrect KDM */
			} catch (KDMError &) {
				/* We have an incorrect KDM */
			}
		} else {
			_kdm->Enable (false);
			_ov->Enable (false);
			_choose_cpl->Enable (false);
			_set_dcp_settings->Enable (false);
			_set_dcp_markers->Enable(false);
		}
	} else {
		_kdm->Enable (false);
		_ov->Enable(false);
		_set_dcp_settings->Enable (false);
		_set_dcp_markers->Enable(false);
	}

	_remove->Enable (!_content.empty ());

	_pop_up_open = true;
	_parent->PopupMenu (_menu, p);
	_pop_up_open = false;
}


void
ContentMenu::set_dcp_settings ()
{
	auto film = _film.lock ();
	if (!film) {
		return;
	}

	DCPOMATIC_ASSERT (_content.size() == 1);
	auto dcp = dynamic_pointer_cast<DCPContent>(_content.front());
	DCPOMATIC_ASSERT (dcp);
	copy_dcp_settings_to_film(dcp, film);
}


void
ContentMenu::set_dcp_markers()
{
	auto film = _film.lock();
	if (!film) {
		return;
	}

	DCPOMATIC_ASSERT(_content.size() == 1);
	auto dcp = dynamic_pointer_cast<DCPContent>(_content.front());
	DCPOMATIC_ASSERT(dcp);
	copy_dcp_markers_to_film(dcp, film);
}


void
ContentMenu::repeat ()
{
	if (_content.empty ()) {
		return;
	}

	RepeatDialog dialog(_parent);
	if (dialog.ShowModal() != wxID_OK) {
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	film->repeat_content(_content, dialog.number());

	_content.clear ();
	_views.clear ();
}


void
ContentMenu::join ()
{
	vector<shared_ptr<Content>> fc;
	for (auto i: _content) {
		auto f = dynamic_pointer_cast<FFmpegContent> (i);
		if (f) {
			fc.push_back (f);
		}
	}

	DCPOMATIC_ASSERT (fc.size() > 1);

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	try {
		auto joined = make_shared<FFmpegContent>(fc);
		film->remove_content (_content);
		film->examine_and_add_content (joined);
	} catch (JoinError& e) {
		error_dialog (_parent, std_to_wx (e.what ()));
	}
}


void
ContentMenu::remove ()
{
	if (_content.empty ()) {
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	/* We are removing from the timeline if _views is not empty */
	bool handled = false;
	if (!_views.empty ()) {
		/* Special case: we only remove FFmpegContent if its video view is selected;
		   if not, and its audio view is selected, we unmap the audio.
		*/
		for (auto i: _content) {
			auto fc = dynamic_pointer_cast<FFmpegContent> (i);
			if (!fc) {
				continue;
			}

			shared_ptr<ContentTimelineVideoView> video;
			shared_ptr<ContentTimelineAudioView> audio;

			for (auto j: _views) {
				auto v = dynamic_pointer_cast<ContentTimelineVideoView>(j);
				auto a = dynamic_pointer_cast<ContentTimelineAudioView>(j);
				if (v && v->content() == fc) {
					video = v;
				} else if (a && a->content() == fc) {
					audio = a;
				}
			}

			if (!video && audio) {
				auto m = fc->audio->mapping ();
				m.unmap_all ();
				fc->audio->set_mapping (m);
				handled = true;
			}
		}
	}

	if (!handled) {
		film->remove_content (_content);
	}

	_content.clear ();
	_views.clear ();
}


void
ContentMenu::find_missing ()
{
	if (_content.size() != 1) {
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	/* XXX: a bit nasty */
	auto ic = dynamic_pointer_cast<ImageContent> (_content.front());
	auto dc = dynamic_pointer_cast<DCPContent> (_content.front());

	int r = wxID_CANCEL;
	boost::filesystem::path path;

	if ((ic && !ic->still ()) || dc) {
		wxDirDialog dialog(nullptr, _("Choose a folder"), {}, wxDD_DIR_MUST_EXIST);
		r = dialog.ShowModal();
		path = wx_to_std(dialog.GetPath());
	} else {
		wxFileDialog dialog(nullptr, _("Choose a file"), {}, {}, char_to_wx("*.*"));
		r = dialog.ShowModal();
		path = wx_to_std(dialog.GetPath());
	}

	if (r == wxID_CANCEL) {
		return;
	}

	dcpomatic::find_missing (film->content(), path);
}

void
ContentMenu::re_examine ()
{
	auto film = _film.lock ();
	if (!film) {
		return;
	}

	for (auto i: _content) {
		JobManager::instance()->add(make_shared<ExamineContentJob>(film, i, false));
	}
}


void
ContentMenu::kdm ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
	DCPOMATIC_ASSERT (dcp);

	FileDialog dialog(_parent, _("Select KDM"), char_to_wx("XML files|*.xml|All files|*.*"), 0, "AddKDMPath");

	if (!dialog.show()) {
		return;
	}

	optional<dcp::EncryptedKDM> kdm;
	try {
		kdm = dcp::EncryptedKDM(dcp::file_to_string(dialog.path(), MAX_KDM_SIZE));
	} catch (exception& e) {
		error_dialog (_parent, _("Could not load KDM"), std_to_wx(e.what()));
		return;
	}

	/* Try to decrypt it to get an early preview of any errors */
	try {
		decrypt_kdm_with_helpful_error (*kdm);
	} catch (KDMError& e) {
		error_dialog (_parent, std_to_wx(e.summary()), std_to_wx(e.detail()));
		return;
	} catch (exception& e) {
		error_dialog(_parent, std_to_wx(e.what()));
		return;
	}

	auto cpls = dcp::find_and_resolve_cpls (dcp->directories(), true);
	bool const kdm_matches_any_cpl = std::any_of(cpls.begin(), cpls.end(), [kdm](shared_ptr<const dcp::CPL> cpl) { return cpl->id() == kdm->cpl_id(); });
	bool const kdm_matches_selected_cpl = dcp->cpl() || kdm->cpl_id() == dcp->cpl().get();

	if (!kdm_matches_any_cpl) {
		error_dialog (_parent, _("This KDM was not made for this DCP.  You will need a different one."));
		return;
	}

	if (!kdm_matches_selected_cpl && kdm_matches_any_cpl) {
		message_dialog (_parent, _("This KDM was made for one of the CPLs in this DCP, but not the currently selected one.  To play the currently-selected CPL you will need a different KDM."));
	}

	dcp->add_kdm (*kdm);

	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	JobManager::instance()->add(make_shared<ExamineContentJob>(film, dcp, false));
}

void
ContentMenu::ov ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
	DCPOMATIC_ASSERT (dcp);

	auto film = _film.lock();
	DCPOMATIC_ASSERT(film);
	DirDialog dialog(_parent, _("Select OV"), wxDD_DIR_MUST_EXIST, "AddFilesPath", dcpomatic::film::add_files_override_path(film));

	if (dialog.show()) {
		dcp->add_ov(dialog.path());
		auto film = _film.lock();
		DCPOMATIC_ASSERT (film);
		JobManager::instance()->add (make_shared<ExamineContentJob>(film, dcp, false));
	}
}

void
ContentMenu::properties ()
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	ContentPropertiesDialog dialog(_parent, film, _content.front());
	dialog.ShowModal();
}


void
ContentMenu::advanced ()
{
	DCPOMATIC_ASSERT(!_content.empty());

	auto content = _content.front();
	ContentAdvancedDialog dialog(_parent, content);

	if (dialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	if (content->video) {
		content->video->set_use(!dialog.ignore_video());
		content->video->set_burnt_subtitle_language(dialog.burnt_subtitle_language());
	}

	auto ffmpeg = dynamic_pointer_cast<FFmpegContent>(content);
	if (ffmpeg) {
		ffmpeg->set_filters(dialog.filters());
	}

	if (dialog.video_frame_rate()) {
		auto film = _film.lock();
		DCPOMATIC_ASSERT(film);
		content->set_video_frame_rate(film, *dialog.video_frame_rate());
	} else {
		content->unset_video_frame_rate();
	}
}


void
ContentMenu::cpl_selected (wxCommandEvent& ev)
{
	if (!_pop_up_open) {
		return;
	}

	DCPOMATIC_ASSERT (!_content.empty ());
	auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
	DCPOMATIC_ASSERT (dcp);

	auto cpls = dcp::find_and_resolve_cpls (dcp->directories(), true);

	DCPOMATIC_ASSERT(ev.GetId() >= DCPOMATIC_CPL_MENU);
	DCPOMATIC_ASSERT(ev.GetId() < int(DCPOMATIC_CPL_MENU + cpls.size()));
	dcp->set_cpl(cpls[ev.GetId() - DCPOMATIC_CPL_MENU]->id());

	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	JobManager::instance()->add(make_shared<ExamineContentJob>(film, dcp, false));
}


void
ContentMenu::auto_crop ()
{
	DCPOMATIC_ASSERT (_content.size() == 1);

	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	auto update_viewer = [this](Crop crop) {
		auto film = _film.lock();
		DCPOMATIC_ASSERT (film);
		auto const content = _content.front();
		auto const current_crop = content->video->actual_crop();
		auto const video_size_guess = content->video->size().get_value_or(dcp::Size(1998, 1080));
		_viewer.set_crop_guess(
			dcpomatic::Rect<float>(
				static_cast<float>(std::max(0, crop.left - current_crop.left)) / video_size_guess.width,
				static_cast<float>(std::max(0, crop.top - current_crop.top)) / video_size_guess.height,
				1.0f - (static_cast<float>(std::max(0, crop.left - current_crop.left + crop.right - current_crop.right)) / video_size_guess.width),
				1.0f - (static_cast<float>(std::max(0, crop.top - current_crop.top + crop.bottom - current_crop.bottom)) / video_size_guess.height)
				));
	};

	auto guess_crop_for_content = [this, film]() {
		auto position = _viewer.position_in_content(_content.front()).get_value_or(
			ContentTime::from_frames(_content.front()->video->length(), _content.front()->video_frame_rate().get_value_or(24))
			);
		return guess_crop_by_brightness(film, _content.front(), Config::instance()->auto_crop_threshold(), position);
	};

	/* Make an initial guess in the view and open the dialog */

	auto const crop = guess_crop_for_content ();
	update_viewer (crop);

	_auto_crop_dialog.reset(_parent, crop);
	_auto_crop_dialog->Show ();

	/* Update the dialog and view when the crop threshold changes */
	_auto_crop_config_connection = Config::instance()->Changed.connect([this, guess_crop_for_content, update_viewer](Config::Property property) {
		auto film = _film.lock();
		DCPOMATIC_ASSERT (film);
		if (property == Config::AUTO_CROP_THRESHOLD) {
			auto const crop = guess_crop_for_content();
			_auto_crop_dialog->set(crop);
			update_viewer(crop);
		}
	});

	/* Also update the dialog and view when we're looking at a different frame */
	_auto_crop_viewer_connection = _viewer.ImageChanged.connect([this, guess_crop_for_content, update_viewer](shared_ptr<PlayerVideo>) {
		auto const crop = guess_crop_for_content();
		_auto_crop_dialog->set(crop);
		update_viewer(crop);
	});

	/* Handle the user closing the dialog (with OK or cancel) */
	_auto_crop_dialog->Bind(wxEVT_BUTTON, [this](wxCommandEvent& ev) {
		_auto_crop_config_connection.disconnect ();
		_auto_crop_viewer_connection.disconnect ();
		if (ev.GetId() == wxID_OK) {
			_content.front()->video->set_crop(_auto_crop_dialog->get());
		}
		_auto_crop_dialog->Show (false);
		_viewer.unset_crop_guess ();
	});

	/* Update the view when something in the dialog is changed */
	_auto_crop_dialog->Changed.connect([update_viewer](Crop crop) {
		update_viewer (crop);
	});
}


void
ContentMenu::copy_settings()
{
	DCPOMATIC_ASSERT(_content.size() == 1);

	auto film = _film.lock();
	DCPOMATIC_ASSERT (film);

	DirDialog dialog(_parent, _("Film to copy settings from"), wxDD_DIR_MUST_EXIST, "CopySettingsPath", dcpomatic::film::add_files_override_path(film));

	if (!dialog.show()) {
		return;
	}

	try {
		auto copy_film = std::make_shared<Film>(dialog.path());
		copy_film->read_metadata();
		auto copy_content = copy_film->content();
		auto iter = std::find_if(copy_content.begin(), copy_content.end(), [this](shared_ptr<const Content> content) {
			return content->paths() == _content[0]->paths();
		});
		if (iter == copy_content.end()) {
			error_dialog(_parent, wxString::Format(_("Could not find this content in the project \"%s\"."), std_to_wx(dialog.path().filename().string())));
		} else {
			_content[0]->take_settings_from(*iter);
		}
	} catch (std::exception& e) {
		error_dialog(_parent, std_to_wx(e.what()));
	}
}

