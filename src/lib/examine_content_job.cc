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

#include <boost/filesystem.hpp>
#include "examine_content_job.h"
#include "log.h"
#include "content.h"
#include "film.h"
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ExamineContentJob::ExamineContentJob (shared_ptr<const Film> film, shared_ptr<Content> c)
	: Job (film)
	, _content (c)
{

}

string
ExamineContentJob::name () const
{
	return _("Examine content");
}

string
ExamineContentJob::json_name () const
{
	return N_("examine_content");
}

void
ExamineContentJob::run ()
{
	_content->examine (shared_from_this ());
	set_progress (1);
	set_state (FINISHED_OK);
}
