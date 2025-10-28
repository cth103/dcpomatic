/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include <dcp/certificate_chain.h>
#include <dcp/file.h>


using std::make_shared;
using std::shared_ptr;
using std::string;


void
export_decryption_chain_and_key(shared_ptr<const dcp::CertificateChain> chain, boost::filesystem::path const& path)
{
	dcp::File f(path, "w");
	if (!f) {
		throw OpenFileError(path, f.open_error(), OpenFileError::WRITE);
	}

	auto const certs = chain->chain();
	f.checked_write(certs.c_str(), certs.length());
	auto const key = chain->key();
	DCPOMATIC_ASSERT(key);
	f.checked_write(key->c_str(), key->length());
}


shared_ptr<dcp::CertificateChain>
import_decryption_chain_and_key(boost::filesystem::path const& path)
{
	auto new_chain = make_shared<dcp::CertificateChain>();

	dcp::File f(path, "r");
	if (!f) {
		throw OpenFileError(f.path(), f.open_error(), OpenFileError::WRITE);
	}

	string current;
	while (!f.eof()) {
		char buffer[128];
		if (f.gets(buffer, 128) == 0) {
			break;
		}
		current += buffer;

		if (current.find("-----END CERTIFICATE-----") != string::npos) {
			new_chain->add(dcp::Certificate(current));
			current = "";
		} else if (current.find("-----END") != string::npos && current.find("PRIVATE KEY-----", 29) != string::npos) {
			new_chain->set_key(current);
			current = "";
		}
	}

	if (!new_chain->chain_valid() || !new_chain->private_key_valid()) {
		return {};
	}

	return new_chain;
}

