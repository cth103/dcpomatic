# DCP-o-matic / libdcp: Dolby Atmos, channel 14 sync signal, and player channel routing

Local repo: /Users/jap/projects/dcpomatic (branch main, HEAD 92a954bed). Paths below are relative to that repo. libdcp is NOT present locally (only asdcplib headers `AS_DCP.h` in /usr/local/include or /opt/homebrew/include); libdcp references come from the upstream mirror https://github.com/cth103/libdcp (branch main, fetched 2026-09-26), so they may differ slightly from the exact libdcp version pinned by this DCP-o-matic checkout.

## 1. What Atmos-related classes/files exist in src/lib and src/wx, and what do they do?

### Takeaway
Atmos support is a pure pass-through pipeline: `AtmosMXFContent` (standalone Atmos MXF) or `DCPContent` (a DCP with an Atmos reel asset) is read frame-by-frame as opaque `dcp::AtmosFrame` blobs, forwarded by `Player::Atmos`, and rewritten by `ReelWriter` into a new `dcp::AtmosAsset`. src/wx only has timeline/UI glue; no class decodes or renders Atmos audio.

### Cited Findings
- src/lib Atmos files: `atmos_content.{h,cc}`, `atmos_decoder.{h,cc}`, `atmos_metadata.{h,cc}`, `atmos_mxf_content.{h,cc}`, `atmos_mxf_decoder.{h,cc}`, `content_atmos.h`; all listed in `src/lib/wscript:28-32` — [local repo](file:///Users/jap/projects/dcpomatic/src/lib)
- `AtmosContent` (src/lib/atmos_content.h:37-62) is a `ContentPart` holding only `_length` (Frame) and `_edit_rate` (dcp::Fraction); property `AtmosContentProperty::EDIT_RATE = 700` (atmos_content.h:33). Every `Content` has `std::shared_ptr<AtmosContent> atmos` (src/lib/content.h:230) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/atmos_content.h)
- `AtmosMXFContent::valid_mxf()` (src/lib/atmos_mxf_content.cc:56-73) detects an Atmos MXF by trying to construct `dcp::AtmosAsset(path)`; content_factory uses it for any `.mxf` (src/lib/content_factory.cc:189-190); XML type string "AtmosMXF" (content_factory.cc:98-99, atmos_mxf_content.cc:102). `examine()` sets length = `intrinsic_duration()` and edit rate from the asset (atmos_mxf_content.cc:76-89). Summary shows "{} [Atmos]" (atmos_mxf_content.cc:95) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/atmos_mxf_content.cc)
- `AtmosMXFDecoder` (src/lib/atmos_mxf_decoder.cc:35-69) opens `dcp::AtmosAsset`, `start_read()`, `set_check_hmac(false)`, and in `pass()` emits `_reader->get_frame(frame)` one video frame at a time (lines 48-61) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/atmos_mxf_decoder.cc)
- `AtmosDecoder::emit()` (src/lib/atmos_decoder.cc:48-54) just raises `Data(ContentAtmos(data, frame, metadata))`; comment: "There's no fiddling with frame rates when we are using Atmos; the DCP rate must be the same as the Atmos one" — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/atmos_decoder.cc)
- `ContentAtmos` (src/lib/content_atmos.h:34-45) bundles `shared_ptr<const dcp::AtmosFrame> data`, `Frame frame`, `AtmosMetadata metadata` — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/content_atmos.h)
- `AtmosMetadata` (src/lib/atmos_metadata.h:26-37, .cc:30-44) copies `first_frame`, `max_channel_count`, `max_object_count`, `atmos_version` from the source asset and `create(edit_rate)` builds a new `dcp::AtmosAsset` with those values — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/atmos_metadata.cc)
- DCP input: `DCPExaminer` sets `_has_atmos`, `_atmos_length`, `_atmos_edit_rate` from `reel->atmos()` (src/lib/dcp_examiner.cc:288-294); `DCPContent` creates `AtmosContent` from that (src/lib/dcp_content.cc:295-304); `DCPDecoder` creates an `AtmosDecoder` (dcp_decoder.cc:93-94), opens the reader per reel (dcp_decoder.cc:446-450) and emits frames honoring the reel entry point (dcp_decoder.cc:268-271) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/dcp_decoder.cc)
- Film-level rules: `Film::add_content` forces `audio_channels >= 14` and `set_interop(false)` when any Atmos content is added (src/lib/film.cc:1558-1583); Atmos content forces film frame rate = Atmos edit rate and rejects mixed Atmos rates (film.cc:1787-1803); reel boundaries are forced at Atmos content boundaries (`check_reel_boundaries_for_atmos`, film.cc:1738-1777); ISDCF name gets "-IAB" (film.cc:1127-1129); `contains_atmos_content()` at film.cc:2313-2316 — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/film.cc)
- src/wx: `ContentTimelineAtmosView` (src/wx/content_timeline_atmos_view.{h,cc}; colour `ATMOS_CONTENT_COLOUR` src/wx/colours.h:25), "Atmos" track label (src/wx/timeline_labels_view.cc:44,84-86), created in content_timeline.cc:320-321, drawn in dcp_timeline.cc:497,531. `DCPPanel`: minimum audio channels = 14 when Atmos present (src/wx/dcp_panel.cc:950-958), frame-rate controls disabled (dcp_panel.cc:718-719), Interop/MPEG2 standards not offered with Atmos (dcp_panel.cc:161-180) — [local repo](file:///Users/jap/projects/dcpomatic/src/wx)

### Inferences
- There is no Atmos-specific audio class in src/wx (no Atmos meter, no object view, no renderer).

### Gaps
- Could not check the pinned libdcp version (cscript) against upstream main.

## 2. How is Atmos passed through to the output DCP?

### Takeaway
Byte-for-byte frame copy: `Player::atmos` -> `Player::Atmos` signal -> `DCPFilmEncoder::atmos` -> `Writer::write(AtmosFrame)` -> `ReelWriter::write` -> `dcp::AtmosAssetWriter::write`. The only connection to `Player::Atmos` in the whole tree is the DCP encoder.

### Cited Findings
- Player connects decoder Atmos data at src/lib/player.cc:349-350; later overlapping Atmos content wins, earlier piece gets `ignore_atmos` periods (player.cc:382-388; field in src/lib/piece.h:48); `Player::atmos()` maps content frame to DCP time, discards out-of-range/ignored frames and emits `Atmos(data.data, dcp_time, data.metadata)` (player.cc:1636-1664); signal declared at src/lib/player.h:132 — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/player.cc)
- `grep "Atmos.connect"` over src/ returns only src/lib/dcp_film_encoder.cc:69 (`_player.Atmos.connect(bind(&DCPFilmEncoder::atmos...))`) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/dcp_film_encoder.cc)
- `Writer::write(AtmosFrame, DCPTime, AtmosMetadata)` advances reels on boundary and assumes "a video frame's worth of data" (src/lib/writer.cc:310-320) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/writer.cc)
- `ReelWriter::write(AtmosFrame, AtmosMetadata)` lazily creates `_atmos_asset = metadata.create(Fraction(video_frame_rate,1))`, sets encryption key if needed, `start_write()` into film dir, then `_atmos_asset_writer->write(atmos)` (src/lib/reel_writer.cc:308-319); finalized and moved into the DCP (reel_writer.cc:392-406); added as `dcp::ReelAtmosAsset(_atmos_asset, 0)` (reel_writer.cc:751-752) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/reel_writer.cc)
- Forum (Carl, 2021-11-30): "DoM has supported packaging of existing Atmos MXFs for a while" — [DCP-o-matic forum t=1766](https://dcpomatic.com/forum/viewtopic.php?t=1766)
- Forum (Carl, 2025-07-31): a correctly formatted MXF "should be listed as "[ATMOS]" in the file list" — [DCP-o-matic forum t=2854](https://dcpomatic.com/forum/viewtopic.php?t=2854)
- Forum users (Jan 2026) report multi-reel Atmos (7 separate MXF reels) where only the first reel was incorporated correctly; one user suspects DCP-o-matic re-writes IAB duration/entry points (user hypothesis, not confirmed by developer) — [DCP-o-matic forum t=2854](https://dcpomatic.com/forum/viewtopic.php?t=2854)

### Inferences
- Because frames are re-wrapped (new asset, new UUID, new metadata header from `AtmosMetadata::create`), this is re-packaging, not a file copy; per-frame payload is untouched.

### Gaps
- Did not verify the Jan 2026 multi-reel bug report against code.

## 3. Which 0-based index is the sync signal channel, and how is it generated/passed?

### Takeaway
Sync = 0-based index 13 (channel 14, 1-based). DCP-o-matic does not generate it itself: when the film contains Atmos, `ReelWriter` opens the sound asset with `AtmosSync::ENABLED` and libdcp's `SoundAssetWriter` overwrites sample channel 13 with an FSK-encoded sync bitstream, ignoring any audio passed on that channel.

### Cited Findings
- libdcp `enum class Channel`: LEFT=0 ... BSL=10, BSR=11, MOTION_DATA=12, SYNC_SIGNAL=13, SIGN_LANGUAGE=14, "15 is not used", CHANNEL_COUNT=16 (src/types.h:93-111) — [libdcp types.h](https://github.com/cth103/libdcp/blob/main/src/types.h)
- DCP-o-matic: `film()->contains_atmos_content() ? dcp::SoundAsset::AtmosSync::ENABLED : ...DISABLED` passed to `start_write` (src/lib/reel_writer.cc:239) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/reel_writer.cc)
- libdcp `SoundAssetWriter` constructor asserts `!_sync || channels() >= 14` and `!_sync || standard == SMPTE` (src/sound_asset_writer.cc:83-84); SYNC_SIGNAL is disallowed in `extra_active_channels` (lines 88-101) — [libdcp sound_asset_writer.cc](https://github.com/cth103/libdcp/blob/main/src/sound_asset_writer.cc)
- libdcp `do_write`: `if (j == 13 && _sync) { s = _fsk.get(); } else if (j < data_channels) {...}` (src/sound_asset_writer.h:142-146); member comment: "true to ignore any signal passed to write() on channel 14 and instead write a sync track" (sound_asset_writer.h:181-182) — [libdcp sound_asset_writer.h](https://github.com/cth103/libdcp/blob/main/src/sound_asset_writer.h)
- `create_sync_packets()` (sound_asset_writer.cc:296-374): assumes 48 kHz; per edit unit writes packets with 0x4d 0x56 header, CRC-16 (0x1021), edit-rate code (24->0, 25->1, 30->2, 48->3, 50->4, 60->5, 96->6, 100->7, 120->8), packet index 0-3, 4 bytes of the sound asset UUID per packet, 24-bit frame count, padding; 4 packets/frame at 24-30 fps, 2 at 48-60, 1 at 96-120; regenerated every frame (sound_asset_writer.cc ~271-274) — [libdcp sound_asset_writer.cc](https://github.com/cth103/libdcp/blob/main/src/sound_asset_writer.cc)
- `dcp::FSK` "Create frequency-shift-keyed samples for encoding synchronization signals", MSB first, 4 samples per bit (src/fsk.h) — [libdcp fsk.h](https://github.com/cth103/libdcp/blob/main/src/fsk.h)
- Mantis #1777 "Channel 14 sync-track not created": requirement "create the ATMOS channel 14 sync track as per ST-430-12"; "ATMOS DCPs need to be forced to 14-channel minimum and any existing signal overwritten"; fixed in 2.16.0, commit 3ec476bec2965284a011d35e9ee9a4c799372de7 (2020-07-11) — [DCP-o-matic Mantis 1777](https://www.dcpomatic.com/mantis/view.php?id=1777)
- DCP-o-matic treats SYNC_SIGNAL (and HI, VI, MOTION_DATA, SIGN_LANGUAGE) as pass-through for audio processors (src/lib/audio_processor.cc:151-161) and excludes it from the "x.y" channel count (src/lib/util.cc:797-806); LEQ(m) weights channel 13 at -144 dB, comment "// Sync" (src/lib/audio_analyser.cc:114) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib)

### Inferences
- Labeling inconsistency in DCP-o-matic UI vs libdcp: `short_audio_channel_name` gives index 12 "DBP", 13 "DBS", 14 "Sign" (src/lib/util.cc:594-620) and `audio_channel_name` gives 12 "D-BOX primary", 13 "D-BOX secondary", 14 "Unused" (util.cc:567-592), while libdcp names 13 SYNC_SIGNAL. `AudioProcessor::input_names()` labels "DBP" as 13 and "DBS" as 14 (audio_processor.cc:139-148), inconsistent with util.cc. So in the content mapping grid, output 13 is shown as "DBS", yet with Atmos present whatever is mapped there is replaced by FSK in the written MXF.
- For a non-Atmos film, index 13 is written with whatever audio is mapped to it (no sync is generated).

### Gaps
- Did not verify whether upstream libdcp at the version pinned by this checkout has identical `j == 13` logic (upstream main has it).

## 4. Can the player output only a chosen channel (e.g. channel 14) in real time? Where is audio routed to the sound card?

### Takeaway
There is no mute/solo or per-channel selector in the player UI. Routing is a single global `AudioMapping` (16 DCP channels x N device outputs) edited in Preferences > Sound; changing it rebuilds the Butler during playback, so channel 14 can be soloed only indirectly by editing that matrix (e.g. map input 13 -> outputs 0/1 and clear others). Audio goes Player -> Butler (`remap`) -> RtAudio callback.

### Cited Findings
- Signal path: `Player::audio` remaps content to `film->audio_channels()` using the content stream mapping (src/lib/player.cc:1242) and optional audio processor (player.cc:1247); emits `Audio(...)` (player.cc:1514) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/player.cc)
- `Butler` gets `Config::instance()->audio_mapping(_audio_channels)` and `_audio_channels` at creation (src/wx/film_viewer.cc:246-258) and applies `remap(audio, _audio_channels, _audio_mapping)` (src/lib/butler.cc:376); `remap()` is a gain matrix accumulate (src/lib/util.cc:810+) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/butler.cc)
- Sound card: RtAudio stream opened with `nChannels = device outputChannels`, `RTAUDIO_FLOAT32`, 48000 Hz (src/wx/film_viewer.cc:615-692); `FilmViewer::audio_callback` pulls from `_butler->get_audio(NON_BLOCKING, ...)` (film_viewer.cc:731-758) — [local repo](file:///Users/jap/projects/dcpomatic/src/wx/film_viewer.cc)
- Default output mapping `Config::audio_mapping(output_channels)`: for 2 outputs a Lt/Rt downmix of L,R,C,Lfe,Ls,Rs only; otherwise 1:1 for i < min(16, outputs) (src/lib/config.cc:1519-1545). So index 13 is silent on a stereo device and only reaches output 13 on devices with >=14 outputs — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/config.cc)
- The mapping is editable in `preferences::SoundPage` (AudioMappingView "DCP" x "Output", src/wx/sound_preferences_page.cc:97, 126-133, 210), present in both main and player preferences (src/wx/full_config_dialog.cc:1519, src/wx/player_config_dialog.cc:436) — [local repo](file:///Users/jap/projects/dcpomatic/src/wx/sound_preferences_page.cc)
- `FilmViewer::config_changed(Config::AUDIO_MAPPING)` calls `destroy_and_maybe_create_butler()` (film_viewer.cc:600-604), which destroys and recreates the Butler and re-seeks the player (film_viewer.cc:219-243) — [local repo](file:///Users/jap/projects/dcpomatic/src/wx/film_viewer.cc)
- The standalone player forces `_film->set_audio_channels(MAX_DCP_AUDIO_CHANNELS)` (16) (src/tools/dcpomatic_player.cc:501), so all 16 DCP channels (including index 13) reach the Butler — [local repo](file:///Users/jap/projects/dcpomatic/src/tools/dcpomatic_player.cc)
- DCP audio decoding reads all channels of the sound MXF into `AudioBuffers` (src/lib/dcp_decoder.cc:232-265); DCP content default mapping is 1:1 (src/lib/audio_mapping.cc:141-145, via dcp_content.cc:282-286) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/dcp_decoder.cc)
- No "mute"/"solo" code in src/wx, src/tools or src/lib (grep). No live level meters: `AudioDialog` (Tools > Audio graph, src/tools/dcpomatic_player.cc:1050-1058) plots precomputed `AudioAnalysis` with per-channel show/hide checkboxes (src/wx/audio_dialog.cc:116-119, 190-193); that is display-only, not playback routing — [local repo](file:///Users/jap/projects/dcpomatic/src/wx/audio_dialog.cc)

### Inferences
- Soloing index 13 today: Preferences > Sound, clear matrix, set DCP 13 -> Output 0 (and 1). Works while playing but with a butler rebuild + reseek (not glitch-free), and the setting is global/persistent in config.xml.
- Listening to the channel-14 FSK is only meaningful for DCP content that already has it (a DCP made with Atmos). In the editor preview (dcpomatic main), the FSK does not exist yet: it is synthesized only in libdcp at MXF write time.

### Gaps
- Not tested at runtime; the behaviour of the butler rebuild during playback (audible gap length) not measured.

## 5. Is Atmos essence ever decoded/rendered, or only passed through?

### Takeaway
Only passed through. No code decodes IAB/Atmos bitstream into PCM; FilmViewer/Butler never subscribe to `Player::Atmos`, so Atmos frames are dropped during preview/playback.

### Cited Findings
- Only subscriber to `Player::Atmos` is `DCPFilmEncoder` (src/lib/dcp_film_encoder.cc:69); no Atmos reference in src/lib/butler.cc or src/wx/film_viewer.cc (grep) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/dcp_film_encoder.cc)
- `dcp::AtmosFrame` is handled as an opaque buffer (decoder emits reader frames; writer writes them) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/reel_writer.cc)
- asdcplib exposes only an `AtmosDescriptor` (AtmosID, AtmosVersion) and frame read/write, no rendering (AS_DCP.h:1851-1955, installed header) — local install /usr/local/include/AS_DCP.h
- Forum: DCP-o-matic "has no specific features to generate or convert ATMOS files"; creating theatrical Atmos MXFs requires Dolby-certified tools — [DCP-o-matic forum search results t=2854/t=2243](https://dcpomatic.com/forum/viewtopic.php?t=2854)

### Inferences
- Rendering Atmos in the player would require an IAB/Dolby Atmos decoder+renderer; none is in the dependency tree. The only candidate open-source renderers would be for ADM/IAB (e.g. EBU EAR/BEAR for ADM), not for encrypted/proprietary theatrical Atmos bitstreams — this is inference, not verified.

### Gaps
- Did not verify whether theatrical Atmos MXF frames are SMPTE ST 2098-2 IAB (renderable by open IAB tools) versus proprietary Dolby format in all cases.

## 6. What would be required to add real-time selection of channel 14 or of Atmos audio in the player?

### Takeaway
Channel 14 solo is a small UI change reusing existing machinery (swap the Butler's `AudioMapping`); Atmos audio would require a whole new decode/render path.

### Cited Findings
- Existing hook: changing `Config::AUDIO_MAPPING` already triggers Butler rebuild (film_viewer.cc:600-604); Butler stores `_audio_mapping` once at construction (butler.cc:64,82) and applies it in `Butler::audio` (butler.cc:376) — [local repo](file:///Users/jap/projects/dcpomatic/src/lib/butler.cc)

### Inferences
- Minimal channel-14 solo: add a player control (e.g. menu/choice in src/tools/dcpomatic_player.cc or FilmViewer controls) that builds a temporary `AudioMapping(16, _audio_channels)` with only input 13 -> outputs, and pass it to the Butler. Cleanest glitch-free variant: add a mutex-protected `Butler::set_audio_mapping()` used in `Butler::audio()` (butler.cc:376) instead of recreating the Butler; note audio already queued in `_audio` (up to the butler buffer) was remapped with the old matrix, so the switch lags by the buffer length unless the buffer is flushed/reseeked.
- Also consider fixing channel labels (util.cc short names, audio_processor.cc input_names) if a UI names index 13 "Sync".
- Atmos audio: would need (1) a subscriber to `Player::Atmos` in the Butler/FilmViewer path, (2) an IAB decoder + object renderer to N speakers/binaural, synchronized to DCPTime, (3) mixing into the Butler audio stream or replacing the bed. Not present in any dependency; licensing/format constraints (Dolby proprietary, encryption) likely block it.

### Gaps
- No upstream feature request found for channel solo or Atmos playback in the player (searched forum/Mantis briefly; not exhaustive).
