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
#include "decrypted_ecinema_kdm.h"
#include "ecinema_kdm_data.h"
#include "exceptions.h"
#include "compose.hpp"
#include <dcp/key.h>
#include <dcp/util.h>
#include <dcp/certificate.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

using std::string;
using std::runtime_error;
using dcp::Certificate;
using boost::optional;

DecryptedECinemaKDM::DecryptedECinemaKDM (string id, string name, dcp::Key content_key, optional<dcp::LocalTime> not_valid_before, optional<dcp::LocalTime> not_valid_after)
	: _id (id)
	, _name (name)
	, _content_key (content_key)
	, _not_valid_before (not_valid_before)
	, _not_valid_after (not_valid_after)
{

}

DecryptedECinemaKDM::DecryptedECinemaKDM (EncryptedECinemaKDM kdm, string private_key)
	: _id (kdm.id())
	, _name (kdm.name())
{
	/* Read the private key */

	BIO* bio = BIO_new_mem_buf (const_cast<char *> (private_key.c_str()), -1);
	if (!bio) {
		throw runtime_error ("could not create memory BIO");
	}

	RSA* rsa = PEM_read_bio_RSAPrivateKey (bio, 0, 0, 0);
	if (!rsa) {
		throw FileError ("could not read RSA private key file", private_key);
	}

	uint8_t value[RSA_size(rsa)];
	int const len = RSA_private_decrypt (kdm.data().size(), kdm.data().data().get(), value, rsa, RSA_PKCS1_OAEP_PADDING);
	if (len == -1) {
		throw KDMError (ERR_error_string(ERR_get_error(), 0), "");
	}

	if (len != ECINEMA_KDM_KEY_LENGTH && len != (ECINEMA_KDM_KEY_LENGTH + ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH + ECINEMA_KDM_NOT_VALID_AFTER_LENGTH)) {
		throw KDMError (
			"Unexpected data block size in ECinema KDM.",
			String::compose("Size was %1; expected %2 or %3", ECINEMA_KDM_KEY_LENGTH, ECINEMA_KDM_KEY_LENGTH + ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH + ECINEMA_KDM_NOT_VALID_AFTER_LENGTH)
			);
	}

	_content_key = dcp::Key (value + ECINEMA_KDM_KEY, ECINEMA_KDM_KEY_LENGTH);
	if (len > ECINEMA_KDM_KEY_LENGTH) {
		uint8_t* p = value + ECINEMA_KDM_NOT_VALID_BEFORE;
		string b;
		for (int i = 0; i < ECINEMA_KDM_NOT_VALID_BEFORE_LENGTH; ++i) {
			b += *p++;
		}
		_not_valid_before = dcp::LocalTime (b);
		string a;
		for (int i = 0; i < ECINEMA_KDM_NOT_VALID_AFTER_LENGTH; ++i) {
			a += *p++;
		}
		_not_valid_after = dcp::LocalTime (a);
	}
}

EncryptedECinemaKDM
DecryptedECinemaKDM::encrypt (Certificate recipient)
{
	return EncryptedECinemaKDM (_id, _name, _content_key, _not_valid_before, _not_valid_after, recipient);
}

#endif
