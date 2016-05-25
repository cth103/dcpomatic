/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "uploader.h"
#include <libssh/libssh.h>

class SCPUploader : public Uploader
{
public:
	SCPUploader (boost::function<void (std::string)> set_status, boost::function<void (float)> set_progress);
	~SCPUploader ();

protected:
	virtual void create_directory (boost::filesystem::path directory);
	virtual void upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size);

private:
	ssh_session _session;
	ssh_scp _scp;
};
