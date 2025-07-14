/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include "examine_content_job.h"
#include "film.h"
#include "log.h"
#include <boost/filesystem.hpp>
#include <iostream>

#include "i18n.h"


using std::string;
using std::cout;
using std::shared_ptr;


ExamineContentJob::ExamineContentJob(shared_ptr<const Film> film, shared_ptr<Content> content, bool tolerant)
	: Job(film)
	, _content(content)
	, _tolerant(tolerant)
{

}


ExamineContentJob::~ExamineContentJob()
{
	stop_thread();
}


string
ExamineContentJob::name() const
{
	return _("Examining content");
}


string
ExamineContentJob::json_name() const
{
	return N_("examine_content");
}


void
ExamineContentJob::run()
{
	_content->examine(_film, shared_from_this(), _tolerant);
	set_progress(1);
	set_state(FINISHED_OK);
}
