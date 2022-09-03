/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_assert.h"
#include "lib/examine_content_job.h"
#include "lib/job_manager.h"
#include <dcp/exceptions.h>
#include <dcp/warnings.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
LIBDCP_DISABLE_WARNINGS
#include <wx/progdlg.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::optional;
using namespace dcpomatic;


ContentView::ContentView (wxWindow* parent)
	: wxListCtrl (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER)
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
		return {};
	}

	DCPOMATIC_ASSERT (s < int(_content.size()));
	return _content[s];
}


void
ContentView::update ()
{
	using namespace boost::filesystem;

	DeleteAllItems ();
	_content.clear ();
	auto dir = Config::instance()->player_content_directory();
	if (!dir || !boost::filesystem::is_directory(*dir)) {
		dir = home_directory ();
	}

	wxProgressDialog progress (_("DCP-o-matic"), _("Reading content directory"));
	auto jm = JobManager::instance ();

	list<shared_ptr<ExamineContentJob>> jobs;

	for (auto i: directory_iterator(*dir)) {
		try {
			progress.Pulse ();

			shared_ptr<Content> content;
			if (is_directory(i) && (is_regular_file(i / "ASSETMAP") || is_regular_file(i / "ASSETMAP.xml"))) {
				content = make_shared<DCPContent>(i);
			} else if (i.path().extension() == ".mp4") {
				auto all_content = content_factory(i);
				if (!all_content.empty()) {
					content = all_content[0];
				}
			}

			if (content) {
				auto job = make_shared<ExamineContentJob>(shared_ptr<Film>(), content);
				jm->add (job);
				jobs.push_back (job);
			}
		} catch (boost::filesystem::filesystem_error& e) {
			/* Never mind */
		} catch (dcp::ReadError& e) {
			/* Never mind */
		}
	}

	while (jm->work_to_do()) {
		if (!progress.Pulse()) {
			/* user pressed cancel */
			for (auto i: jm->get()) {
				i->cancel();
			}
			return;
		}
		dcpomatic_sleep_seconds (1);
	}

	/* Add content from successful jobs and report errors */
	for (auto i: jobs) {
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
	auto length = content->approximate_length ();
	auto const hmsf = length.split (24);
	it.SetText(wxString::Format("%02d:%02d:%02d", hmsf.h, hmsf.m, hmsf.s));
	InsertItem(it);

	auto dcp = dynamic_pointer_cast<DCPContent>(content);
	if (dcp && dcp->content_kind()) {
		it.SetId(N);
		it.SetColumn(1);
		it.SetText(std_to_wx(dcp->content_kind()->name()));
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
	for (auto i: _content) {
		if (i->digest() == digest) {
			return i;
		}
	}

	return {};
}
