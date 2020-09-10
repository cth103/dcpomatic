/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "combine_dcp_job.h"
#include <dcp/combine.h>
#include <dcp/exceptions.h>

#include "i18n.h"


using std::string;
using std::vector;
using boost::shared_ptr;


CombineDCPJob::CombineDCPJob (vector<boost::filesystem::path> inputs, boost::filesystem::path output)
	: Job (shared_ptr<Film>())
	, _inputs (inputs)
	, _output (output)
{

}


string
CombineDCPJob::name () const
{
	return _("Combine DCPs");
}


string
CombineDCPJob::json_name () const
{
	return N_("combine_dcps");
}


void
CombineDCPJob::run ()
{
	try {
		dcp::combine (_inputs, _output);
	} catch (dcp::CombineError& e) {
		set_state (FINISHED_ERROR);
		set_error (e.what(), "");
		return;
	} catch (dcp::ReadError& e) {
		set_state (FINISHED_ERROR);
		set_error (e.what(), e.detail().get_value_or(""));
		return;
	}

	set_progress (1);
	set_state (FINISHED_OK);
}
