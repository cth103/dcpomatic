/*
    Copyright (C) 2012-2022 Carl Hetherington <cth@carlh.net>

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


#include "audio_mapping.h"
#include "enum_indexed_vector.h"
#include "export_config.h"
#include "rough_duration.h"
#include "state.h"
#include "video_encoding.h"
#include <dcp/name_format.h>
#include <dcp/certificate_chain.h>
#include <dcp/encrypted_kdm.h>
#include <dcp/language_tag.h>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>
#include <vector>


class Cinema;
class CinemaSoundProcessor;
class DCPContentType;
class DKDMGroup;
class DKDMRecipient;
class Film;
class Ratio;

#undef IGNORE


extern void save_all_config_as_zip(boost::filesystem::path zip_file);


/** @class Config
 *  @brief A singleton class holding configuration.
 */
class Config : public State
{
public:
	/** @return number of threads which a master DoM should use for J2K encoding on the local machine */
	int master_encoding_threads() const {
		return _master_encoding_threads;
	}

	/** @return number of threads which a server should use for J2K encoding on the local machine */
	int server_encoding_threads() const {
		return _server_encoding_threads;
	}

	boost::optional<boost::filesystem::path> default_directory() const {
		return _default_directory;
	}

	boost::optional<boost::filesystem::path> default_kdm_directory() const {
		return _default_kdm_directory;
	}

	boost::filesystem::path default_directory_or(boost::filesystem::path a) const;
	boost::filesystem::path default_kdm_directory_or(boost::filesystem::path a) const;

	enum class CinemasAction
	{
		/** Copy the cinemas.{xml,sqlite3} in the ZIP file to the path
		 *  specified in the current config, overwriting whatever is there,
		 *  and use that path.
		 */
		WRITE_TO_CURRENT_PATH,
		/** Copy the cinemas.{xml,sqlite3} in the ZIP file over the path
		 *  specified in the config.xml from the ZIP, overwriting whatever
		 *  is there and creating any required directories, and use
		 *  that path.
		 */
		WRITE_TO_PATH_IN_ZIPPED_CONFIG,
		/** Do nothing with the cinemas.{xml,sqlite3} in the ZIP file */
		IGNORE
	};

	void load_from_zip(boost::filesystem::path zip_file, CinemasAction action);

	enum Property {
		USE_ANY_SERVERS,
		SERVERS,
		SOUND,
		SOUND_OUTPUT,
		PLAYER_CONTENT_DIRECTORY,
		PLAYER_PLAYLIST_DIRECTORY,
		PLAYER_DEBUG_LOG,
		KDM_DEBUG_LOG,
		HISTORY,
		SHOW_EXPERIMENTAL_AUDIO_PROCESSORS,
		AUDIO_MAPPING,
		AUTO_CROP_THRESHOLD,
		ALLOW_SMPTE_BV20,
		ISDCF_NAME_PART_LENGTH,
		ALLOW_ANY_CONTAINER,
#ifdef DCPOMATIC_GROK
		GROK,
#endif
		CINEMAS_FILE,
		OTHER
	};

	/** @return base port number to use for J2K encoding servers */
	int server_port_base() const {
		return _server_port_base;
	}

	void set_use_any_servers(bool u) {
		_use_any_servers = u;
		changed(USE_ANY_SERVERS);
	}

	bool use_any_servers() const {
		return _use_any_servers;
	}

	/** @param s New list of servers */
	void set_servers(std::vector<std::string> s) {
		_servers = s;
		changed(SERVERS);
	}

	/** @return Host names / IP addresses of J2K encoding servers that should definitely be used */
	std::vector<std::string> servers() const {
		return _servers;
	}

	bool only_servers_encode() const {
		return _only_servers_encode;
	}

	FileTransferProtocol tms_protocol() const {
		return _tms_protocol;
	}

	bool tms_passive() const {
		return _tms_passive;
	}

	/** @return The IP address of a TMS that we can copy DCPs to */
	std::string tms_ip() const {
		return _tms_ip;
	}

	/** @return The path on a TMS that we should changed DCPs to */
	std::string tms_path() const {
		return _tms_path;
	}

	/** @return User name to log into the TMS with */
	std::string tms_user() const {
		return _tms_user;
	}

	/** @return Password to log into the TMS with */
	std::string tms_password() const {
		return _tms_password;
	}

	std::list<int> allowed_dcp_frame_rates() const {
		return _allowed_dcp_frame_rates;
	}

	bool allow_any_dcp_frame_rate() const {
		return _allow_any_dcp_frame_rate;
	}

	bool allow_any_container() const {
		return _allow_any_container;
	}

	bool allow_96khz_audio() const {
		return _allow_96khz_audio;
	}

	bool use_all_audio_channels() const {
		return _use_all_audio_channels;
	}

	bool show_experimental_audio_processors() const {
		return _show_experimental_audio_processors;
	}

	boost::optional<std::string> language() const {
		return _language;
	}

	int default_still_length() const {
		return _default_still_length;
	}

	DCPContentType const * default_dcp_content_type() const {
		return _default_dcp_content_type;
	}

	int default_dcp_audio_channels() const {
		return _default_dcp_audio_channels;
	}

	std::string dcp_issuer() const {
		return _dcp_issuer;
	}

	std::string dcp_creator() const {
		return _dcp_creator;
	}

	std::string dcp_company_name() const {
		return _dcp_company_name;
	}

	std::string dcp_product_name() const {
		return _dcp_product_name;
	}

	std::string dcp_product_version() const {
		return _dcp_product_version;
	}

	std::string dcp_j2k_comment() const {
		return _dcp_j2k_comment;
	}

	int64_t default_video_bit_rate(VideoEncoding encoding) const {
		return _default_video_bit_rate[encoding];
	}

	int default_audio_delay() const {
		return _default_audio_delay;
	}

	bool default_interop() const {
		return _default_interop;
	}

	boost::optional<dcp::LanguageTag> default_audio_language() const {
		return _default_audio_language;
	}

	boost::optional<dcp::LanguageTag::RegionSubtag> default_territory() const {
		return _default_territory;
	}

	std::map<std::string, std::string> default_metadata() const {
		return _default_metadata;
	}

	bool upload_after_make_dcp() {
		return _upload_after_make_dcp;
	}

	void set_default_kdm_directory(boost::filesystem::path d) {
		if (_default_kdm_directory && _default_kdm_directory.get() == d) {
			return;
		}
		_default_kdm_directory = d;
		changed();
	}

	std::string mail_server() const {
		return _mail_server;
	}

	int mail_port() const {
		return _mail_port;
	}

	EmailProtocol mail_protocol() const {
		return _mail_protocol;
	}

	std::string mail_user() const {
		return _mail_user;
	}

	std::string mail_password() const {
		return _mail_password;
	}

	std::string kdm_subject() const {
		return _kdm_subject;
	}

	std::string kdm_from() const {
		return _kdm_from;
	}

	std::vector<std::string> kdm_cc() const {
		return _kdm_cc;
	}

	std::string kdm_bcc() const {
		return _kdm_bcc;
	}

	std::string kdm_email() const {
		return _kdm_email;
	}

	std::string notification_subject() const {
		return _notification_subject;
	}

	std::string notification_from() const {
		return _notification_from;
	}

	std::string notification_to() const {
		return _notification_to;
	}

	std::vector<std::string> notification_cc() const {
		return _notification_cc;
	}

	std::string notification_bcc() const {
		return _notification_bcc;
	}

	std::string notification_email() const {
		return _notification_email;
	}

	std::shared_ptr<const dcp::CertificateChain> signer_chain() const {
		return _signer_chain;
	}

	std::shared_ptr<const dcp::CertificateChain> decryption_chain() const {
		return _decryption_chain;
	}

	bool check_for_updates() const {
		return _check_for_updates;
	}

	bool check_for_test_updates() const {
		return _check_for_test_updates;
	}

	int64_t maximum_video_bit_rate(VideoEncoding encoding) const {
		return _maximum_video_bit_rate[encoding];
	}

	int log_types() const {
		return _log_types;
	}

	bool analyse_ebur128() const {
		return _analyse_ebur128;
	}

	bool automatic_audio_analysis() const {
		return _automatic_audio_analysis;
	}

#ifdef DCPOMATIC_WINDOWS
	bool win32_console() const {
		return _win32_console;
	}
#endif

	std::vector<boost::filesystem::path> history() const {
		return _history;
	}

	std::vector<boost::filesystem::path> player_history() const {
		return _player_history;
	}

	std::shared_ptr<DKDMGroup> dkdms() const {
		return _dkdms;
	}

	boost::filesystem::path cinemas_file() const;

	boost::filesystem::path dkdm_recipients_file() const {
		return _dkdm_recipients_file;
	}

	bool show_hints_before_make_dcp() const {
		return _show_hints_before_make_dcp;
	}

	bool confirm_kdm_email() const {
		return _confirm_kdm_email;
	}

	dcp::NameFormat kdm_container_name_format() const {
		return _kdm_container_name_format;
	}

	dcp::NameFormat kdm_filename_format() const {
		return _kdm_filename_format;
	}

	dcp::NameFormat dkdm_filename_format() const {
		return _dkdm_filename_format;
	}

	dcp::NameFormat dcp_metadata_filename_format() const {
		return _dcp_metadata_filename_format;
	}

	dcp::NameFormat dcp_asset_filename_format() const {
		return _dcp_asset_filename_format;
	}

	bool jump_to_selected() const {
		return _jump_to_selected;
	}

	/* This could be an enum class but we use the enum a lot to index _nagged
	 * so it means a lot of casts.
	 */
	enum Nag {
		NAG_DKDM_CONFIG,
		NAG_ENCRYPTED_METADATA,
		NAG_ALTER_DECRYPTION_CHAIN,
		NAG_BAD_SIGNER_CHAIN_UTF8,
		NAG_IMPORT_DECRYPTION_CHAIN,
		NAG_DELETE_DKDM,
		NAG_32_ON_64,
		NAG_TOO_MANY_DROPPED_FRAMES,
		NAG_BAD_SIGNER_CHAIN_VALIDITY,
		NAG_BAD_SIGNER_DN_QUALIFIER,
		NAG_COUNT
	};

	bool nagged(Nag nag) const {
		return _nagged[nag];
	}

	bool sound() const {
		return _sound;
	}

	std::string cover_sheet() const {
		return _cover_sheet;
	}

	boost::optional<std::string> sound_output() const {
		return _sound_output;
	}

	boost::optional<boost::filesystem::path> last_player_load_directory() const {
		return _last_player_load_directory;
	}

	enum KDMWriteType {
		KDM_WRITE_FLAT,
		KDM_WRITE_FOLDER,
		KDM_WRITE_ZIP
	};

	boost::optional<KDMWriteType> last_kdm_write_type() const {
		return _last_kdm_write_type;
	}

	enum DKDMWriteType {
		DKDM_WRITE_INTERNAL,
		DKDM_WRITE_FILE
	};

	boost::optional<DKDMWriteType> last_dkdm_write_type() const {
		return _last_dkdm_write_type;
	}

	int frames_in_memory_multiplier() const {
		return _frames_in_memory_multiplier;
	}

	boost::optional<int> decode_reduction() const {
		return _decode_reduction;
	}

	bool default_notify() const {
		return _default_notify;
	}

	enum Notification {
		MESSAGE_BOX,
		EMAIL,
		NOTIFICATION_COUNT
	};

	bool notification(Notification n) const {
		return _notification[n];
	}

	boost::optional<std::string> barco_username() const {
		return _barco_username;
	}

	boost::optional<std::string> barco_password() const {
		return _barco_password;
	}

	boost::optional<std::string> christie_username() const {
		return _christie_username;
	}

	boost::optional<std::string> christie_password() const {
		return _christie_password;
	}

	boost::optional<std::string> gdc_username() const {
		return _gdc_username;
	}

	boost::optional<std::string> gdc_password() const {
		return _gdc_password;
	}

	enum class PlayerMode {
		WINDOW, ///< one window containing image and controls
		FULL,   ///< just the image filling the screen
		DUAL    ///< image on one monitor and extended controls on the other
	};

	PlayerMode player_mode() const {
		return _player_mode;
	}

	bool player_restricted_menus() const {
		return _player_restricted_menus;
	}

	bool playlist_editor_restricted_menus() const {
		return _playlist_editor_restricted_menus;
	}

	boost::optional<float> player_crop_output_ratio() const {
		return _player_crop_output_ratio;
	}

	int image_display() const {
		return _image_display;
	}

	enum VideoViewType {
		VIDEO_VIEW_SIMPLE,
		VIDEO_VIEW_OPENGL
	};

	VideoViewType video_view_type() const {
		return _video_view_type;
	}

	bool respect_kdm_validity_periods() const {
		return _respect_kdm_validity_periods;
	}

	boost::optional<boost::filesystem::path> player_debug_log_file() const {
		return _player_debug_log_file;
	}

	boost::optional<boost::filesystem::path> kdm_debug_log_file() const {
		return _kdm_debug_log_file;
	}

	boost::optional<boost::filesystem::path> player_content_directory() const {
		return _player_content_directory;
	}

	boost::optional<boost::filesystem::path> player_playlist_directory() const {
		return _player_playlist_directory;
	}

	boost::optional<boost::filesystem::path> player_kdm_directory() const {
		return _player_kdm_directory;
	}

	AudioMapping audio_mapping(int output_channels);

	std::vector<dcp::LanguageTag> custom_languages() const {
		return _custom_languages;
	}

	boost::optional<boost::filesystem::path> initial_path(std::string id) const;

	bool use_isdcf_name_by_default() const {
		return _use_isdcf_name_by_default;
	}

	bool write_kdms_to_disk() const {
		return _write_kdms_to_disk;
	}

	bool email_kdms() const {
		return _email_kdms;
	}

	dcp::Formulation default_kdm_type() const {
		return _default_kdm_type;
	}

	RoughDuration default_kdm_duration() const {
		return _default_kdm_duration;
	}

	double auto_crop_threshold() const {
		return _auto_crop_threshold;
	}

	boost::optional<std::string> last_release_notes_version() const {
		return _last_release_notes_version;
	}

	boost::optional<int> main_divider_sash_position() const {
		return _main_divider_sash_position;
	}

	boost::optional<int> main_content_divider_sash_position() const {
		return _main_content_divider_sash_position;
	}

	enum class DefaultAddFileLocation {
		SAME_AS_LAST_TIME,
		SAME_AS_PROJECT
	};

	DefaultAddFileLocation default_add_file_location() const {
		return _default_add_file_location;
	}

	bool allow_smpte_bv20() const {
		return _allow_smpte_bv20;
	}

#ifdef DCPOMATIC_GROK
	class Grok
	{
	public:
		Grok();
		Grok(cxml::ConstNodePtr node);

		void as_xml(xmlpp::Element* node) const;

		bool enable = false;
		boost::filesystem::path binary_location;
		int selected = 0;
		std::string licence_server;
		std::string licence;
	};

	Grok grok() const {
		return _grok;
	}
#endif

	int isdcf_name_part_length() const {
		return _isdcf_name_part_length;
	}

	bool enable_player_http_server() const {
		return _enable_player_http_server;
	}

	int player_http_server_port() const {
		return _player_http_server_port;
	}

	bool relative_paths() const {
		return _relative_paths;
	}

	bool layout_for_short_screen() {
		return _layout_for_short_screen;
	}

	/* SET (mostly) */

	void set_master_encoding_threads(int n) {
		maybe_set(_master_encoding_threads, n);
	}

	void set_server_encoding_threads(int n) {
		maybe_set(_server_encoding_threads, n);
	}

	void set_default_directory(boost::filesystem::path d) {
		if (_default_directory && *_default_directory == d) {
			return;
		}
		_default_directory = d;
		changed();
	}

	/** @param p New server port */
	void set_server_port_base(int p) {
		maybe_set(_server_port_base, p);
	}

	void set_only_servers_encode(bool o) {
		maybe_set(_only_servers_encode, o);
	}

	void set_tms_protocol(FileTransferProtocol p) {
		maybe_set(_tms_protocol, p);
	}

	void set_tms_passive(bool passive) {
		maybe_set(_tms_passive, passive);
	}

	/** @param i IP address of a TMS that we can copy DCPs to */
	void set_tms_ip(std::string i) {
		maybe_set(_tms_ip, i);
	}

	/** @param p Path on a TMS that we should changed DCPs to */
	void set_tms_path(std::string p) {
		maybe_set(_tms_path, p);
	}

	/** @param u User name to log into the TMS with */
	void set_tms_user(std::string u) {
		maybe_set(_tms_user, u);
	}

	/** @param p Password to log into the TMS with */
	void set_tms_password(std::string p) {
		maybe_set(_tms_password, p);
	}

	void set_allowed_dcp_frame_rates(std::list<int> const & r) {
		maybe_set(_allowed_dcp_frame_rates, r);
	}

	void set_allow_any_dcp_frame_rate(bool a) {
		maybe_set(_allow_any_dcp_frame_rate, a);
	}

	void set_allow_any_container(bool a) {
		maybe_set(_allow_any_container, a, ALLOW_ANY_CONTAINER);
	}

	void set_allow_96hhz_audio(bool a) {
		maybe_set(_allow_96khz_audio, a);
	}

	void set_use_all_audio_channels(bool a) {
		maybe_set(_use_all_audio_channels, a);
	}

	void set_show_experimental_audio_processors(bool e) {
		maybe_set(_show_experimental_audio_processors, e, SHOW_EXPERIMENTAL_AUDIO_PROCESSORS);
	}

	void set_language(std::string l) {
		if (_language && _language.get() == l) {
			return;
		}
		_language = l;
		changed();
	}

	void unset_language() {
		if (!_language) {
			return;
		}

		_language = boost::none;
		changed();
	}

	void set_default_still_length(int s) {
		maybe_set(_default_still_length, s);
	}

	void set_dcp_issuer(std::string i) {
		maybe_set(_dcp_issuer, i);
	}

	void set_dcp_creator(std::string c) {
		maybe_set(_dcp_creator, c);
	}

	void set_dcp_company_name(std::string c) {
		maybe_set(_dcp_company_name, c);
	}

	void set_dcp_product_name(std::string c) {
		maybe_set(_dcp_product_name, c);
	}

	void set_dcp_product_version(std::string c) {
		maybe_set(_dcp_product_version, c);
	}

	void set_dcp_j2k_comment(std::string c) {
		maybe_set(_dcp_j2k_comment, c);
	}

	void set_default_audio_delay(int d) {
		maybe_set(_default_audio_delay, d);
	}

	void set_default_audio_language(dcp::LanguageTag tag) {
		maybe_set(_default_audio_language, tag);
	}

	void unset_default_audio_language() {
		maybe_set(_default_audio_language, boost::optional<dcp::LanguageTag>());
	}

	void set_upload_after_make_dcp(bool u) {
		maybe_set(_upload_after_make_dcp, u);
	}

	void set_mail_server(std::string s) {
		maybe_set(_mail_server, s);
	}

	void set_mail_port(int p) {
		maybe_set(_mail_port, p);
	}

	void set_mail_protocol(EmailProtocol p) {
		maybe_set(_mail_protocol, p);
	}

	void set_mail_user(std::string u) {
		maybe_set(_mail_user, u);
	}

	void set_mail_password(std::string p) {
		maybe_set(_mail_password, p);
	}

	void set_kdm_subject(std::string s) {
		maybe_set(_kdm_subject, s);
	}

	void set_kdm_from(std::string f) {
		maybe_set(_kdm_from, f);
	}

	void set_kdm_cc(std::vector<std::string> f) {
		maybe_set(_kdm_cc, f);
	}

	void set_kdm_bcc(std::string f) {
		maybe_set(_kdm_bcc, f);
	}

	void set_kdm_email(std::string e) {
		maybe_set(_kdm_email, e);
	}

	void reset_kdm_email();

	void set_notification_subject(std::string s) {
		maybe_set(_notification_subject, s);
	}

	void set_notification_from(std::string f) {
		maybe_set(_notification_from, f);
	}

	void set_notification_to(std::string t) {
		maybe_set(_notification_to, t);
	}

	void set_notification_cc(std::vector<std::string> f) {
		maybe_set(_notification_cc, f);
	}

	void set_notification_bcc(std::string f) {
		maybe_set(_notification_bcc, f);
	}

	void set_notification_email(std::string e) {
		maybe_set(_notification_email, e);
	}

	void reset_notification_email();

	void set_signer_chain(std::shared_ptr<const dcp::CertificateChain> s) {
		maybe_set(_signer_chain, s);
	}

	void set_decryption_chain(std::shared_ptr<const dcp::CertificateChain> c) {
		maybe_set(_decryption_chain, c);
	}

	void set_check_for_updates(bool c) {
		maybe_set(_check_for_updates, c);
		if (!c) {
			set_check_for_test_updates(false);
		}
	}

	void set_check_for_test_updates(bool c) {
		maybe_set(_check_for_test_updates, c);
	}

	void set_maximum_video_bit_rate(VideoEncoding encoding, int64_t b) {
		maybe_set(_maximum_video_bit_rate[encoding], b);
	}

	void set_log_types(int t) {
		maybe_set(_log_types, t);
	}

	void set_analyse_ebur128(bool a) {
		maybe_set(_analyse_ebur128, a);
	}

	void set_automatic_audio_analysis(bool a) {
		maybe_set(_automatic_audio_analysis, a);
	}

#ifdef DCPOMATIC_WINDOWS
	void set_win32_console(bool c) {
		maybe_set(_win32_console, c);
	}
#endif

	void set_dkdms(std::shared_ptr<DKDMGroup> dkdms) {
		_dkdms = dkdms;
		changed();
	}

	void set_cinemas_file(boost::filesystem::path file);

	void set_dkdm_recipients_file(boost::filesystem::path file);

	void set_show_hints_before_make_dcp(bool s) {
		maybe_set(_show_hints_before_make_dcp, s);
	}

	void set_confirm_kdm_email(bool s) {
		maybe_set(_confirm_kdm_email, s);
	}

	void set_sound(bool s) {
		maybe_set(_sound, s, SOUND);
	}

	void set_sound_output(std::string o) {
		maybe_set(_sound_output, o, SOUND_OUTPUT);
	}

	void set_last_player_load_directory(boost::filesystem::path d) {
		maybe_set(_last_player_load_directory, d);
	}

	void set_last_kdm_write_type(KDMWriteType t) {
		maybe_set(_last_kdm_write_type, t);
	}

	void set_last_dkdm_write_type(DKDMWriteType t) {
		maybe_set(_last_dkdm_write_type, t);
	}

	void unset_sound_output() {
		if (!_sound_output) {
			return;
		}

		_sound_output = boost::none;
		changed(SOUND_OUTPUT);
	}

	void set_kdm_container_name_format(dcp::NameFormat n) {
		maybe_set(_kdm_container_name_format, n);
	}

	void set_kdm_filename_format(dcp::NameFormat n) {
		maybe_set(_kdm_filename_format, n);
	}

	void set_dkdm_filename_format(dcp::NameFormat n) {
		maybe_set(_dkdm_filename_format, n);
	}

	void set_dcp_metadata_filename_format(dcp::NameFormat n) {
		maybe_set(_dcp_metadata_filename_format, n);
	}

	void set_dcp_asset_filename_format(dcp::NameFormat n) {
		maybe_set(_dcp_asset_filename_format, n);
	}

	void set_frames_in_memory_multiplier(int m) {
		maybe_set(_frames_in_memory_multiplier, m);
	}

	void set_decode_reduction(boost::optional<int> r) {
		maybe_set(_decode_reduction, r);
	}

	void set_default_notify(bool n) {
		maybe_set(_default_notify, n);
	}

	void clear_history() {
		_history.clear();
		changed();
	}

	void clear_player_history() {
		_player_history.clear();
		changed();
	}

	void add_to_history(boost::filesystem::path p);
	void clean_history();
	void add_to_player_history(boost::filesystem::path p);
	void clean_player_history();

	void set_jump_to_selected(bool j) {
		maybe_set(_jump_to_selected, j);
	}

	void set_nagged(Nag nag, bool nagged) {
		maybe_set(_nagged[nag], nagged);
	}

	void set_cover_sheet(std::string s) {
		maybe_set(_cover_sheet, s);
	}

	void reset_cover_sheet();

	void set_notification(Notification n, bool v) {
		maybe_set(_notification[n], v);
	}

	void set_barco_username(std::string u) {
		maybe_set(_barco_username, u);
	}

	void unset_barco_username() {
		maybe_set(_barco_username, boost::optional<std::string>());
	}

	void set_barco_password(std::string p) {
		maybe_set(_barco_password, p);
	}

	void unset_barco_password() {
		maybe_set(_barco_password, boost::optional<std::string>());
	}

	void set_christie_username(std::string u) {
		maybe_set(_christie_username, u);
	}

	void unset_christie_username() {
		maybe_set(_christie_username, boost::optional<std::string>());
	}

	void set_christie_password(std::string p) {
		maybe_set(_christie_password, p);
	}

	void unset_christie_password() {
		maybe_set(_christie_password, boost::optional<std::string>());
	}

	void set_gdc_username(std::string u) {
		maybe_set(_gdc_username, u);
	}

	void unset_gdc_username() {
		maybe_set(_gdc_username, boost::optional<std::string>());
	}

	void set_gdc_password(std::string p) {
		maybe_set(_gdc_password, p);
	}

	void unset_gdc_password() {
		maybe_set(_gdc_password, boost::optional<std::string>());
	}

	void set_player_mode(PlayerMode m) {
		maybe_set(_player_mode, m);
	}

	void set_player_crop_output_ratio(float ratio) {
		maybe_set(_player_crop_output_ratio, ratio);
	}

	void unset_player_crop_output_ratio() {
		if (!_player_crop_output_ratio) {
			return;
		}

		_player_crop_output_ratio = boost::none;
		changed(OTHER);
	}

	void set_image_display(int n) {
		maybe_set(_image_display, n);
	}

	void set_video_view_type(VideoViewType v) {
		maybe_set(_video_view_type, v);
	}

	void set_respect_kdm_validity_periods(bool r) {
		maybe_set(_respect_kdm_validity_periods, r);
	}

	void set_player_debug_log_file(boost::filesystem::path p) {
		maybe_set(_player_debug_log_file, p, PLAYER_DEBUG_LOG);
	}

	void unset_player_debug_log_file() {
		if (!_player_debug_log_file) {
			return;
		}
		_player_debug_log_file = boost::none;
		changed(PLAYER_DEBUG_LOG);
	}

	void set_kdm_debug_log_file(boost::filesystem::path p) {
		maybe_set(_kdm_debug_log_file, p, KDM_DEBUG_LOG);
	}

	void set_player_content_directory(boost::filesystem::path p) {
		maybe_set(_player_content_directory, p, PLAYER_CONTENT_DIRECTORY);
	}

	void unset_player_content_directory() {
		if (!_player_content_directory) {
			return;
		}
		_player_content_directory = boost::none;
		changed(PLAYER_CONTENT_DIRECTORY);
	}

	void set_player_playlist_directory(boost::filesystem::path p) {
		maybe_set(_player_playlist_directory, p, PLAYER_PLAYLIST_DIRECTORY);
	}

	void unset_player_playlist_directory() {
		if (!_player_playlist_directory) {
			return;
		}
		_player_playlist_directory = boost::none;
		changed(PLAYER_PLAYLIST_DIRECTORY);
	}

	void set_player_kdm_directory(boost::filesystem::path p) {
		maybe_set(_player_kdm_directory, p);
	}

	void unset_player_kdm_directory() {
		if (!_player_kdm_directory) {
			return;
		}
		_player_kdm_directory = boost::none;
		changed();
	}

	void set_audio_mapping(AudioMapping m);
	void set_audio_mapping_to_default();

	void add_custom_language(dcp::LanguageTag tag);

	void set_initial_path(std::string id, boost::filesystem::path path);

	void set_use_isdcf_name_by_default(bool use) {
		maybe_set(_use_isdcf_name_by_default, use);
	}

	void set_write_kdms_to_disk(bool write) {
		maybe_set(_write_kdms_to_disk, write);
	}

	void set_email_kdms(bool email) {
		maybe_set(_email_kdms, email);
	}

	void set_default_kdm_type(dcp::Formulation type) {
		maybe_set(_default_kdm_type, type);
	}

	void set_default_kdm_duration(RoughDuration duration) {
		maybe_set(_default_kdm_duration, duration);
	}

	void set_auto_crop_threshold(double threshold) {
		maybe_set(_auto_crop_threshold, threshold, AUTO_CROP_THRESHOLD);
	}

	void set_last_release_notes_version(std::string version) {
		maybe_set(_last_release_notes_version, version);
	}

	void unset_last_release_notes_version() {
		maybe_set(_last_release_notes_version, boost::optional<std::string>());
	}

	ExportConfig& export_config() {
		return _export;
	}

	void set_main_divider_sash_position(int position) {
		maybe_set(_main_divider_sash_position, position);
	}

	void set_main_content_divider_sash_position(int position) {
		maybe_set(_main_content_divider_sash_position, position);
	}

	void set_default_add_file_location(DefaultAddFileLocation location) {
		maybe_set(_default_add_file_location, location);
	}

	void set_allow_smpte_bv20(bool allow) {
		maybe_set(_allow_smpte_bv20, allow, ALLOW_SMPTE_BV20);
	}

#ifdef DCPOMATIC_GROK
	void set_grok(Grok const& grok);
#endif

	void set_isdcf_name_part_length(int length) {
		maybe_set(_isdcf_name_part_length, length, ISDCF_NAME_PART_LENGTH);
	}

	void set_enable_player_http_server(bool enable) {
		maybe_set(_enable_player_http_server, enable);
	}

	void set_player_http_server_port(int port) {
		maybe_set(_player_http_server_port, port);
	}

	void set_relative_paths(bool relative) {
		maybe_set(_relative_paths, relative);
	}

	void set_layout_for_short_screen(bool layout) {
		maybe_set(_layout_for_short_screen, layout);
	}


	void changed(Property p = OTHER);
	boost::signals2::signal<void (Property)> Changed;
	/** Emitted if read() failed on an existing Config file.  There is nothing
	    a listener can do about it: this is just for information.
	*/
	enum class LoadFailure {
		CONFIG,
	};
	static boost::signals2::signal<void (LoadFailure)> FailedToLoad;
	/** Emitted if read() issued a warning which the user might want to know about */
	static boost::signals2::signal<void (std::string)> Warning;
	/** Emitted if there is a something wrong the contents of our config.  Handler can call
	 *  true to ask Config to solve the problem (by discarding and recreating the bad thing)
	 */
	enum BadReason {
		BAD_SIGNER_UTF8_STRINGS,      ///< signer chain contains UTF-8 strings (not PRINTABLESTRING)
		BAD_SIGNER_INCONSISTENT,      ///< signer chain is somehow inconsistent
		BAD_DECRYPTION_INCONSISTENT,  ///< KDM decryption chain is somehow inconsistent
		BAD_SIGNER_VALIDITY_TOO_LONG, ///< signer certificate validity periods are >10 years
		BAD_SIGNER_DN_QUALIFIER,      ///< some signer certificate has a bad dnQualifier (DoM #2716).
	};

	static boost::signals2::signal<bool (BadReason)> Bad;

	void write() const override;
	void write_config() const;
	void link(boost::filesystem::path new_file) const;
	void copy_and_link(boost::filesystem::path new_file) const;
	bool have_write_permission() const;

	void save_default_template(std::shared_ptr<const Film> film) const;
	void save_template(std::shared_ptr<const Film> film, std::string name) const;
	bool existing_template(std::string name) const;
	/** @return Template names(not including the default) */
	std::vector<std::string> templates() const;
	boost::filesystem::path template_read_path(std::string name) const;
	boost::filesystem::path default_template_read_path() const;
	void rename_template(std::string old_name, std::string new_name) const;
	void delete_template(std::string name) const;

	boost::optional<BadReason> check_certificates() const;

	static Config* instance();
	static void drop();
	static void restore_defaults();
	static bool have_existing(std::string);
	static boost::filesystem::path config_read_file();
	static boost::filesystem::path config_write_file();
	static bool zip_contains_cinemas(boost::filesystem::path zip);
	static boost::filesystem::path cinemas_file_from_zip(boost::filesystem::path zip);


	template <class T>
	void maybe_set(T& member, T new_value, Property prop = OTHER) {
		if (member == new_value) {
			return;
		}
		member = new_value;
		changed(prop);
	}

	template <class T>
	void maybe_set(boost::optional<T>& member, T new_value, Property prop = OTHER) {
		if (member && member.get() == new_value) {
			return;
		}
		member = new_value;
		changed(prop);
	}

private:
	Config();
	void read() override;
	void set_defaults();
	void set_kdm_email_to_default();
	void set_notification_email_to_default();
	void set_cover_sheet_to_default();
	std::shared_ptr<dcp::CertificateChain> create_certificate_chain();
	boost::filesystem::path directory_or(boost::optional<boost::filesystem::path> dir, boost::filesystem::path a) const;
	void add_to_history_internal(std::vector<boost::filesystem::path>& h, boost::filesystem::path p);
	void clean_history_internal(std::vector<boost::filesystem::path>& h);
	void backup();
	boost::filesystem::path template_write_path(std::string name) const;

	/** number of threads which a master DoM should use for J2K encoding on the local machine */
	int _master_encoding_threads;
	/** number of threads which a server should use for J2K encoding on the local machine */
	int _server_encoding_threads;
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
	FileTransferProtocol _tms_protocol;
	bool _tms_passive;
	/** The IP address of a TMS that we can copy DCPs to */
	std::string _tms_ip;
	/** The path on a TMS that we should write DCPs to */
	std::string _tms_path;
	/** User name to log into the TMS with */
	std::string _tms_user;
	/** Password to log into the TMS with */
	std::string _tms_password;
	/** The list of possible DCP frame rates that DCP-o-matic will use */
	std::list<int> _allowed_dcp_frame_rates;
	/** Allow any video frame rate for the DCP; if true, overrides _allowed_dcp_frame_rates */
	bool _allow_any_dcp_frame_rate;
	/** Allow any container ratio, not just the standard ones.  GDC SX-2001 will not play Flat
	    DCPs at 25fps but will play 16:9, so this is very useful for some users.
	    https://www.dcpomatic.com/forum/viewtopic.php?f=2&t=1119&p=4468
	*/
	bool _allow_any_container;
	bool _allow_96khz_audio;
	bool _use_all_audio_channels;
	/** Offer the upmixers in the audio processor settings */
	bool _show_experimental_audio_processors;
	boost::optional<std::string> _language;
 	/** Default length of still image content (seconds) */
	int _default_still_length;
	DCPContentType const * _default_dcp_content_type;
	int _default_dcp_audio_channels;
	std::string _dcp_issuer;
	std::string _dcp_creator;
	std::string _dcp_company_name;
	std::string _dcp_product_name;
	std::string _dcp_product_version;
	std::string _dcp_j2k_comment;
	EnumIndexedVector<int64_t, VideoEncoding> _default_video_bit_rate;
	int _default_audio_delay;
	bool _default_interop;
	boost::optional<dcp::LanguageTag> _default_audio_language;
	boost::optional<dcp::LanguageTag::RegionSubtag> _default_territory;
	std::map<std::string, std::string> _default_metadata;
	/** Default directory to offer to write KDMs to; if it's not set,
	    the home directory will be offered.
	*/
	boost::optional<boost::filesystem::path> _default_kdm_directory;
	bool _upload_after_make_dcp;
	std::string _mail_server;
	int _mail_port;
	EmailProtocol _mail_protocol;
	std::string _mail_user;
	std::string _mail_password;
	std::string _kdm_subject;
	std::string _kdm_from;
	std::vector<std::string> _kdm_cc;
	std::string _kdm_bcc;
	std::string _kdm_email;
	std::string _notification_subject;
	std::string _notification_from;
	std::string _notification_to;
	std::vector<std::string> _notification_cc;
	std::string _notification_bcc;
	std::string _notification_email;
	std::shared_ptr<const dcp::CertificateChain> _signer_chain;
	/** Chain used to decrypt KDMs; the leaf of this chain is the target
	 *  certificate for making KDMs given to DCP-o-matic.
	 */
	std::shared_ptr<const dcp::CertificateChain> _decryption_chain;
	/** true to check for updates on startup */
	bool _check_for_updates;
	bool _check_for_test_updates;
	/** maximum allowed video bit rate in bits per second */
	EnumIndexedVector<int64_t, VideoEncoding> _maximum_video_bit_rate;
	int _log_types;
	bool _analyse_ebur128;
	bool _automatic_audio_analysis;
#ifdef DCPOMATIC_WINDOWS
	bool _win32_console;
#endif
	std::vector<boost::filesystem::path> _history;
	std::vector<boost::filesystem::path> _player_history;
	std::shared_ptr<DKDMGroup> _dkdms;
	boost::filesystem::path _cinemas_file;
	boost::filesystem::path _dkdm_recipients_file;
	bool _show_hints_before_make_dcp;
	bool _confirm_kdm_email;
	dcp::NameFormat _kdm_filename_format;
	dcp::NameFormat _dkdm_filename_format;
	dcp::NameFormat _kdm_container_name_format;
	dcp::NameFormat _dcp_metadata_filename_format;
	dcp::NameFormat _dcp_asset_filename_format;
	bool _jump_to_selected;
	bool _nagged[NAG_COUNT];
	bool _sound;
	/** name of a specific sound output stream to use, or empty to use the default */
	boost::optional<std::string> _sound_output;
	std::string _cover_sheet;
	boost::optional<boost::filesystem::path> _last_player_load_directory;
	boost::optional<KDMWriteType> _last_kdm_write_type;
	boost::optional<DKDMWriteType> _last_dkdm_write_type;
	int _frames_in_memory_multiplier;
	boost::optional<int> _decode_reduction;
	bool _default_notify;
	bool _notification[NOTIFICATION_COUNT];
	boost::optional<std::string> _barco_username;
	boost::optional<std::string> _barco_password;
	boost::optional<std::string> _christie_username;
	boost::optional<std::string> _christie_password;
	boost::optional<std::string> _gdc_username;
	boost::optional<std::string> _gdc_password;
	PlayerMode _player_mode;
	bool _player_restricted_menus = false;
	bool _playlist_editor_restricted_menus = false;
	boost::optional<float> _player_crop_output_ratio;
	int _image_display;
	VideoViewType _video_view_type;
	bool _respect_kdm_validity_periods;
	/** Log file containing debug information for the player */
	boost::optional<boost::filesystem::path> _player_debug_log_file;
	boost::optional<boost::filesystem::path> _kdm_debug_log_file;
	/** A directory containing DCPs whose contents are presented to the user
	    in the dual-screen player mode.  DCPs on the list can be loaded
	    for playback.
	*/
	boost::optional<boost::filesystem::path> _player_content_directory;
	boost::optional<boost::filesystem::path> _player_playlist_directory;
	boost::optional<boost::filesystem::path> _player_kdm_directory;
	boost::optional<AudioMapping> _audio_mapping;
	std::vector<dcp::LanguageTag> _custom_languages;
	std::map<std::string, boost::optional<boost::filesystem::path>> _initial_paths;
	bool _use_isdcf_name_by_default;
	bool _write_kdms_to_disk;
	bool _email_kdms;
	dcp::Formulation _default_kdm_type;
	RoughDuration _default_kdm_duration;
	double _auto_crop_threshold;
	boost::optional<std::string> _last_release_notes_version;
	boost::optional<int> _main_divider_sash_position;
	boost::optional<int> _main_content_divider_sash_position;
	DefaultAddFileLocation _default_add_file_location;
	bool _allow_smpte_bv20;
	int _isdcf_name_part_length;
	bool _enable_player_http_server;
	int _player_http_server_port;
	bool _relative_paths;
	bool _layout_for_short_screen;

#ifdef DCPOMATIC_GROK
	Grok _grok;
#endif

	ExportConfig _export;

	static int const _current_version;

	/** Singleton instance, or 0 */
	static Config* _instance;
};


#endif
