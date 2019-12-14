/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "dcp.h"
#include "config.h"
#include "film.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "compose.hpp"
#include "dcp_content.h"
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::list;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

dcp::DecryptedKDM
DCP::decrypted_kdm () const
{
	try {
		return dcp::DecryptedKDM (_dcp_content->kdm().get(), Config::instance()->decryption_chain()->key().get());
	} catch (dcp::KDMDecryptionError& e) {
		/* Try to flesh out the error a bit */
		string const kdm_subject_name = _dcp_content->kdm()->recipient_x509_subject_name();
		bool on_chain = false;
		shared_ptr<const dcp::CertificateChain> dc = Config::instance()->decryption_chain();
		BOOST_FOREACH (dcp::Certificate i, dc->root_to_leaf()) {
			if (i.subject() == kdm_subject_name) {
				on_chain = true;
			}
		}
		if (!on_chain) {
			throw KDMError (_("KDM was not made for DCP-o-matic's decryption certificate."), e.what());
		} else if (on_chain && kdm_subject_name != dc->leaf().subject()) {
			throw KDMError (_("KDM was made for DCP-o-matic but not for its leaf certificate."), e.what());
		} else {
			throw;
		}
	}
}

/** Find all the CPLs in our directories, cross-add assets and return the CPLs */
list<shared_ptr<dcp::CPL> >
DCP::cpls () const
{
	list<shared_ptr<dcp::DCP> > dcps;
	list<shared_ptr<dcp::CPL> > cpls;

	LOG_GENERAL ("Reading %1 DCP directories", _dcp_content->directories().size());
	BOOST_FOREACH (boost::filesystem::path i, _dcp_content->directories()) {
		shared_ptr<dcp::DCP> dcp (new dcp::DCP (i));
		list<dcp::VerificationNote> notes;
		dcp->read (&notes, true);
		if (!_tolerant) {
			/** We accept and ignore EmptyAssetPathError but everything else is bad */
			BOOST_FOREACH (dcp::VerificationNote j, notes) {
				if (j.code() == dcp::VerificationNote::EMPTY_ASSET_PATH) {
					LOG_WARNING("Empty path in ASSETMAP of %1", i.string());
				} else {
					boost::throw_exception(dcp::DCPReadError(dcp::note_to_string(j)));
				}
			}
		}
		dcps.push_back (dcp);
		LOG_GENERAL ("Reading DCP %1: %2 CPLs", i.string(), dcp->cpls().size());
		BOOST_FOREACH (shared_ptr<dcp::CPL> i, dcp->cpls()) {
			cpls.push_back (i);
		}
	}

	BOOST_FOREACH (shared_ptr<dcp::DCP> i, dcps) {
		BOOST_FOREACH (shared_ptr<dcp::DCP> j, dcps) {
			if (i != j) {
				i->resolve_refs (j->assets (true));
			}
		}
	}

	if (_dcp_content->kdm ()) {
		dcp::DecryptedKDM k = decrypted_kdm ();
		BOOST_FOREACH (shared_ptr<dcp::DCP> i, dcps) {
			i->add (k);
		}
	}

	return cpls;
}
