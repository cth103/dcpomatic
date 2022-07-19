/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "cross.h"
#include "job.h"
#include "nanomsg.h"

class CopyToDriveJob : public Job
{
public:
	CopyToDriveJob (std::vector<boost::filesystem::path> const& dcps, Drive drive, Nanomsg& nanomsg);

	std::string name () const override;
	std::string json_name () const override;
	void run () override;
	bool enable_notify () const override {
		return true;
	}

private:
	void count (boost::filesystem::path dir, uint64_t& total_bytes);
	void copy (boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total);
	std::vector<boost::filesystem::path> _dcps;
	Drive _drive;
	Nanomsg& _nanomsg;
};
