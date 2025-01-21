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


#include "config.h"
#include "content.h"
#include "cross.h"
#include "verify_dcp_job.h"

#include "i18n.h"


using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


VerifyDCPJob::VerifyDCPJob(vector<boost::filesystem::path> directories, vector<boost::filesystem::path> kdms)
	, _directories (directories)
	: Job({})
	, _kdms(kdms)
{

}


VerifyDCPJob::~VerifyDCPJob ()
{
	stop_thread ();
}


string
VerifyDCPJob::name () const
{
	return _("Verify DCP");
}


string
VerifyDCPJob::json_name () const
{
	return N_("verify_dcp");
}


void
VerifyDCPJob::update_stage (string s, optional<boost::filesystem::path> path)
{
	if (path) {
		s += ": " + path->string();
	}
	sub (s);
}


void
VerifyDCPJob::run ()
{
	vector<dcp::DecryptedKDM> decrypted_kdms;
	auto key = Config::instance()->decryption_chain()->key();
	if (key) {
		for (auto kdm: _kdms) {
			decrypted_kdms.push_back(dcp::DecryptedKDM{dcp::EncryptedKDM(dcp::file_to_string(kdm)), *key});
		}
	}

	_result = dcp::verify(
		_directories,
		decrypted_kdms,
		bind(&VerifyDCPJob::update_stage, this, _1, _2),
		bind(&VerifyDCPJob::set_progress, this, _1, false),
		{},
		libdcp_resources_path() / "xsd"
		);

	bool failed = false;
	for (auto i: _result.notes) {
		if (i.type() == dcp::VerificationNote::Type::ERROR) {
			failed = true;
		}
	}

	set_progress (1);
	set_state (failed ? FINISHED_ERROR : FINISHED_OK);
}
