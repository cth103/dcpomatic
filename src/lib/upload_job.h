/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/upload_job.h
 *  @brief A job to copy DCPs to a server using libcurl.
 */


#include "job.h"


class UploadJob : public Job
{
public:
	explicit UploadJob(std::shared_ptr<const Film>);
	~UploadJob();

	std::string name() const override;
	std::string json_name() const override;
	void run() override;
	std::string status() const override;

private:
	void set_status(std::string);

	mutable boost::mutex _status_mutex;
	std::string _status;
};
