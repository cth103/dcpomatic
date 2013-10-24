/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include <wx/dirdlg.h>
#include "lib/playlist.h"
#include "lib/film.h"
#include "lib/moving_image_content.h"
#include "lib/content_factory.h"
#include "lib/examine_content_job.h"
#include "lib/job_manager.h"
#include "content_menu.h"
#include "repeat_dialog.h"
#include "wx_util.h"

using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

enum {
	ID_repeat = 1,
	ID_find_missing,
	ID_remove
};

ContentMenu::ContentMenu (shared_ptr<Film> f, wxWindow* p)
	: _menu (new wxMenu)
	, _film (f)
	, _parent (p)
{
	_repeat = _menu->Append (ID_repeat, _("Repeat..."));
	_find_missing = _menu->Append (ID_find_missing, _("Find missing..."));
	_menu->AppendSeparator ();
	_remove = _menu->Append (ID_remove, _("Remove"));

	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::repeat, this), ID_repeat);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::find_missing, this), ID_find_missing);
	_parent->Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&ContentMenu::remove, this), ID_remove);
}

ContentMenu::~ContentMenu ()
{
	delete _menu;
}

void
ContentMenu::popup (ContentList c, wxPoint p)
{
	_content = c;
	_repeat->Enable (!_content.empty ());
	_find_missing->Enable (_content.size() == 1 && !_content.front()->path_valid ());
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

	film->playlist()->remove (_content);

	_content.clear ();
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
	if (dynamic_pointer_cast<MovingImageContent> (_content.front ())) {
		wxDirDialog* d = new wxDirDialog (0, _("Choose a folder"), wxT (""), wxDD_DIR_MUST_EXIST);
		int const r = d->ShowModal ();
		if (r == wxID_OK) {
			content.reset (new MovingImageContent (film, boost::filesystem::path (wx_to_std (d->GetPath ()))));
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
	
	j->Finished.connect (
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
ContentMenu::maybe_found_missing (weak_ptr<Job> j, weak_ptr<Content> oc, weak_ptr<Content> nc)
{
	shared_ptr<Job> job = j.lock ();
	if (!job || !job->finished_ok ()) {
		return;
	}

	shared_ptr<Content> old_content = oc.lock ();
	shared_ptr<Content> new_content = nc.lock ();
	assert (old_content);
	assert (new_content);

	if (new_content->digest() != old_content->digest()) {
		error_dialog (0, _("The content file(s) you specified are not the same as those that are missing.  Either try again with the correct content file or remove the missing content."));
		return;
	}

	old_content->set_path (new_content->path ());
}
