/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_advanced_dialog.h"
#include "content_menu.h"
#include "content_properties_dialog.h"
#include "repeat_dialog.h"
#include "timeline_audio_content_view.h"
#include "timeline_video_content_view.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/copy_dcp_details_to_film.h"
#include "lib/dcp_content.h"
#include "lib/examine_content_job.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/find_missing.h"
#include "lib/image_content.h"
#include "lib/job_manager.h"
#include "lib/playlist.h"
#include <dcp/cpl.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <dcp/search.h>
#include <wx/dirdlg.h>
#include <wx/wx.h>
#include <algorithm>
#include <iostream>


using std::cout;
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


enum {
	/* Start at 256 so we can have IDs on _cpl_menu from 1 to 255 */
	ID_repeat = 256,
	ID_join,
	ID_find_missing,
	ID_properties,
	ID_advanced,
	ID_re_examine,
	ID_kdm,
	ID_ov,
	ID_choose_cpl,
	ID_set_dcp_settings,
	ID_remove
};


ContentMenu::ContentMenu (wxWindow* p)
	: _menu (new wxMenu)
	, _parent (p)
	, _pop_up_open (false)
{
	_repeat = _menu->Append (ID_repeat, _("Repeat..."));
	_join = _menu->Append (ID_join, _("Join"));
	_find_missing = _menu->Append (ID_find_missing, _("Find missing..."));
	_properties = _menu->Append (ID_properties, _("Properties..."));
	_advanced = _menu->Append (ID_advanced, _("Advanced settings..."));
	_re_examine = _menu->Append (ID_re_examine, _("Re-examine..."));
	_menu->AppendSeparator ();
	_kdm = _menu->Append (ID_kdm, _("Add KDM..."));
	_ov = _menu->Append (ID_ov, _("Add OV..."));
	_cpl_menu = new wxMenu ();
	_choose_cpl = _menu->Append (ID_choose_cpl, _("Choose CPL..."), _cpl_menu);
	_set_dcp_settings = _menu->Append (ID_set_dcp_settings, _("Set project DCP settings from this DCP"));
	_menu->AppendSeparator ();
	_remove = _menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::repeat, this), ID_repeat);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::join, this), ID_join);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::find_missing, this), ID_find_missing);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::properties, this), ID_properties);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::advanced, this), ID_advanced);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::re_examine, this), ID_re_examine);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::kdm, this), ID_kdm);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::ov, this), ID_ov);
	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::set_dcp_settings, this), ID_set_dcp_settings);
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
	for (int i = 1; i <= N; ++i) {
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

	_find_missing->Enable (_content.size() == 1 && !_content.front()->paths_valid ());
	_properties->Enable (_content.size() == 1);
	_advanced->Enable (_content.size() == 1);
	_re_examine->Enable (!_content.empty ());

	if (_content.size() == 1) {
		auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
		if (dcp) {
			_kdm->Enable (dcp->encrypted ());
			_ov->Enable (dcp->needs_assets ());
			_set_dcp_settings->Enable (static_cast<bool>(dcp));
			try {
				auto cpls = dcp::find_and_resolve_cpls (dcp->directories(), true);
				_choose_cpl->Enable (cpls.size() > 1);
				/* We can't have 0 as a menu item ID on OS X */
				int id = 1;
				for (auto i: cpls) {
					auto item = _cpl_menu->AppendRadioItem (
						id++,
						wxString::Format (
							"%s (%s)",
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
		}
	} else {
		_kdm->Enable (false);
		_set_dcp_settings->Enable (false);
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
	copy_dcp_details_to_film (dcp, film);
}


void
ContentMenu::repeat ()
{
	if (_content.empty ()) {
		return;
	}

	auto d = new RepeatDialog (_parent);
	if (d->ShowModal() != wxID_OK) {
		d->Destroy ();
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	film->repeat_content (_content, d->number ());
	d->Destroy ();

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

			shared_ptr<TimelineVideoContentView> video;
			shared_ptr<TimelineAudioContentView> audio;

			for (auto j: _views) {
				auto v = dynamic_pointer_cast<TimelineVideoContentView>(j);
				auto a = dynamic_pointer_cast<TimelineAudioContentView>(j);
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
		auto d = new wxDirDialog (nullptr, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
		r = d->ShowModal ();
		path = wx_to_std (d->GetPath());
		d->Destroy ();
	} else {
		auto d = new wxFileDialog (nullptr, _("Choose a file"), wxT (""), wxT (""), wxT ("*.*"));
		r = d->ShowModal ();
		path = wx_to_std (d->GetPath());
		d->Destroy ();
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
		JobManager::instance()->add(make_shared<ExamineContentJob>(film, i));
	}
}


void
ContentMenu::kdm ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
	DCPOMATIC_ASSERT (dcp);

	auto d = new wxFileDialog (_parent, _("Select KDM"));

	if (d->ShowModal() == wxID_OK) {
		optional<dcp::EncryptedKDM> kdm;
		try {
			kdm = dcp::EncryptedKDM (dcp::file_to_string(wx_to_std(d->GetPath()), MAX_KDM_SIZE));
		} catch (exception& e) {
			error_dialog (_parent, _("Could not load KDM"), std_to_wx(e.what()));
			d->Destroy ();
			return;
		}

		/* Try to decrypt it to get an early preview of any errors */
		try {
			decrypt_kdm_with_helpful_error (*kdm);
		} catch (KDMError& e) {
			error_dialog (_parent, std_to_wx(e.summary()), std_to_wx(e.detail()));
			return;
		} catch (exception& e) {
			error_dialog (_parent, e.what());
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
		JobManager::instance()->add (make_shared<ExamineContentJob>(film, dcp));
	}

	d->Destroy ();
}

void
ContentMenu::ov ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	auto dcp = dynamic_pointer_cast<DCPContent> (_content.front());
	DCPOMATIC_ASSERT (dcp);

	auto d = new wxDirDialog (_parent, _("Select OV"));

	if (d->ShowModal() == wxID_OK) {
		dcp->add_ov (wx_to_std (d->GetPath()));
		shared_ptr<Film> film = _film.lock ();
		DCPOMATIC_ASSERT (film);
		JobManager::instance()->add (make_shared<ExamineContentJob>(film, dcp));
	}

	d->Destroy ();
}

void
ContentMenu::properties ()
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	auto d = new ContentPropertiesDialog (_parent, film, _content.front());
	d->ShowModal ();
	d->Destroy ();
}


void
ContentMenu::advanced ()
{
	auto d = new ContentAdvancedDialog (_parent, _content.front());
	d->ShowModal ();
	d->Destroy ();
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
	DCPOMATIC_ASSERT (ev.GetId() > 0);
	DCPOMATIC_ASSERT (ev.GetId() <= int (cpls.size()));

	auto i = cpls.begin ();
	for (int j = 0; j < ev.GetId() - 1; ++j) {
		++i;
	}

	dcp->set_cpl ((*i)->id ());
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	JobManager::instance()->add (make_shared<ExamineContentJob>(film, dcp));
}
