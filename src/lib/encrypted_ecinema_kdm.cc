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
#include <openssl/rsa.h>
#include <iostream>

using std::cout;
using boost::shared_ptr;
using dcp::Certificate;

EncryptedECinemaKDM::EncryptedECinemaKDM (dcp::Key content_key, Certificate recipient)
{
	RSA* rsa = recipient.public_key ();
	dcp::Data encrypted (RSA_size(rsa));
	int const N = RSA_public_encrypt (content_key.length(), content_key.value(), encrypted.data().get(), rsa, RSA_PKCS1_OAEP_PADDING);
}

#endif
