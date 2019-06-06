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
#include "ecinema_kdm_data.h"
#include "exceptions.h"
#include "cross.h"
#include <dcp/key.h>
#include <dcp/certificate.h>
#include <dcp/util.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <iostream>

using std::cout;
using std::string;
using boost::shared_ptr;
using boost::optional;
using dcp::Certificate;

EncryptedECinemaKDM::EncryptedECinemaKDM (string id, string name, dcp::Key content_key, optional<dcp::LocalTime> not_valid_before, optional<dcp::LocalTime> not_valid_after, Certificate recipient)
	: _id (id)
	, _name (name)
{
	RSA* rsa = recipient.public_key ();
	_data = dcp::Data (RSA_size(rsa));

	int input_size = ECINEMA_KDM_KEY_LENGTH;
	if (not_valid_before && not_valid_after) {
		input_size += ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH + ECINEMA_KDM_NOT_VALID_AFTER_LENGTH;
	}

	dcp::Data input (input_size);
	memcpy (input.data().get(), content_key.value(), ECINEMA_KDM_KEY_LENGTH);
	if (not_valid_before && not_valid_after) {
		memcpy (input.data().get() + ECINEMA_KDM_NOT_VALID_BEFORE, not_valid_before->as_string().c_str(), ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH);
		memcpy (input.data().get() + ECINEMA_KDM_NOT_VALID_AFTER, not_valid_after->as_string().c_str(), ECINEMA_KDM_NOT_VALID_AFTER_LENGTH);
	}

	int const N = RSA_public_encrypt (input_size, input.data().get(), _data.data().get(), rsa, RSA_PKCS1_OAEP_PADDING);
	if (N == -1) {
		throw KDMError ("Could not encrypt ECinema KDM", ERR_error_string(ERR_get_error(), 0));
	}

}

EncryptedECinemaKDM::EncryptedECinemaKDM (string xml)
{
	cxml::Document doc ("ECinemaSecurityMessage");
	doc.read_string (xml);
	_id = doc.string_child ("Id");
	_name = doc.string_child ("Name");
	_data = dcp::Data (256);
	int const len = dcp::base64_decode (doc.string_child("Data"), _data.data().get(), _data.size());
	_data.set_size (len);
}

string
EncryptedECinemaKDM::as_xml () const
{
	string key;

	/* Lazy overallocation */
	char out[_data.size() * 2];
	Kumu::base64encode (_data.data().get(), _data.size(), out, _data.size() * 2);
	int const N = strlen (out);
	string lines;
	for (int i = 0; i < N; ++i) {
		if (i > 0 && (i % 64) == 0) {
			lines += "\n";
		}
		lines += out[i];
	}

	xmlpp::Document document;
	xmlpp::Element* root = document.create_root_node ("ECinemaSecurityMessage");
	root->add_child("Id")->add_child_text(_id);
	root->add_child("Name")->add_child_text(_name);
	root->add_child("Data")->add_child_text(lines);
	return document.write_to_string ("UTF-8");
}

void
EncryptedECinemaKDM::as_xml (boost::filesystem::path path) const
{
	FILE* f = fopen_boost (path, "w");
	string const x = as_xml ();
	fwrite (x.c_str(), 1, x.length(), f);
	fclose (f);
}

#endif
