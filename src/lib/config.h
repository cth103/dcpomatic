/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

/** @file src/config.h
 *  @brief Class holding configuration.
 */

#ifndef DCPOMATIC_CONFIG_H
#define DCPOMATIC_CONFIG_H

#include "isdcf_metadata.h"
#include "types.h"
#include <dcp/name_format.h>
#include <dcp/certificate_chain.h>
#include <dcp/encrypted_kdm.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>
#include <vector>

class CinemaSoundProcessor;
class DCPContentType;
class Ratio;
class Cinema;
class Film;

/** @class Config
 *  @brief A singleton class holding configuration.
 */
class Config : public boost::noncopyable
{
public:
	/** @return number of threads to use for J2K encoding on the local machine */
	int num_local_encoding_threads () const {
		return _num_local_encoding_threads;
	}

	boost::optional<boost::filesystem::path> default_directory () const {
		return _default_directory;
	}

	boost::optional<boost::filesystem::path> default_kdm_directory () const {
		return _default_kdm_directory;
	}

	boost::filesystem::path default_directory_or (boost::filesystem::path a) const;
	boost::filesystem::path default_kdm_directory_or (boost::filesystem::path a) const;

	enum Property {
		USE_ANY_SERVERS,
		SERVERS,
		CINEMAS,
		PREVIEW_SOUND,
		PREVIEW_SOUND_OUTPUT,
		OTHER
	};

	/** @return base port number to use for J2K encoding servers */
	int server_port_base () const {
		return _server_port_base;
	}

	void set_use_any_servers (bool u) {
		_use_any_servers = u;
		changed (USE_ANY_SERVERS);
	}

	bool use_any_servers () const {
		return _use_any_servers;
	}

	/** @param s New list of servers */
	void set_servers (std::vector<std::string> s) {
		_servers = s;
		changed (SERVERS);
	}

	/** @return Host names / IP addresses of J2K encoding servers that should definitely be used */
	std::vector<std::string> servers () const {
		return _servers;
	}

	bool only_servers_encode () const {
		return _only_servers_encode;
	}

	Protocol tms_protocol () const {
		return _tms_protocol;
	}

	/** @return The IP address of a TMS that we can copy DCPs to */
	std::string tms_ip () const {
		return _tms_ip;
	}

	/** @return The path on a TMS that we should changed DCPs to */
	std::string tms_path () const {
		return _tms_path;
	}

	/** @return User name to log into the TMS with */
	std::string tms_user () const {
		return _tms_user;
	}

	/** @return Password to log into the TMS with */
	std::string tms_password () const {
		return _tms_password;
	}

	/** @return The cinema sound processor that we are using */
	CinemaSoundProcessor const * cinema_sound_processor () const {
		return _cinema_sound_processor;
	}

	std::list<boost::shared_ptr<Cinema> > cinemas () const {
		return _cinemas;
	}

	std::list<int> allowed_dcp_frame_rates () const {
		return _allowed_dcp_frame_rates;
	}

	bool allow_any_dcp_frame_rate () const {
		return _allow_any_dcp_frame_rate;
	}

	ISDCFMetadata default_isdcf_metadata () const {
		return _default_isdcf_metadata;
	}

	boost::optional<std::string> language () const {
		return _language;
	}

	int default_still_length () const {
		return _default_still_length;
	}

	Ratio const * default_container () const {
		return _default_container;
	}

	DCPContentType const * default_dcp_content_type () const {
		return _default_dcp_content_type;
	}

	int default_dcp_audio_channels () const {
		return _default_dcp_audio_channels;
	}

	std::string dcp_issuer () const {
		return _dcp_issuer;
	}

	std::string dcp_creator () const {
		return _dcp_creator;
	}

	int default_j2k_bandwidth () const {
		return _default_j2k_bandwidth;
	}

	int default_audio_delay () const {
		return _default_audio_delay;
	}

	bool default_interop () const {
		return _default_interop;
	}

	void set_default_kdm_directory (boost::filesystem::path d) {
		if (_default_kdm_directory && _default_kdm_directory.get() == d) {
			return;
		}
		_default_kdm_directory = d;
		changed ();
	}

	std::string mail_server () const {
		return _mail_server;
	}

	int mail_port () const {
		return _mail_port;
	}

	std::string mail_user () const {
		return _mail_user;
	}

	std::string mail_password () const {
		return _mail_password;
	}

	std::string kdm_subject () const {
		return _kdm_subject;
	}

	std::string kdm_from () const {
		return _kdm_from;
	}

	std::vector<std::string> kdm_cc () const {
		return _kdm_cc;
	}

	std::string kdm_bcc () const {
		return _kdm_bcc;
	}

	std::string kdm_email () const {
		return _kdm_email;
	}

	boost::shared_ptr<const dcp::CertificateChain> signer_chain () const {
		return _signer_chain;
	}

	boost::shared_ptr<const dcp::CertificateChain> decryption_chain () const {
		return _decryption_chain;
	}

	bool check_for_updates () const {
		return _check_for_updates;
	}

	bool check_for_test_updates () const {
		return _check_for_test_updates;
	}

	int maximum_j2k_bandwidth () const {
		return _maximum_j2k_bandwidth;
	}

	int log_types () const {
		return _log_types;
	}

	bool analyse_ebur128 () const {
		return _analyse_ebur128;
	}

	bool automatic_audio_analysis () const {
		return _automatic_audio_analysis;
	}

#ifdef DCPOMATIC_WINDOWS
	bool win32_console () const {
		return _win32_console;
	}
#endif

	std::vector<boost::filesystem::path> history () const {
		return _history;
	}

	std::vector<dcp::EncryptedKDM> dkdms () const {
		return _dkdms;
	}

	boost::filesystem::path cinemas_file () const {
		return _cinemas_file;
	}

	bool show_hints_before_make_dcp () const {
		return _show_hints_before_make_dcp;
	}

	bool confirm_kdm_email () const {
		return _confirm_kdm_email;
	}

	dcp::NameFormat kdm_container_name_format () const {
		return _kdm_container_name_format;
	}

	dcp::NameFormat kdm_filename_format () const {
		return _kdm_filename_format;
	}

	dcp::NameFormat dcp_metadata_filename_format () const {
		return _dcp_metadata_filename_format;
	}

	dcp::NameFormat dcp_asset_filename_format () const {
		return _dcp_asset_filename_format;
	}

	bool jump_to_selected () const {
		return _jump_to_selected;
	}

	bool preview_sound () const {
		return _preview_sound;
	}

	boost::optional<std::string> preview_sound_output () const {
		return _preview_sound_output;
	}

	/** @param n New number of local encoding threads */
	void set_num_local_encoding_threads (int n) {
		maybe_set (_num_local_encoding_threads, n);
	}

	void set_default_directory (boost::filesystem::path d) {
		if (_default_directory && *_default_directory == d) {
			return;
		}
		_default_directory = d;
		changed ();
	}

	/** @param p New server port */
	void set_server_port_base (int p) {
		maybe_set (_server_port_base, p);
	}

	void set_only_servers_encode (bool o) {
		maybe_set (_only_servers_encode, o);
	}

	void set_tms_protocol (Protocol p) {
		maybe_set (_tms_protocol, p);
	}

	/** @param i IP address of a TMS that we can copy DCPs to */
	void set_tms_ip (std::string i) {
		maybe_set (_tms_ip, i);
	}

	/** @param p Path on a TMS that we should changed DCPs to */
	void set_tms_path (std::string p) {
		maybe_set (_tms_path, p);
	}

	/** @param u User name to log into the TMS with */
	void set_tms_user (std::string u) {
		maybe_set (_tms_user, u);
	}

	/** @param p Password to log into the TMS with */
	void set_tms_password (std::string p) {
		maybe_set (_tms_password, p);
	}

	void add_cinema (boost::shared_ptr<Cinema> c) {
		_cinemas.push_back (c);
		changed (CINEMAS);
	}

	void remove_cinema (boost::shared_ptr<Cinema> c) {
		_cinemas.remove (c);
		changed (CINEMAS);
	}

	void set_allowed_dcp_frame_rates (std::list<int> const & r) {
		maybe_set (_allowed_dcp_frame_rates, r);
	}

	void set_allow_any_dcp_frame_rate (bool a) {
		maybe_set (_allow_any_dcp_frame_rate, a);
	}

	void set_default_isdcf_metadata (ISDCFMetadata d) {
		maybe_set (_default_isdcf_metadata, d);
	}

	void set_language (std::string l) {
		if (_language && _language.get() == l) {
			return;
		}
		_language = l;
		changed ();
	}

	void unset_language () {
		if (!_language) {
			return;
		}

		_language = boost::none;
		changed ();
	}

	void set_default_still_length (int s) {
		maybe_set (_default_still_length, s);
	}

	void set_default_container (Ratio const * c) {
		maybe_set (_default_container, c);
	}

	void set_default_dcp_content_type (DCPContentType const * t) {
		maybe_set (_default_dcp_content_type, t);
	}

	void set_default_dcp_audio_channels (int c) {
		maybe_set (_default_dcp_audio_channels, c);
	}

	void set_dcp_issuer (std::string i) {
		maybe_set (_dcp_issuer, i);
	}

	void set_dcp_creator (std::string c) {
		maybe_set (_dcp_creator, c);
	}

	void set_default_j2k_bandwidth (int b) {
		maybe_set (_default_j2k_bandwidth, b);
	}

	void set_default_audio_delay (int d) {
		maybe_set (_default_audio_delay, d);
	}

	void set_default_interop (bool i) {
		maybe_set (_default_interop, i);
	}

	void set_mail_server (std::string s) {
		maybe_set (_mail_server, s);
	}

	void set_mail_port (int p) {
		maybe_set (_mail_port, p);
	}

	void set_mail_user (std::string u) {
		maybe_set (_mail_user, u);
	}

	void set_mail_password (std::string p) {
		maybe_set (_mail_password, p);
	}

	void set_kdm_subject (std::string s) {
		maybe_set (_kdm_subject, s);
	}

	void set_kdm_from (std::string f) {
		maybe_set (_kdm_from, f);
	}

	void set_kdm_cc (std::vector<std::string> f) {
		maybe_set (_kdm_cc, f);
	}

	void set_kdm_bcc (std::string f) {
		maybe_set (_kdm_bcc, f);
	}

	void set_kdm_email (std::string e) {
		maybe_set (_kdm_email, e);
	}

	void reset_kdm_email ();

	void set_signer_chain (boost::shared_ptr<const dcp::CertificateChain> s) {
		maybe_set (_signer_chain, s);
	}

	void set_decryption_chain (boost::shared_ptr<const dcp::CertificateChain> c) {
		maybe_set (_decryption_chain, c);
	}

	void set_check_for_updates (bool c) {
		maybe_set (_check_for_updates, c);
		if (!c) {
			set_check_for_test_updates (false);
		}
	}

	void set_check_for_test_updates (bool c) {
		maybe_set (_check_for_test_updates, c);
	}

	void set_maximum_j2k_bandwidth (int b) {
		maybe_set (_maximum_j2k_bandwidth, b);
	}

	void set_log_types (int t) {
		maybe_set (_log_types, t);
	}

	void set_analyse_ebur128 (bool a) {
		maybe_set (_analyse_ebur128, a);
	}

	void set_automatic_audio_analysis (bool a) {
		maybe_set (_automatic_audio_analysis, a);
	}

#ifdef DCPOMATIC_WINDOWS
	void set_win32_console (bool c) {
		maybe_set (_win32_console, c);
	}
#endif

	void set_dkdms (std::vector<dcp::EncryptedKDM> dkdms) {
		_dkdms = dkdms;
		changed ();
	}

	void set_cinemas_file (boost::filesystem::path file);

	void set_show_hints_before_make_dcp (bool s) {
		maybe_set (_show_hints_before_make_dcp, s);
	}

	void set_confirm_kdm_email (bool s) {
		maybe_set (_confirm_kdm_email, s);
	}

	void set_preview_sound (bool s) {
		maybe_set (_preview_sound, s, PREVIEW_SOUND);
	}

	void set_preview_sound_output (std::string o)
	{
		maybe_set (_preview_sound_output, o, PREVIEW_SOUND_OUTPUT);
	}

	void unset_preview_sound_output ()
	{
		if (!_preview_sound_output) {
			return;
		}

		_preview_sound_output = boost::none;
		changed ();
	}

	void set_kdm_container_name_format (dcp::NameFormat n) {
		maybe_set (_kdm_container_name_format, n);
	}

	void set_kdm_filename_format (dcp::NameFormat n) {
		maybe_set (_kdm_filename_format, n);
	}

	void set_dcp_metadata_filename_format (dcp::NameFormat n) {
		maybe_set (_dcp_metadata_filename_format, n);
	}

	void set_dcp_asset_filename_format (dcp::NameFormat n) {
		maybe_set (_dcp_asset_filename_format, n);
	}

	void clear_history () {
		_history.clear ();
		changed ();
	}

	void add_to_history (boost::filesystem::path p);

	void set_jump_to_selected (bool j) {
		maybe_set (_jump_to_selected, j);
	}

	void changed (Property p = OTHER);
	boost::signals2::signal<void (Property)> Changed;
	/** Emitted if read() failed on an existing Config file.  There is nothing
	    a listener can do about it: this is just for information.
	*/
	static boost::signals2::signal<void ()> FailedToLoad;

	void write () const;
	void write_config () const;
	void write_cinemas () const;

	void save_template (boost::shared_ptr<const Film> film, std::string name) const;
	bool existing_template (std::string name) const;
	std::list<std::string> templates () const;
	boost::filesystem::path template_path (std::string name) const;
	void rename_template (std::string old_name, std::string new_name) const;
	void delete_template (std::string name) const;

	static Config* instance ();
	static void drop ();
	static void restore_defaults ();
	static bool have_existing (std::string);

private:
	Config ();
	static boost::filesystem::path path (std::string file, bool create_directories = true);
	void read ();
	void set_defaults ();
	void set_kdm_email_to_default ();
	void read_cinemas (cxml::Document const & f);
	boost::shared_ptr<dcp::CertificateChain> create_certificate_chain ();
	boost::filesystem::path directory_or (boost::optional<boost::filesystem::path> dir, boost::filesystem::path a) const;

	template <class T>
	void maybe_set (T& member, T new_value, Property prop = OTHER) {
		if (member == new_value) {
			return;
		}
		member = new_value;
		changed (prop);
	}

	template <class T>
	void maybe_set (boost::optional<T>& member, T new_value, Property prop = OTHER) {
		if (member && member.get() == new_value) {
			return;
		}
		member = new_value;
		changed (prop);
	}

	/** number of threads to use for J2K encoding on the local machine */
	int _num_local_encoding_threads;
	/** default directory to put new films in */
	boost::optional<boost::filesystem::path> _default_directory;
	/** base port number to use for J2K encoding servers;
	 *  this port and the two above it will be used.
	 */
	int _server_port_base;
	/** true to broadcast on the `any' address to look for servers */
	bool _use_any_servers;
	/** J2K encoding servers that should definitely be used */
	std::vector<std::string> _servers;
	bool _only_servers_encode;
	Protocol _tms_protocol;
	/** The IP address of a TMS that we can copy DCPs to */
	std::string _tms_ip;
	/** The path on a TMS that we should write DCPs to */
	std::string _tms_path;
	/** User name to log into the TMS with */
	std::string _tms_user;
	/** Password to log into the TMS with */
	std::string _tms_password;
	/** Our cinema sound processor */
	CinemaSoundProcessor const * _cinema_sound_processor;
	std::list<int> _allowed_dcp_frame_rates;
	/** Allow any video frame rate for the DCP; if true, overrides _allowed_dcp_frame_rates */
	bool _allow_any_dcp_frame_rate;
	/** Default ISDCF metadata for newly-created Films */
	ISDCFMetadata _default_isdcf_metadata;
	boost::optional<std::string> _language;
 	/** Default length of still image content (seconds) */
	int _default_still_length;
	Ratio const * _default_container;
	DCPContentType const * _default_dcp_content_type;
	int _default_dcp_audio_channels;
	std::string _dcp_issuer;
	std::string _dcp_creator;
	int _default_j2k_bandwidth;
	int _default_audio_delay;
	bool _default_interop;
	/** Default directory to offer to write KDMs to; if it's not set,
	    the home directory will be offered.
	*/
	boost::optional<boost::filesystem::path> _default_kdm_directory;
	std::list<boost::shared_ptr<Cinema> > _cinemas;
	std::string _mail_server;
	int _mail_port;
	std::string _mail_user;
	std::string _mail_password;
	std::string _kdm_subject;
	std::string _kdm_from;
	std::vector<std::string> _kdm_cc;
	std::string _kdm_bcc;
	std::string _kdm_email;
	boost::shared_ptr<const dcp::CertificateChain> _signer_chain;
	/** Chain used to decrypt KDMs; the leaf of this chain is the target
	 *  certificate for making KDMs given to DCP-o-matic.
	 */
	boost::shared_ptr<const dcp::CertificateChain> _decryption_chain;
	/** true to check for updates on startup */
	bool _check_for_updates;
	bool _check_for_test_updates;
	/** maximum allowed J2K bandwidth in bits per second */
	int _maximum_j2k_bandwidth;
	int _log_types;
	bool _analyse_ebur128;
	bool _automatic_audio_analysis;
#ifdef DCPOMATIC_WINDOWS
	bool _win32_console;
#endif
	std::vector<boost::filesystem::path> _history;
	std::vector<dcp::EncryptedKDM> _dkdms;
	boost::filesystem::path _cinemas_file;
	bool _show_hints_before_make_dcp;
	bool _confirm_kdm_email;
	dcp::NameFormat _kdm_filename_format;
	dcp::NameFormat _kdm_container_name_format;
	dcp::NameFormat _dcp_metadata_filename_format;
	dcp::NameFormat _dcp_asset_filename_format;
	bool _jump_to_selected;
	bool _preview_sound;
	/** name of a specific sound output stream to use for preview, or empty to use the default */
	boost::optional<std::string> _preview_sound_output;

	/** Singleton instance, or 0 */
	static Config* _instance;
};

#endif
