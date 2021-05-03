/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_TRUSTED_DEVICE_H
#define DCPOMATIC_TRUSTED_DEVICE_H


#include <dcp/certificate.h>
#include <boost/optional.hpp>
#include <string>


class TrustedDevice
{
public:
	explicit TrustedDevice (std::string);
	explicit TrustedDevice (dcp::Certificate);

	boost::optional<dcp::Certificate> certificate () const {
		return _certificate;
	}

	std::string thumbprint () const;
	std::string as_string () const;

private:
	boost::optional<dcp::Certificate> _certificate;
	boost::optional<std::string> _thumbprint;
};


#endif
