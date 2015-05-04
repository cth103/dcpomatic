/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
#include <wx/wx.h>
#include <wx/dirdlg.h>

using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

enum {
	ID_repeat = 1,
	ID_join,
	ID_find_missing,
	ID_properties,
	ID_re_examine,
	ID_kdm,
	ID_remove
};

ContentMenu::ContentMenu (wxWindow* p)
	: _menu (new wxMenu)
	, _parent (p)
{
	_repeat = _menu->Append (ID_repeat, _("Repeat..."));
	_join = _menu->Append (ID_join, _("Join"));
	_find_missing = _menu->Append (ID_find_missing, _("Find missing..."));
	_properties = _menu->Append (ID_properties, _("Properties..."));
	_re_examine = _menu->Append (ID_re_examine, _("Re-examine..."));
	_kdm = _menu->Append (ID_kdm, _("Add KDM..."));
	_menu->AppendSeparator ();
	_remove = _menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::repeat, this), ID_repeat);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::join, this), ID_join);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::find_missing, this), ID_find_missing);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::properties, this), ID_properties);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::re_examine, this), ID_re_examine);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::kdm, this), ID_kdm);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::remove, this), ID_remove);
}

ContentMenu::~ContentMenu ()
{
	delete _menu;
}

void
ContentMenu::popup (weak_ptr<Film> f, ContentList c, TimelineContentViewList v, wxPoint p)
{
	_film = f;
	_content = c;
	_views = v;
	_repeat->Enable (!_content.empty ());

	int n = 0;
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		if (dynamic_pointer_cast<FFmpegContent> (*i)) {
			++n;
		}
	}
	
	_join->Enable (n > 1);
	
	_find_missing->Enable (_content.size() == 1 && !_content.front()->paths_valid ());
	_properties->Enable (_content.size() == 1);
	_re_examine->Enable (!_content.empty ());

	if (_content.size() == 1) {
		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (_content.front ());
		_kdm->Enable (dcp && dcp->encrypted ());
	} else {
		_kdm->Enable (false);
	}
	
	_remove->Enable (!_content.empty ());
	_parent->PopupMenu (_menu, p);
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

	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	film->playlist()->repeat (_content, d->number ());
	d->Destroy ();

	_content.clear ();
	_views.clear ();
}

void
ContentMenu::join ()
{
	vector<shared_ptr<Content> > fc;
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		shared_ptr<FFmpegContent> f = dynamic_pointer_cast<FFmpegContent> (*i);
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
		for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
			film->remove_content (*i);
		}
		film->add_content (joined);
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

	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	/* We are removing from the timeline if _views is not empty */
	bool handled = false;
	if (!_views.empty ()) {
		/* Special case: we only remove FFmpegContent if its video view is selected;
		   if not, and its audio view is selected, we unmap the audio.
		*/
		for (ContentList::iterator i = _content.begin(); i != _content.end(); ++i) {
			shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (*i);
			if (!fc) {
				continue;
			}
			
			shared_ptr<TimelineVideoContentView> video;
			shared_ptr<TimelineAudioContentView> audio;

			for (TimelineContentViewList::iterator i = _views.begin(); i != _views.end(); ++i) {
				shared_ptr<TimelineVideoContentView> v = dynamic_pointer_cast<TimelineVideoContentView> (*i);
				shared_ptr<TimelineAudioContentView> a = dynamic_pointer_cast<TimelineAudioContentView> (*i);
				if (v && v->content() == fc) {
					video = v;
				} else if (a && a->content() == fc) {
					audio = a;
				}
			}

			if (!video && audio) {
				AudioMapping m = fc->audio_mapping ();
				m.unmap_all ();
				fc->set_audio_mapping (m);
				handled = true;
			}
		}
	}

	if (!handled) {
		film->playlist()->remove (_content);
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
	if (ic && !ic->still ()) {
		wxDirDialog* d = new wxDirDialog (0, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
		int const r = d->ShowModal ();
		if (r == wxID_OK) {
			content.reset (new ImageContent (film, boost::filesystem::path (wx_to_std (d->GetPath ()))));
		}
		d->Destroy ();
	} else {
		wxFileDialog* d = new wxFileDialog (0, _("Choose a file"), wxT (""), wxT (""), wxT ("*.*"), wxFD_MULTIPLE);
		int const r = d->ShowModal ();
		if (r == wxID_OK) {
			content = content_factory (film, wx_to_std (d->GetPath ()));
		}
		d->Destroy ();
	}

	if (!content) {
		return;
	}

	shared_ptr<Job> j (new ExamineContentJob (film, content));
	
	_job_connection = j->Finished.connect (
		bind (
			&ContentMenu::maybe_found_missing,
			this,
			boost::weak_ptr<Job> (j),
			boost::weak_ptr<Content> (_content.front ()),
			boost::weak_ptr<Content> (content)
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

	for (ContentList::iterator i = _content.begin(); i != _content.end(); ++i) {
		film->examine_content (*i);
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
		dcp->add_kdm (dcp::EncryptedKDM (dcp::file_to_string (wx_to_std (d->GetPath ()))));
		shared_ptr<Film> film = _film.lock ();
		DCPOMATIC_ASSERT (film);
		film->examine_content (dcp);
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
