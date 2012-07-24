/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/job_manager_view.cc
 *  @brief Class generating a GTK widget to show the progress of jobs.
 */

#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/util.h"
#include "lib/exceptions.h"
#include "job_manager_view.h"
#include "gtk_util.h"

using namespace std;
using namespace boost;

/** Must be called in the GUI thread */
JobManagerView::JobManagerView ()
{
	_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_ALWAYS);
	
	_store = Gtk::TreeStore::create (_columns);
	_view.set_model (_store);
	_view.append_column ("Name", _columns.name);

	Gtk::CellRendererProgress* r = Gtk::manage (new Gtk::CellRendererProgress ());
	int const n = _view.append_column ("Progress", *r);
	Gtk::TreeViewColumn* c = _view.get_column (n - 1);
	c->add_attribute (r->property_value(), _columns.progress);
	c->add_attribute (r->property_pulse(), _columns.progress_unknown);
	c->add_attribute (r->property_text(), _columns.text);

	_scroller.add (_view);
	_scroller.set_size_request (-1, 150);
	
	update ();
}

/** Update the view by examining the state of each jobs.
 *  Must be called in the GUI thread.
 */
void
JobManagerView::update ()
{
	list<shared_ptr<Job> > jobs = JobManager::instance()->get ();

	for (list<shared_ptr<Job> >::iterator i = jobs.begin(); i != jobs.end(); ++i) {
		Gtk::ListStore::iterator j = _store->children().begin();
		while (j != _store->children().end()) {
			Gtk::TreeRow r = *j;
			shared_ptr<Job> job = r[_columns.job];
			if (job == *i) {
				break;
			}
			++j;
		}

		Gtk::TreeRow r;
		if (j == _store->children().end ()) {
			j = _store->append ();
			r = *j;
			r[_columns.name] = (*i)->name ();
			r[_columns.job] = *i;
			r[_columns.progress_unknown] = -1;
			r[_columns.informed_of_finish] = false;
		} else {
			r = *j;
		}

		bool inform_of_finish = false;
		string const st = (*i)->status ();

		if (!(*i)->finished ()) {
			float const p = (*i)->overall_progress ();
			if (p >= 0) {
				r[_columns.text] = st;
				r[_columns.progress] = p * 100;
			} else {
				r[_columns.text] = "Running";
				r[_columns.progress_unknown] = r[_columns.progress_unknown] + 1;
			}
		}
		
		/* Hack to work around our lack of cross-thread
		   signalling; we tell the job to emit_finished()
		   from here (the GUI thread).
		*/
		
		if ((*i)->finished_ok ()) {
			bool i = r[_columns.informed_of_finish];
			if (!i) {
				r[_columns.progress_unknown] = -1;
				r[_columns.progress] = 100;
				r[_columns.text] = st;
				inform_of_finish = true;
			}
		} else if ((*i)->finished_in_error ()) {
			bool i = r[_columns.informed_of_finish];
			if (!i) {
				r[_columns.progress_unknown] = -1;
				r[_columns.progress] = 100;
				r[_columns.text] = st;
				inform_of_finish = true;
			}
		}

		if (inform_of_finish) {
			try {
				(*i)->emit_finished ();
			} catch (OpenFileError& e) {
				stringstream s;
				s << "Error: " << e.what();
				error_dialog (s.str ());
			}
			r[_columns.informed_of_finish] = true;
		}
	}
}
