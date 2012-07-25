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

/** @file src/job_manager_view.h
 *  @brief Class which is a wxPanel for showing the progress of jobs.
 */

#include <string>
#include <boost/shared_ptr.hpp>
#include <wx/wx.h>

class Job;

/** @class JobManagerView
 *  @brief Class which is a wxPanel for showing the progress of jobs.
 */
class JobManagerView : public wxPanel
{
public:
	JobManagerView (wxWindow *);

	void update ();

private:
	void periodic (wxTimerEvent &);

	boost::shared_ptr<wxTimer> _timer;
	wxFlexGridSizer* _sizer;
	struct JobRecord {
		wxGauge* gauge;
		bool informed_of_finish;
	};
		
	std::map<boost::shared_ptr<Job>, JobRecord> _job_records;
#if 0	
	/** Scroller for all our contents */
	Gtk::ScrolledWindow _scroller;
	/** View for the jobs */
	Gtk::TreeView _view;
	/** Store for the jobs */
	Glib::RefPtr<Gtk::TreeStore> _store;

	/** The TreeModelColumnRecord for the store */
	class StoreColumns : public Gtk::TreeModelColumnRecord
	{
	public:
		StoreColumns ()
		{
			add (name);
			add (job);
			add (progress);
			add (progress_unknown);
			add (text);
			add (informed_of_finish);
		}

		/** Job name */
		Gtk::TreeModelColumn<std::string> name;
		/** Job */
		Gtk::TreeModelColumn<boost::shared_ptr<Job> > job;
		/** Progress */
		Gtk::TreeModelColumn<float> progress;
		/** An increasing integer number if the progress is unknown */
		Gtk::TreeModelColumn<int> progress_unknown;
		/** Text to write into the progress bar */
		Gtk::TreeModelColumn<std::string> text;
		/** true if the job has been informed of its finish */
		Gtk::TreeModelColumn<bool> informed_of_finish;
	};

	/** The columns for the store */
	StoreColumns _columns;
#endif	
};
