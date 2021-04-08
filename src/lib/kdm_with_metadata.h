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


#ifndef DCPOMATIC_KDM_WITH_METADATA_H
#define DCPOMATIC_KDM_WITH_METADATA_H


#include <dcp/encrypted_kdm.h>
#include <dcp/name_format.h>


class Cinema;


class KDMWithMetadata
{
public:
	KDMWithMetadata (dcp::NameFormat::Map const& name_values, void const* group, std::list<std::string> emails, dcp::EncryptedKDM kdm)
		: _name_values (name_values)
		, _group (group)
		, _emails (emails)
		, _kdm (kdm)
	{}

	std::string kdm_as_xml () const {
		return _kdm.as_xml ();
	}

	void kdm_as_xml (boost::filesystem::path out) const {
		return _kdm.as_xml (out);
	}

	dcp::NameFormat::Map const& name_values () const {
		return _name_values;
	}

	boost::optional<std::string> get (char k) const;

	void const* group () const {
		return _group;
	}

	std::list<std::string> emails () const {
		return _emails;
	}

private:
	dcp::NameFormat::Map _name_values;
	void const* _group;
	std::list<std::string> _emails;
	dcp::EncryptedKDM _kdm;
};


typedef std::shared_ptr<KDMWithMetadata> KDMWithMetadataPtr;


int write_files (
	std::list<KDMWithMetadataPtr> screen_kdms, boost::filesystem::path directory,
	dcp::NameFormat name_format, boost::function<bool (boost::filesystem::path)> confirm_overwrite
	);


void make_zip_file (std::list<KDMWithMetadataPtr> kdms, boost::filesystem::path zip_file, dcp::NameFormat name_format);


std::list<std::list<KDMWithMetadataPtr>> collect (std::list<KDMWithMetadataPtr> kdms);


int write_directories (
		std::list<std::list<KDMWithMetadataPtr>> kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);


int write_zip_files (
		std::list<std::list<KDMWithMetadataPtr>> kdms,
		boost::filesystem::path directory,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);


void email (
		std::list<std::list<KDMWithMetadataPtr>> kdms,
		dcp::NameFormat container_name_format,
		dcp::NameFormat filename_format,
		std::string cpl_name
		);

#endif

