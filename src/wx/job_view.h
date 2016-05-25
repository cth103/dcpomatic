/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class Job;
class wxScrolledWindow;
class wxWindow;
class wxFlexGridSizer;
class wxCommandEvent;
class wxBoxSizer;
class wxGauge;
class wxStaticText;
class wxButton;

class JobView : public boost::noncopyable
{
public:
	JobView (boost::shared_ptr<Job> job, wxWindow* parent, wxWindow* container, wxFlexGridSizer* table);

	void maybe_pulse ();

private:

	void progress ();
	void finished ();
	void details_clicked (wxCommandEvent &);
	void cancel_clicked (wxCommandEvent &);
	void pause_clicked (wxCommandEvent &);

	boost::shared_ptr<Job> _job;
	wxWindow* _parent;
	wxBoxSizer* _gauge_message;
	wxGauge* _gauge;
	wxStaticText* _message;
	wxButton* _cancel;
	wxButton* _pause;
	wxButton* _details;
	std::string _last_message;

	boost::signals2::scoped_connection _progress_connection;
	boost::signals2::scoped_connection _finished_connection;
};
