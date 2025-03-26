/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "cinema_list.h"
#include "config.h"
#include "dkdm_recipient.h"
#include "film.h"
#include "kdm_with_metadata.h"


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


KDMWithMetadataPtr
kdm_for_dkdm_recipient (
	shared_ptr<const Film> film,
	boost::filesystem::path cpl,
	DKDMRecipient const& recipient
	)
{
	if (!recipient.recipient()) {
		return {};
	}

	auto signer = Config::instance()->signer_chain();
	if (!signer->valid()) {
		throw InvalidSignerError();
	}

	auto start = signer->leaf().not_before();
	start.add_days(1);
	auto end = signer->leaf().not_after();
	end.add_days(-1);

	auto const decrypted_kdm = film->make_kdm(cpl, start, end);
	auto const kdm = decrypted_kdm.encrypt(signer, recipient.recipient().get(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, true, 0);

	dcp::NameFormat::Map name_values;
	name_values['f'] = kdm.content_title_text();
	name_values['r'] = recipient.name;
	name_values['i'] = kdm.cpl_id();

	return make_shared<KDMWithMetadata>(name_values, CinemaID(0), recipient.emails, kdm);
}

