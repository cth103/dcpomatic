/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "send_notification_email_job.h"
#include "exceptions.h"
#include "config.h"
#include "emailer.h"
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <list>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;

/** @param body Email body */
SendNotificationEmailJob::SendNotificationEmailJob (string body)
	: Job (shared_ptr<Film>())
	, _body (body)
{

}

string
SendNotificationEmailJob::name () const
{
	return _("Email notification");
}

string
SendNotificationEmailJob::json_name () const
{
	return N_("send_notification_email");
}

void
SendNotificationEmailJob::run ()
{
	Config* config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	set_progress_unknown ();
	list<string> to;
	to.push_back (config->notification_to ());
	Emailer email (config->notification_from(), to, config->notification_subject(), _body);
	BOOST_FOREACH (string i, config->notification_cc()) {
		email.add_cc (i);
	}
	if (!config->notification_bcc().empty()) {
		email.add_bcc (config->notification_bcc ());
	}

	email.send (config->mail_server(), config->mail_port(), config->mail_user(), config->mail_password());

	set_progress (1);
	set_state (FINISHED_OK);
}
