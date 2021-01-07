/*
    Copyright (C) 2015-2018 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "lib/user_property.h"
#include <list>
#include <map>

class Content;
class Film;
class UserProperty;

class ContentPropertiesDialog : public TableDialog
{
public:
	ContentPropertiesDialog (wxWindow* parent, std::shared_ptr<const Film> film, std::shared_ptr<Content> content);

private:
	void maybe_add_group (std::map<UserProperty::Category, std::list<UserProperty> > const & groups, UserProperty::Category category);
};
