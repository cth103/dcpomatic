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

#include "lib/content_store.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/listctrl.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <vector>

class Content;
class Film;

class ContentView : public wxListCtrl, public ContentStore
{
public:
	ContentView (wxWindow* parent);

	boost::shared_ptr<Content> selected () const;
	void update ();

	boost::shared_ptr<Content> get (std::string digest) const;

private:
	void add (boost::shared_ptr<Content> content);

	boost::weak_ptr<Film> _film;
	std::vector<boost::shared_ptr<Content> > _content;
};
