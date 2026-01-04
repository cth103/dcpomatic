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


#include "exceptions.h"
#include "sqlite_database.h"

#include "i18n.h"


using std::string;
using std::runtime_error;
using boost::optional;


/** @param f File that we were trying to open */
OpenFileError::OpenFileError (boost::filesystem::path f, int error, Mode mode)
	: FileError (
		fmt::format(
			mode == READ_WRITE ? _("could not open file {} for read/write ({})") :
			(mode == READ ? _("could not open file {} for read ({})") : _("could not open file {} for write ({})")),
			f.string(),
			error),
		f
		)
{

}


FileNotFoundError::FileNotFoundError (boost::filesystem::path f)
	: runtime_error(fmt::format("File {} not found", f.string()))
	, _file (f)
{

}


ReadFileError::ReadFileError (boost::filesystem::path f, int e)
	: FileError (fmt::format(_("could not read from file {} ({})"), f.string(), strerror(e)), f)
{

}


WriteFileError::WriteFileError (boost::filesystem::path f, int e)
	: FileError (fmt::format(_("could not write to file {} ({})"), f.string(), strerror(e)), f)
{

}


MissingSettingError::MissingSettingError (string s)
	: SettingError (s, fmt::format(_("Missing required setting {}"), s))
{

}


PixelFormatError::PixelFormatError (string o, AVPixelFormat f)
	: runtime_error (fmt::format(_("Cannot handle pixel format {} during {}"), (int) f, o))
{

}


TextSubtitleError::TextSubtitleError (string saw, string expecting, boost::filesystem::path f)
	: FileError (fmt::format(_("Error in subtitle file: saw {} while expecting {}"), saw.empty() ? "[nothing]" : saw, expecting), f)
{

}


InvalidSignerError::InvalidSignerError ()
	: runtime_error (_("The certificate chain for signing is invalid"))
{

}


InvalidSignerError::InvalidSignerError (string reason)
	: runtime_error (fmt::format(_("The certificate chain for signing is invalid ({})"), reason))
{

}


ProgrammingError::ProgrammingError (string file, int line, string message)
	: runtime_error (fmt::format(_("Programming error at {}:{} {}"), file, line, message))
{

}


KDMAsContentError::KDMAsContentError ()
	: runtime_error (_("This file is a KDM.  KDMs should be added to DCP content by right-clicking the content and choosing \"Add KDM\"."))
{

}


NetworkError::NetworkError (string s, optional<string> d)
	: runtime_error (fmt::format("{}{}", s, d ? fmt::format(" ({})", *d) : ""))
	, _summary (s)
	, _detail (d)
{

}


KDMError::KDMError (string s, string d)
	: runtime_error (fmt::format("{} ({})", s, d))
	, _summary (s)
	, _detail (d)
{

}


GLError::GLError (char const* last, int e)
	: runtime_error (fmt::format("{} failed {}", last, e))
{

}


GLError::GLError (char const* message)
	: runtime_error (message)
{

}


CopyError::CopyError(string m, optional<int> ext4, optional<int> platform)
	: runtime_error(fmt::format("{}{}{}", m, ext4 ? fmt::format(" ({})", *ext4) : "", platform ? fmt::format(" ({})", *platform) : ""))
	, _message (m)
	, _ext4_number(ext4)
	, _platform_number(platform)
{

}


CommunicationFailedError::CommunicationFailedError ()
	: CopyError (_("Lost communication between main and writer processes"))
{

}


VerifyError::VerifyError (string m, int n)
	: runtime_error (fmt::format("{} ({})", m, n))
	, _message (m)
	, _number (n)
{

}


DiskFullError::DiskFullError(boost::filesystem::path writing)
	: std::runtime_error(fmt::format(_("Disk full when writing {}"), writing.string()))
{

}


boost::filesystem::path
SQLError::get_filename(SQLiteDatabase& db)
{
	if (auto filename = sqlite3_db_filename(db.db(), "main")) {
		return filename;
	}

	return {};
}


CPLNotFoundError::CPLNotFoundError(string id)
	: DCPError(fmt::format(_("CPL {} not found"), id))
{

}
