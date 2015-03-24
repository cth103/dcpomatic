/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <boost/filesystem.hpp>
#include "examine_content_job.h"
#include "log.h"
#include "content.h"
#include "film.h"

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ExamineContentJob::ExamineContentJob (shared_ptr<const Film> f, shared_ptr<Content> c)
	: Job (f)
	, _content (c)
{

}

ExamineContentJob::~ExamineContentJob ()
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
