/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "job.h"
#include <dcp/verify.h>


class Content;


class VerifyDCPJob : public Job
{
public:
	VerifyDCPJob(
		std::vector<boost::filesystem::path> directories,
		std::vector<boost::filesystem::path> kdms,
		dcp::VerificationOptions options
	);

	VerifyDCPJob(
		std::vector<boost::filesystem::path> directories,
		std::vector<dcp::DecryptedKDM> kdms,
		dcp::VerificationOptions options
	);

	~VerifyDCPJob();

	std::string name() const override;
	std::string json_name() const override;
	void run() override;

	dcp::VerificationResult const& result() const {
		return _result;
	}

	std::vector<boost::filesystem::path> directories() const {
		return _directories;
	}

private:
	void update_stage(std::string s, boost::optional<boost::filesystem::path> path);

	std::vector<boost::filesystem::path> _directories;
	std::vector<dcp::DecryptedKDM> _kdms;
	dcp::VerificationOptions _options;
	dcp::VerificationResult _result;
};
