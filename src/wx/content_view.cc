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

#include "content_view.h"
#include "wx_util.h"
#include "lib/dcpomatic_assert.h"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/content_factory.h"
#include "lib/examine_content_job.h"
#include "lib/job_manager.h"
#include "lib/cross.h"
#include <dcp/exceptions.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <wx/progdlg.h>

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

ContentView::ContentView (wxWindow* parent, weak_ptr<Film> film)
	: wxListCtrl (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER)
	, _film (film)
{
	AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 80);
	/* type */
	AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 80);
	/* annotation text */
	AppendColumn (wxT(""), wxLIST_FORMAT_LEFT, 580);
}

shared_ptr<Content>
ContentView::selected () const
{
	long int s = GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (s == -1) {
		return shared_ptr<Content>();
	}

	DCPOMATIC_ASSERT (s < int(_content.size()));
	return _content[s];
}

void
ContentView::update ()
{
	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	using namespace boost::filesystem;

	DeleteAllItems ();
	_content.clear ();
	optional<path> dir = Config::instance()->player_content_directory();
	if (!dir) {
		return;
	}

	wxProgressDialog progress (_("DCP-o-matic"), _("Reading content directory"));
	JobManager* jm = JobManager::instance ();

	list<shared_ptr<ExamineContentJob> > jobs;

	for (directory_iterator i = directory_iterator(*dir); i != directory_iterator(); ++i) {
		try {
			shared_ptr<Content> content;
			if (is_directory(*i) && (is_regular_file(*i / "ASSETMAP") || is_regular_file(*i / "ASSETMAP.xml"))) {
				content.reset (new DCPContent(film, *i));
			} else if (i->path().extension() == ".mp4" || i->path().extension() == ".ecinema") {
				content = content_factory(film, *i).front();
			}

			if (content) {
				shared_ptr<ExamineContentJob> job(new ExamineContentJob(film, content));
				jm->add (job);
				jobs.push_back (job);
			}
		} catch (boost::filesystem::filesystem_error& e) {
			/* Never mind */
		} catch (dcp::DCPReadError& e) {
			/* Never mind */
		}
	}

	while (jm->work_to_do()) {
		if (!progress.Pulse()) {
			/* user pressed cancel */
			BOOST_FOREACH (shared_ptr<Job> i, jm->get()) {
				i->cancel();
			}
			return;
		}
		dcpomatic_sleep (1);
	}

	/* Add content from successful jobs and report errors */
	BOOST_FOREACH (shared_ptr<ExamineContentJob> i, jobs) {
		if (i->finished_in_error()) {
			error_dialog(this, std_to_wx(i->error_summary()) + ".\n", std_to_wx(i->error_details()));
		} else {
			add (i->content());
			_content.push_back (i->content());
		}
	}
}

void
ContentView::add (shared_ptr<Content> content)
{
	int const N = GetItemCount();

	wxListItem it;
	it.SetId(N);
	it.SetColumn(0);
	DCPTime length = content->length_after_trim ();
	int h, m, s, f;
	length.split (24, h, m, s, f);
	it.SetText(wxString::Format("%02d:%02d:%02d", h, m, s));
	InsertItem(it);

	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(content);
	if (dcp && dcp->content_kind()) {
		it.SetId(N);
		it.SetColumn(1);
		it.SetText(std_to_wx(dcp::content_kind_to_string(*dcp->content_kind())));
		SetItem(it);
	}

	it.SetId(N);
	it.SetColumn(2);
	it.SetText(std_to_wx(content->summary()));
	SetItem(it);
}

shared_ptr<Content>
ContentView::get (string digest) const
{
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (i->digest() == digest) {
			return i;
		}
	}

	return shared_ptr<Content>();
}

void
ContentView::set_film (weak_ptr<Film> film)
{
	_film = film;
}
