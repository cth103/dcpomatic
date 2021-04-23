/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "dcp.h"
#include "dcp_content.h"
#include "dcpomatic_log.h"
#include "log.h"
#include "util.h"
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>

#include "i18n.h"


using std::list;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;
using std::vector;


/** Find all the CPLs in our directories, cross-add assets and return the CPLs */
list<shared_ptr<dcp::CPL>>
DCP::cpls () const
{
	list<shared_ptr<dcp::DCP>> dcps;
	list<shared_ptr<dcp::CPL>> cpls;

	/** We accept and ignore some warnings / errors but everything else is bad */
	vector<dcp::VerificationNote::Code> ignore = {
		dcp::VerificationNote::Code::EMPTY_ASSET_PATH,
		dcp::VerificationNote::Code::EXTERNAL_ASSET,
		dcp::VerificationNote::Code::THREED_ASSET_MARKED_AS_TWOD,
	};

	LOG_GENERAL ("Reading %1 DCP directories", _dcp_content->directories().size());
	for (auto i: _dcp_content->directories()) {
		auto dcp = make_shared<dcp::DCP>(i);
		vector<dcp::VerificationNote> notes;
		dcp->read (&notes, true);
		if (!_tolerant) {
			for (auto j: notes) {
				if (std::find(ignore.begin(), ignore.end(), j.code()) != ignore.end()) {
					LOG_WARNING("Ignoring: %1", dcp::note_to_string(j));
				} else {
					boost::throw_exception(dcp::ReadError(dcp::note_to_string(j)));
				}
			}
		}
		dcps.push_back (dcp);
		LOG_GENERAL ("Reading DCP %1: %2 CPLs", i.string(), dcp->cpls().size());
		for (auto i: dcp->cpls()) {
			cpls.push_back (i);
		}
	}

	for (auto i: dcps) {
		for (auto j: dcps) {
			if (i != j) {
				i->resolve_refs(j->assets());
			}
		}
	}

	if (_dcp_content->kdm ()) {
		auto k = decrypt_kdm_with_helpful_error (_dcp_content->kdm().get());
		for (auto i: dcps) {
			i->add (k);
		}
	}

	return cpls;
}
