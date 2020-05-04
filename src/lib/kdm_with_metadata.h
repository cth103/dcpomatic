/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_KDM_WITH_METADATA_H
#define DCPOMATIC_KDM_WITH_METADATA_H

#ifdef DCPOMATIC_VARIANT_SWAROOP
#include "encrypted_ecinema_kdm.h"
#endif
#include <dcp/encrypted_kdm.h>
#include <dcp/name_format.h>
#include <boost/shared_ptr.hpp>

namespace dcpomatic {
	class Screen;
}

/** Simple class to collect a screen and an encrypted KDM */
class KDMWithMetadata
{
public:
	KDMWithMetadata (boost::shared_ptr<dcpomatic::Screen> s)
		: screen (s)
	{}

	virtual ~KDMWithMetadata () {}

	virtual std::string kdm_as_xml () const = 0;
	virtual void kdm_as_xml (boost::filesystem::path out) const = 0;
	virtual std::string kdm_id () const = 0;

	boost::shared_ptr<dcpomatic::Screen> screen;
};


typedef boost::shared_ptr<KDMWithMetadata> KDMWithMetadataPtr;


int write_files (
	std::list<KDMWithMetadataPtr> screen_kdms, boost::filesystem::path directory,
	dcp::NameFormat name_format, dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	);


class DCPKDMWithMetadata : public KDMWithMetadata
{
public:
	DCPKDMWithMetadata (boost::shared_ptr<dcpomatic::Screen> s, dcp::EncryptedKDM k)
		: KDMWithMetadata (s)
		, kdm (k)
	{}

	std::string kdm_as_xml () const {
		return kdm.as_xml ();
	}

	void kdm_as_xml (boost::filesystem::path out) const {
		return kdm.as_xml (out);
	}

	std::string kdm_id () const {
		return kdm.cpl_id ();
	}

	dcp::EncryptedKDM kdm;
};

#ifdef DCPOMATIC_VARIANT_SWAROOP
class ECinemaKDMWithMetadata : public KDMWithMetadata
{
public:
	ECinemaKDMWithMetadata (boost::shared_ptr<dcpomatic::Screen> s, EncryptedECinemaKDM k)
		: KDMWithMetadata (s)
		, kdm (k)
	{}

	std::string kdm_as_xml () const {
		return kdm.as_xml ();
	}

	void kdm_as_xml (boost::filesystem::path out) const {
		return kdm.as_xml (out);
	}

	std::string kdm_id () const {
		return kdm.id ();
	}

	EncryptedECinemaKDM kdm;
};
#endif

#endif
