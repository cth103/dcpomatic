/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "content_menu.h"
#include "repeat_dialog.h"
#include "wx_util.h"
#include "timeline_video_content_view.h"
#include "timeline_audio_content_view.h"
#include "content_properties_dialog.h"
#include "lib/playlist.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/content_factory.h"
#include "lib/examine_content_job.h"
#include "lib/job_manager.h"
#include "lib/exceptions.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/ffmpeg_content.h"
#include "lib/audio_content.h"
#include <dcp/cpl.h>
#include <dcp/exceptions.h>
#include <wx/wx.h>
#include <wx/dirdlg.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::cout;
using std::vector;
using std::exception;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

enum {
	/* Start at 256 so we can have IDs on _cpl_menu from 1 to 255 */
	ID_repeat = 256,
	ID_join,
	ID_find_missing,
	ID_properties,
	ID_re_examine,
	ID_kdm,
	ID_ov,
	ID_choose_cpl,
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
	_re_examine = _menu->Append (ID_re_examine, _("Re-examine..."));
	_menu->AppendSeparator ();
	_kdm = _menu->Append (ID_kdm, _("Add KDM..."));
	_ov = _menu->Append (ID_ov, _("Add OV..."));
	_cpl_menu = new wxMenu ();
	_choose_cpl = _menu->Append (ID_choose_cpl, _("Choose CPL..."), _cpl_menu);
	_menu->AppendSeparator ();
	_remove = _menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::repeat, this), ID_repeat);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::join, this), ID_join);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::find_missing, this), ID_find_missing);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::properties, this), ID_properties);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::re_examine, this), ID_re_examine);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::kdm, this), ID_kdm);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::ov, this), ID_ov);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::remove, this), ID_remove);

	_parent->Bind (wxEVT_MENU, boost::bind (&ContentMenu::cpl_selected, this, _1), 1, ID_repeat - 1);
}

ContentMenu::~ContentMenu ()
{
	delete _menu;

	BOOST_FOREACH (boost::signals2::connection& i, _job_connections) {
		i.disconnect ();
	}
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
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (dynamic_pointer_cast<FFmpegContent> (i)) {
			++n;
		}
	}

	_join->Enable (n > 1);

	_find_missing->Enable (_content.size() == 1 && !_content.front()->paths_valid ());
	_properties->Enable (_content.size() == 1);
	_re_examine->Enable (!_content.empty ());

	if (_content.size() == 1) {
		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (_content.front ());
		if (dcp) {
			_kdm->Enable (dcp->encrypted ());
			_ov->Enable (dcp->needs_assets ());
			try {
				DCPExaminer ex (dcp);
				list<shared_ptr<dcp::CPL> > cpls = ex.cpls ();
				_choose_cpl->Enable (cpls.size() > 1);
				/* We can't have 0 as a menu item ID on OS X */
				int id = 1;
				BOOST_FOREACH (shared_ptr<dcp::CPL> i, cpls) {
					wxMenuItem* item = _cpl_menu->AppendCheckItem (
						id++,
						wxString::Format (
							"%s (%s)",
							std_to_wx(i->annotation_text()).data(),
							std_to_wx(i->id()).data()
							)
						);
					item->Check (dcp->cpl() && dcp->cpl() == i->id());
				}
			} catch (dcp::DCPReadError) {
				/* The DCP is probably missing */
			}
		} else {
			_kdm->Enable (false);
			_ov->Enable (false);
			_choose_cpl->Enable (false);
		}
	} else {
		_kdm->Enable (false);
	}

	_remove->Enable (!_content.empty ());

	_pop_up_open = true;
	_parent->PopupMenu (_menu, p);
	_pop_up_open = false;
}

void
ContentMenu::repeat ()
{
	if (_content.empty ()) {
		return;
	}

	RepeatDialog* d = new RepeatDialog (_parent);
	if (d->ShowModal() != wxID_OK) {
		d->Destroy ();
		return;
	}

	shared_ptr<Film> film = _film.lock ();
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
	vector<shared_ptr<Content> > fc;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		shared_ptr<FFmpegContent> f = dynamic_pointer_cast<FFmpegContent> (i);
		if (f) {
			fc.push_back (f);
		}
	}

	DCPOMATIC_ASSERT (fc.size() > 1);

	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	try {
		shared_ptr<FFmpegContent> joined (new FFmpegContent (film, fc));
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

	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	/* We are removing from the timeline if _views is not empty */
	bool handled = false;
	if (!_views.empty ()) {
		/* Special case: we only remove FFmpegContent if its video view is selected;
		   if not, and its audio view is selected, we unmap the audio.
		*/
		BOOST_FOREACH (shared_ptr<Content> i, _content) {
			shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (i);
			if (!fc) {
				continue;
			}

			shared_ptr<TimelineVideoContentView> video;
			shared_ptr<TimelineAudioContentView> audio;

			BOOST_FOREACH (shared_ptr<TimelineContentView> j, _views) {
				shared_ptr<TimelineVideoContentView> v = dynamic_pointer_cast<TimelineVideoContentView> (j);
				shared_ptr<TimelineAudioContentView> a = dynamic_pointer_cast<TimelineAudioContentView> (j);
				if (v && v->content() == fc) {
					video = v;
				} else if (a && a->content() == fc) {
					audio = a;
				}
			}

			if (!video && audio) {
				AudioMapping m = fc->audio->mapping ();
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

	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	shared_ptr<Content> content;

	/* XXX: a bit nasty */
	shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (_content.front ());
	shared_ptr<DCPContent> dc = dynamic_pointer_cast<DCPContent> (_content.front ());

	int r = wxID_CANCEL;
	boost::filesystem::path path;

	if ((ic && !ic->still ()) || dc) {
		wxDirDialog* d = new wxDirDialog (0, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
		r = d->ShowModal ();
		path = wx_to_std (d->GetPath ());
		d->Destroy ();
	} else {
		wxFileDialog* d = new wxFileDialog (0, _("Choose a file"), wxT (""), wxT (""), wxT ("*.*"), wxFD_MULTIPLE);
		r = d->ShowModal ();
		path = wx_to_std (d->GetPath ());
		d->Destroy ();
	}

	if (r == wxID_OK) {
		content = content_factory (film, path);
	}

	if (!content) {
		return;
	}

	shared_ptr<Job> j (new ExamineContentJob (film, content));

	_job_connections.push_back (
		j->Finished.connect (
			bind (
				&ContentMenu::maybe_found_missing,
				this,
				boost::weak_ptr<Job> (j),
				boost::weak_ptr<Content> (_content.front ()),
				boost::weak_ptr<Content> (content)
				)
			)
		);

	JobManager::instance()->add (j);
}

void
ContentMenu::re_examine ()
{
	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		JobManager::instance()->add (shared_ptr<Job> (new ExamineContentJob (film, i)));
	}
}

void
ContentMenu::maybe_found_missing (weak_ptr<Job> j, weak_ptr<Content> oc, weak_ptr<Content> nc)
{
	shared_ptr<Job> job = j.lock ();
	if (!job || !job->finished_ok ()) {
		return;
	}

	shared_ptr<Content> old_content = oc.lock ();
	shared_ptr<Content> new_content = nc.lock ();
	DCPOMATIC_ASSERT (old_content);
	DCPOMATIC_ASSERT (new_content);

	if (new_content->digest() != old_content->digest()) {
		error_dialog (0, _("The content file(s) you specified are not the same as those that are missing.  Either try again with the correct content file or remove the missing content."));
		return;
	}

	old_content->set_path (new_content->path (0));
}

void
ContentMenu::kdm ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (_content.front ());
	DCPOMATIC_ASSERT (dcp);

	wxFileDialog* d = new wxFileDialog (_parent, _("Select KDM"));

	if (d->ShowModal() == wxID_OK) {
		try {
			dcp->add_kdm (dcp::EncryptedKDM (dcp::file_to_string (wx_to_std (d->GetPath ()))));
		} catch (exception& e) {
			error_dialog (_parent, wxString::Format (_("Could not load KDM (%s)"), e.what ()));
			d->Destroy ();
			return;
		}

		shared_ptr<Film> film = _film.lock ();
		DCPOMATIC_ASSERT (film);
		JobManager::instance()->add (shared_ptr<Job> (new ExamineContentJob (film, dcp)));
	}

	d->Destroy ();
}

void
ContentMenu::ov ()
{
	DCPOMATIC_ASSERT (!_content.empty ());
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (_content.front ());
	DCPOMATIC_ASSERT (dcp);

	wxDirDialog* d = new wxDirDialog (_parent, _("Select OV"));

	if (d->ShowModal() == wxID_OK) {
		dcp->add_ov (wx_to_std (d->GetPath ()));
		shared_ptr<Film> film = _film.lock ();
		DCPOMATIC_ASSERT (film);
		JobManager::instance()->add (shared_ptr<Job> (new ExamineContentJob (film, dcp)));
	}

	d->Destroy ();
}

void
ContentMenu::properties ()
{
	ContentPropertiesDialog* d = new ContentPropertiesDialog (_parent, _content.front ());
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
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (_content.front ());
	DCPOMATIC_ASSERT (dcp);

	DCPExaminer ex (dcp);
	list<shared_ptr<dcp::CPL> > cpls = ex.cpls ();
	DCPOMATIC_ASSERT (ev.GetId() > 0);
	DCPOMATIC_ASSERT (ev.GetId() <= int (cpls.size()));

	list<shared_ptr<dcp::CPL> >::const_iterator i = cpls.begin ();
	for (int j = 0; j < ev.GetId() - 1; ++j) {
		++i;
	}

	dcp->set_cpl ((*i)->id ());
	shared_ptr<Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	JobManager::instance()->add (shared_ptr<Job> (new ExamineContentJob (film, dcp)));
}
