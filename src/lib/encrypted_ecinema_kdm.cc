/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#ifdef DCPOMATIC_VARIANT_SWAROOP

#include "encrypted_ecinema_kdm.h"
#include <dcp/key.h>
#include <dcp/certificate.h>
#include <libxml++/libxml++.h>
#include <openssl/rsa.h>
#include <iostream>

using std::cout;
using std::string;
using boost::shared_ptr;
using dcp::Certificate;

EncryptedECinemaKDM::EncryptedECinemaKDM (dcp::Key content_key, Certificate recipient)
{
	RSA* rsa = recipient.public_key ();
	_content_key = dcp::Data (RSA_size(rsa));
	int const N = RSA_public_encrypt (content_key.length(), content_key.value(), _content_key.data().get(), rsa, RSA_PKCS1_OAEP_PADDING);
}

string
EncryptedECinemaKDM::as_xml () const
{
	string key;

	/* Lazy overallocation */
	char out[_content_key.size() * 2];
	Kumu::base64encode (_content_key.data().get(), _content_key.size(), out, _content_key.size() * 2);
	int const N = strlen (out);
	string lines;
	for (int i = 0; i < N; ++i) {
		if (i > 0 && (i % 64) == 0) {
			lines += "\n";
		}
		lines += out[i];
	}

	xmlpp::Document document;
	xmlpp::Element* root = document.create_root_node ("ECinemaSecurityMesage");
	root->add_child("Key")->add_child_text(lines);
	return document.write_to_string ("UTF-8");
}

#endif
