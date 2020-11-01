/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

/* Based on code from https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption */

#include "crypto.h"
#include "exceptions.h"
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <boost/scoped_array.hpp>

using std::string;
using namespace dcpomatic;

/** The cipher that this code uses */
#define CIPHER EVP_aes_256_cbc()

dcp::ArrayData
dcpomatic::random_iv ()
{
	EVP_CIPHER const * cipher = CIPHER;
	dcp::ArrayData iv (EVP_CIPHER_iv_length(cipher));
	RAND_bytes (iv.data(), iv.size());
	return iv;
}

dcp::ArrayData
dcpomatic::encrypt (string plaintext, dcp::ArrayData key, dcp::ArrayData iv)
{
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new ();
	if (!ctx) {
		throw CryptoError ("could not create cipher context");
	}

	int r = EVP_EncryptInit_ex (ctx, CIPHER, 0, key.data(), iv.data());
	if (r != 1) {
		throw CryptoError ("could not initialise cipher context for encryption");
	}

	dcp::ArrayData ciphertext (plaintext.size() * 2);

	int len;
	r = EVP_EncryptUpdate (ctx, ciphertext.data(), &len, (uint8_t const *) plaintext.c_str(), plaintext.size());
	if (r != 1) {
		throw CryptoError ("could not encrypt data");
	}

	int ciphertext_len = len;

	r = EVP_EncryptFinal_ex (ctx, ciphertext.data() + len, &len);
	if (r != 1) {
		throw CryptoError ("could not finish encryption");
	}

	ciphertext.set_size (ciphertext_len + len);

	EVP_CIPHER_CTX_free (ctx);

	return ciphertext;
}

string
dcpomatic::decrypt (dcp::ArrayData ciphertext, dcp::ArrayData key, dcp::ArrayData iv)
{
	EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new ();
	if (!ctx) {
		throw CryptoError ("could not create cipher context");
	}

	int r = EVP_DecryptInit_ex (ctx, CIPHER, 0, key.data(), iv.data());
	if (r != 1) {
		throw CryptoError ("could not initialise cipher context for decryption");
	}

	dcp::ArrayData plaintext (ciphertext.size() * 2);

	int len;
	r = EVP_DecryptUpdate (ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size());
	if (r != 1) {
		throw CryptoError ("could not decrypt data");
	}

	int plaintext_len = len;

	r = EVP_DecryptFinal_ex (ctx, plaintext.data() + len, &len);
	if (r != 1) {
		throw CryptoError ("could not finish decryption");
	}

	plaintext_len += len;
	plaintext.set_size (plaintext_len + 1);
	plaintext.data()[plaintext_len] = '\0';

	EVP_CIPHER_CTX_free (ctx);

	return string ((char *) plaintext.data());
}

int
dcpomatic::crypto_key_length ()
{
	return EVP_CIPHER_key_length (CIPHER);
}
