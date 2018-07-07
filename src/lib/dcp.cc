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
#include "dcp_content.h"
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/exceptions.h>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::list;
using std::string;
using boost::shared_ptr;

/** Find all the CPLs in our directories, cross-add assets and return the CPLs */
list<shared_ptr<dcp::CPL> >
DCP::cpls () const
{
	list<shared_ptr<dcp::DCP> > dcps;
	list<shared_ptr<dcp::CPL> > cpls;

	BOOST_FOREACH (boost::filesystem::path i, _dcp_content->directories()) {
		shared_ptr<dcp::DCP> dcp (new dcp::DCP (i));
		dcp->read (false, 0, true);
		dcps.push_back (dcp);
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
		BOOST_FOREACH (shared_ptr<dcp::DCP> i, dcps) {
			try {
				i->add (dcp::DecryptedKDM (_dcp_content->kdm().get(), Config::instance()->decryption_chain()->key().get ()));
			} catch (dcp::KDMDecryptionError& e) {
				/* Flesh out the error a bit */
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
				}
			}
		}
	}

	return cpls;
}
