# Real-time playback and selection of Dolby Atmos and individual channels (channel 14) from DCP MXF

## Cinema servers / media blocks and processors: how Atmos vs PCM is chosen, what happens without ch14 sync, operator switching

### Takeaway
In a cinema chain the Atmos/PCM choice is made by the processor (CP850/CP950A, or an IMS3000 with internal Atmos audio), not by the server: the Atmos MXF is pushed to the processor before the show, and the processor plays Atmos only while it can lock to the sync signal on AES channel 14 of the main PCM track; if it cannot, it "reverts" to the PCM 5.1/7.1 in channels 1-8 automatically. The operator's real-time controls are macro/format selection and an enable/disable Dolby Atmos switch on the processor; there is no documented "solo channel 14" control on these products for listening (ch14 is a data signal, not audio).

### Cited Findings
- CP850: "To ensure that the Dolby Atmos soundtrack is in sync with the picture, the CP850 uses a synchronization signal embedded into channel 14 of the DCP Main Soundtrack. This signal sends information to the CP850 about the ID of the track and frame position." — [CP850 Release Notes v2.1.0.10](https://smart-story.ru/files/products/multimedia/Audio_processors/Dolby/CP850/Docs/dolby_cp850_release_notes_v2_1_0_10.pdf)
- CP850 release notes troubleshooting section: "If you are having a problem where the CP850 reverts to PCM audio instead of playing the encrypted Dolby Atmos audio, one of the first things to check is the content Key Delivery Message (KDM)." Audio Forensic Marking (AFM) of encrypted content can disrupt the sync signal; unencrypted content has no AFM. All KDMs for Atmos content must carry the DCI "disable-above-channel-12" flag so AFM is not applied to channels 13-16. — [CP850 Release Notes v2.1.0.10](https://smart-story.ru/files/products/multimedia/Audio_processors/Dolby/CP850/Docs/dolby_cp850_release_notes_v2_1_0_10.pdf)
- CP850 status reporting includes bitstream states "PCM, Dolby Atmos, reverted to PCM". Latency: "PCM only = 20msec", "PCM + Dolby Atmos = 80msec (previously 251msec)". — [CP850 Release Notes v2.1.0.10](https://smart-story.ru/files/products/multimedia/Audio_processors/Dolby/CP850/Docs/dolby_cp850_release_notes_v2_1_0_10.pdf)
- CP950A manual (Issue 13, Aug 2024): "The packaged Dolby Atmos audio tracks are hosted on the playback system and transferred to the CP950A in advance of playback. If needed, the content is decrypted. The audio is then rendered for the specific room defined in the CP950A. When the content is played, there is a synchronization signal on AES channel 14. This signal provides clip information and timing information to match the video and audio." — [Dolby CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A manual: watermark data "can cause problems on channel 14, resulting in Dolby Atmos rendering that reverts to PCM (standard 5.1 or Dolby Surround 7.1)." — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A manual: "To play back Dolby Atmos content in a CP950A, the macro must have Dolby Atmos sync assigned to channel 14." When a macro uses the "16ch MediaBlock + Dolby Atmos" input, "the CP950A accepts PCM audio at 48 kHz and plays back Dolby Atmos when available." — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A status strings (remote/serial): "pcm 5.1", "pcm 7.1", "atmos: Indicates the content is Dolby Atmos and the CP950A is playing correctly in Dolby Atmos", "reverted: Indicates that the content is Dolby Atmos but the CP950A cannot process that content so it is playing in pcm mode". — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A front panel system settings let the user "enable/disable Dolby Atmos on a CP950A"; device tools also include booth monitor output selection and "Center channel bypass". — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A "failover room configuration": if the CAT1710 media block fails, "the CP950A cannot playback Dolby Atmos or use more than 16 channels", can revert to a failover routing defined in Dolby Atmos Designer, "the system can play only 5.1 or 7.1 audio (no Dolby Atmos)"; on hardware failure "playback may stop, and a reboot is required". — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP950A Atmos Connect ports carry up to 16 (CP950) or 64 (CP950A) channels of AES67 audio. — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- DSS200 servers use an Ethernet port for the Atmos input to the CP950A; the earliest DSS200 units lack that port and cannot be upgraded. — [CP950/CP950A Manual Issue 13](https://professional.dolby.com/siteassets/products/cp950a/dolby_cp950-cp950a_manual_issue_13.pdf)
- CP850 Base "will play back traditional Dolby Surround 7.1 and 5.1 formats as well as render select Dolby Atmos titles to a Dolby Surround 7.1 configuration." — [Dolby CP850 product page](https://professional.dolby.com/product/dolby-cinema-audio-products/cp850/)
- IMS3000 can drive an external CP950/CP950A, or be field-enabled for internal 5.1/7.1 or full Atmos processing; a "Server Mode" IMS3000 passes PCM only on pairs 1-4 (channels 1-8). — [Film-Tech: Ims3000](https://www.film-tech.com/vbb/forum/main-forum/38095-ims3000) (via search snippet; also [Dolby IMS3000 product page](https://professional.dolby.com/product/dolby-cinema-imaging-products/ims3000/))
- Barco ICMP-X + CP950A (fw 2.3.2.5): the ICMP-X would not show/ingest Atmos DCPs until the CP950A's "Server Compatibility Mode" (System > Preferences) was set to Barco; the ICMP-X apparently only offers Atmos/IAB CPLs when it detects a valid IAB-decoder connection (Leo Enticknap; Mike Renlund of Dolby explaining certificate presentation). — [Film-Tech: Barco ICMP-X with CP950A Atmos gotcha](https://ft-forum.com/ft/vbb/forum/main-forum/49620-barco-icmp-x-with-cp950a-atmos-gotcha)
- CP950A thread mentions using AES channel 14 for sync to the CP950A. — [Film-Tech: IMS3000/CP950A/Q-Sys sync issue](https://www.film-tech.com/vbb/forum/main-forum/38152-completely-bizarre-audio-sync-issue-ims3000-cp950a-q-sys) (search snippet only)
- Atmos DCPs use 14 channels with the sync track on channel 14 and the 5.1/7.1 fallback on channels 1-6/1-8; channel 13 is used for D-Box motion sync. — [DCP-o-matic forum: DCP with Atmos for Cinema Release](https://dcpomatic.com/forum/viewtopic.php?t=1845); [DCP-o-matic forum: Atmos track best practices](https://dcpomatic.com/forum/viewtopic.php?t=1766)

### Inferences
- The fallback is automatic and continuous: the same PCM main track always plays; Atmos rendering is layered on only while sync on ch14 is valid. Losing ch14 mid-show (AFM, bad routing, wrong macro) gives "reverted" PCM 5.1/7.1, not silence, as long as channels 1-8 contain a proper fallback mix.
- Real-time operator switching Atmos to PCM is possible on the CP950A by changing macro/format or toggling Atmos enable; the CP850 exposes the same idea through macros (the CP850 manual itself was not readable here).
- The ch14 sync signal is not meant for listening; no processor documentation found offers soloing it to a speaker. On a CP950/CP850 macro, ch14 is marked as Atmos sync and not routed to the room.

### Gaps
- No readable primary text for CP850 manual behaviour when ch14 is present but no Atmos MXF was ingested, or exact timeouts for revert and re-lock (Scribd/manuals.plus copies were not fetched).
- No primary documentation found for Christie (IMB-S3/CP4450 integrated), GDC SR-1000/IMB, Doremi/Dolby DCP-2000/ShowVault Atmos behaviour beyond "server hosts Atmos MXF and streams it to the processor"; the Doremi ShowVault CP850 cable page returned 403.
- Whether any Barco/GDC IMB with a built-in IAB/DTS:X decoder allows live Atmos-vs-PCM switching was not documented in sources found.

## Software players and tools: can they play/render Atmos MXF or pick individual PCM channels live? Licences

### Takeaway
No general-purpose software player renders Dolby's proprietary DCP Atmos MXF (DCData/"ATMOS" essence). easyDCP Player+ (v4.3+) renders IAB (ST 2098-2) tracks to 2.0/5.1/7.1/7.1.4 and has per-channel input/output routing. DCP-o-matic Player reads Atmos MXF through libdcp but only passes it through on DCP creation; playback ignores it, while its preferences have an output audio mapping that can route any PCM channel (including 14) to an output. PCM channels, including ch14, are ordinary 24-bit PCM in the main sound MXF, so any MXF-capable tool (ffmpeg, asdcp-unwrap) can extract them.

### Cited Findings
- easyDCP Player+: "Playback of IAB (Immersive Audio Bitstream) DCPs, including Dolby Atmos content, is supported in easyDCP Player+ starting from version 4.3" for "All license models"; select "IAB Auxiliary Track" instead of the channel-based mix and choose output Stereo, 5.1/7.1 or 7.1.4; "The renderer will automatically map audio objects and channels to your selected configuration." — [easyDCP FAQ: IAB playback](https://en.easydcp.com/support-faq.php?id=179&p=playing-back-a-dcp-with-immersive-audio-bitstream-iab-dolby-atmos-with-easydcp-player-plus)
- The easyDCP FAQ does not say whether the switch between IAB and PCM track can be made while playing, nor whether proprietary (non-IAB) Dolby Atmos MXF renders. — [easyDCP FAQ](https://en.easydcp.com/support-faq.php?id=179&p=playing-back-a-dcp-with-immersive-audio-bitstream-iab-dolby-atmos-with-easydcp-player-plus)
- easyDCP Player+ audio routing: an "Input Channel Order" (from MXF metadata or a default) and an "Output Channel Mapping" that can mix several inputs to one output; it notes "DCPs containing Dolby Atmos Sound Tracks require a sync signal added to the 'traditional' 5.1 / 7.1 Sound Track File" with channel assignment #4 (Wild Track Format); Interop channels 13-16 are listed as Motion Data, Sync Signal, Unused. — [easyDCP docs: Audio Routing](https://docs.easydcp.com/products/player/audio_routing.html)
- DCP-o-matic can add Atmos MXF made with Dolby tools to a DCP "just like any other content" and copy it when editing a DCP, but cannot encode Atmos from ADM BWF, .atmos or DD+ JOC. — [DCP-o-matic forum: Any possible solution for Atmos content today?](https://dcpomatic.com/forum/viewtopic.php?t=2110&start=10); [DCP-o-matic forum: Atmos track best practices p.2](https://dcpomatic.com/forum/viewtopic.php?t=1766&start=10)
- DCP-o-matic source (local repo, current main): Atmos frames are read via libdcp `dcp::AtmosAsset`/`AtmosAssetReader` (`src/lib/atmos_mxf_decoder.cc`) and emitted by `Player::atmos` (`src/lib/player.cc:1636`); the only consumer of `Player::Atmos` is `DCPFilmEncoder` (`src/lib/dcp_film_encoder.cc:69`), i.e. the viewer/player never renders Atmos. The viewer uses `Config::audio_mapping(output_channels)` (`src/wx/film_viewer.cc:249`) to map DCP channels to sound-card outputs. — [DCP-o-matic repository](https://github.com/cth103/dcpomatic) (verified in local checkout)
- DCP-o-matic manual chapter on playing DCPs (playlist, Play button). — [DCP-o-matic manual ch. 18](https://dcpomatic.com/manual/html/ch18.html)
- Dolby Atmos Conversion Tool converts between master formats (IAB.mxf, .atmos, BWF.wav, and to dub_out.rpl) and can do edits/frame-rate conversion, but IMF IAB exported from Production/Mastering Suite "is not compatible with DCP"; a cinema Atmos MXF requires a Dolby theatrical RMU. — [Dolby developer KB: MXF IAB export master and DCP?](https://developerkb.dolby.com/support/discussions/topics/16000027539); [Dolby Atmos Conversion Tool](https://professional.dolby.com/product/dolby-atmos-content-creation/dolby-atmos-conversion-tool/)
- IMF IAB follows ST 2067-201 (IAB Level 0 plug-in); DCP IAB follows RDD 57 / IAB Application Profile 1; they are not interchangeable. CineIA_CLI converts IMF IAB to DCP IAB. — [CineIA_CLI](https://github.com/izwb003/CineIA_CLI); [ISDCF Doc 15 IAB Profile 1](https://files.isdcf.com/papers/ISDCF-Doc15-IAB-Profile-1-202006012.pdf)

### Inferences
- To hear or inspect channel 14 live in DCP-o-matic Player, the existing output audio mapping can route input 14 to a sound-card output; it will sound like a data/timecode-like signal, since it is sync data, not programme audio.
- easyDCP Player+ is the only software player found that renders cinema immersive audio, and only as IAB; a Dolby-proprietary Atmos DCP (pre-IAB "ATMOS" DCData MXF) would only play its PCM fallback.
- ffmpeg can demux the 16-ch PCM main sound MXF and select a channel (e.g. `pan`/`channelmap` filters), but has no decoder for Atmos DCData or IAB essence.

### Gaps
- No sources were fetched for NeoDCP, Clipster, DaVinci Resolve or Dolby Atmos Renderer DCP Atmos handling; not verified, so no claims made.
- ffmpeg behaviour on Atmos/IAB MXF is not confirmed from a primary source (inference only).
- Whether easyDCP IAB/PCM switching is live during playback is undocumented.

## Real-time rendering of Atmos cinema MXF outside Dolby hardware

### Takeaway
For proprietary Dolby DCP Atmos, rendering stays on Dolby hardware (CP850/CP950A/IMS3000 or the RMU for mastering). For IAB (the SMPTE path, used by "Atmos" in newer DCPs), software renderers exist: the open-source DTS/Xperi IABLib (DTSProAudio/iab-renderer) and the renderer inside easyDCP Player+; DTS:X for IAB is licensed to server makers such as GDC.

### Cited Findings
- DTSProAudio/iab-renderer: "IABLib is a C++ library for creating, parsing and rendering IAB essence, as specified in SMPTE ST 2098-2"; no runtime dependencies; includes a render-to-files application; repository last pushed 2023-06-26. — [DTSProAudio/iab-renderer](https://github.com/DTSProAudio/iab-renderer)
- DTSProAudio/iab-validator: open-source CLI validator for ST 2098-2 (2018 and 2019 revision) and IAB Profile 1, MIT-style licence by Xperi. — [DTSProAudio/iab-validator](https://github.com/DTSProAudio/iab-validator)
- SMPTE ST 2098-2 revision extends IAB to 30000/1001 and 60000/1001 fps to align with ST 2067-21 IMF. — [SMPTE/st2098-2](https://github.com/SMPTE/st2098-2)
- GDC Technology and DTS agreement to deliver DTS:X IAB solutions (IAB decoding/rendering in cinema servers). — [GDC press release](https://www.gdc-tech.com/press-release/gdc-technology-and-dts-sign-landmark-agreement-to-deliver-dtsx-iab-solutions/); [Digital Cinema Report: DTS:X IAB SDK](https://www.digitalcinemareport.com/news/dts-releases-dtsx-immersive-audio-bitstream-software-development-kit)
- Proper theatrical Atmos MXF requires a Dolby Theatrical RMU. — [Dolby developer KB](https://developerkb.dolby.com/support/discussions/topics/16000027539)

### Inferences
- IABLib's render-to-files app is offline; real-time use would require integrating the library into a player. Nothing found shows an open-source real-time IAB player.
- No open-source renderer for Dolby's proprietary DCP Atmos (DCData "ATMOS") essence exists; the format is not publicly specified.

### Gaps
- No source found on EBU tools (e.g. EAR renderer / libear) supporting IAB or DCP Atmos directly; ADM-based EBU tools would need a conversion step.
- Licensing terms of the IABLib LICENSE file were not read in full.

## Open-source libraries that read/demux Atmos or IAB MXF

### Takeaway
asdcplib reads and writes Dolby Atmos DCP track files (as DCData with an Atmos descriptor), reads/writes AS-02 IAB (ST 2067-201), and can generate/insert the ch14 Atmos sync channel when wrapping PCM (`asdcp-wrap -s`). libdcp (used by DCP-o-matic) wraps this as `dcp::AtmosAsset`. None of them decode or render the Atmos payload; they only extract the bytestream.

### Cited Findings
- AS_DCP.h essence types include `ESS_DCDATA_DOLBY_ATMOS` ("the file contains one or more DolbyATMOS bytestreams") and `ESS_AS02_IAB` ("an IAB stream (per SMPTE ST 2067-201)"); `ATMOS::AtmosDescriptor` extends `DCDataDescriptor` with `AtmosID` (UUID of Atmos project). — [asdcplib AS_DCP.h](https://github.com/cinecert/asdcplib/blob/master/src/AS_DCP.h)
- asdcp-wrap option: "-s Insert a Dolby Atmos synchronization channel when wrapping PCM. This implies a -L option (SMPTE ULs) and will overide -C and -l options with Configuration 4 Channel Assigment and no format label respectively." Hard-coded sample Atmos properties: `max_channel_count(10), max_object_count(118)`. — [asdcplib asdcp-wrap.cpp](https://github.com/cinecert/asdcplib/blob/master/src/asdcp-wrap.cpp)
- `AtmosSyncChannelMixer` pads with silence channels up to `ATMOS::SYNC_CHANNEL` and then mixes in the Atmos sync channel. — [asdcplib AtmosSyncChannel_Mixer.cpp](https://github.com/cinecert/asdcplib/blob/master/src/AtmosSyncChannel_Mixer.cpp)
- asdcp-unwrap extracts Atmos MXF payload to files with extension ".atmos" (unknown DCData to ".dcdata"). — [asdcplib asdcp-unwrap.cpp](https://github.com/cinecert/asdcplib/blob/master/src/asdcp-unwrap.cpp)
- AS_02_IAB.h: reader/writer classes for IAB track files per ST 2067-201 (IMF). A Dolby-contributed IMF IAB branch exists. — [asdcplib AS_02_IAB.h](https://github.com/cinecert/asdcplib/blob/master/src/AS_02_IAB.h); [asdcplib Dolby imf_iab branch](https://github.com/cinecert/asdcplib/tree/DolbyLaboratories-dolby/imf_iab_implementation)
- DCP-o-matic's `AtmosMetadata` carries first frame, max channel count, max object count and Atmos version from `dcp::AtmosAsset`. — [DCP-o-matic repository](https://github.com/cth103/dcpomatic) (`src/lib/atmos_metadata.cc`, local checkout)

### Inferences
- The ch14 sync signal is algorithmically generated from the Atmos track UUID and frame position (the mixer is constructed with the track UUID), so ch14 content is tied to one specific Atmos MXF; swapping Atmos MXFs without regenerating ch14 would break lock and cause "reverted to PCM".

### Gaps
- The exact ch14 sync signal encoding (Dolby spec) was not found in a public source; only the asdcplib implementation exists as reference.
- libdcp's DCP IAB (RDD 57) support was not checked in this session.
