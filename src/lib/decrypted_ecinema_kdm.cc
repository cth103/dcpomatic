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
#include "exceptions.h"
#include <dcp/key.h>
#include <dcp/util.h>
#include <dcp/certificate.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

using std::string;
using std::runtime_error;
using dcp::Certificate;

DecryptedECinemaKDM::DecryptedECinemaKDM (string id, dcp::Key content_key)
	: _id (id)
	, _content_key (content_key)
{

}

DecryptedECinemaKDM::DecryptedECinemaKDM (EncryptedECinemaKDM kdm, string private_key)
	: _id (kdm.id())
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
	int const len = RSA_private_decrypt (kdm.key().size(), kdm.key().data().get(), value, rsa, RSA_PKCS1_OAEP_PADDING);
	if (len == -1) {
		throw KDMError (ERR_error_string(ERR_get_error(), 0), "");
	}

	_content_key = dcp::Key (value, len);
}

EncryptedECinemaKDM
DecryptedECinemaKDM::encrypt (Certificate recipient)
{
	return EncryptedECinemaKDM (_id, _content_key, recipient);
}

#endif
