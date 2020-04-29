/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "trusted_device.h"

using std::string;

TrustedDevice::TrustedDevice (string thumbprint)
	: _thumbprint (thumbprint)
{

}

TrustedDevice::TrustedDevice (dcp::Certificate certificate)
	: _certificate (certificate)
{

}

string
TrustedDevice::as_string () const
{
	if (_certificate) {
		return _certificate->certificate(true);
	}

	return *_thumbprint;
}

string
TrustedDevice::thumbprint () const
{
	if (_certificate) {
		return _certificate->thumbprint ();
	}

	return *_thumbprint;
}

