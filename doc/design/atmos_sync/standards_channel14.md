# Dolby Atmos in DCP/MXF: packaging, channel 14 and the Atmos sync signal

## Q1. Which channel of the 16-channel main PCM MXF carries the Atmos sync signal? Standard or convention? What does Dolby specify?

### Takeaway
The sync signal goes on **channel 14 in 1-based numbering (index 13 in 0-based code)**. This is an **ISDCF recommendation (ISDCF Doc 4)** and a de facto industry convention followed by Dolby, DTS:X and Auro gear. It is not fixed by a SMPTE standard: SMPTE ST 430-14 defines a *digital* sync signal but does not assign it to a channel number. I found no public Dolby document that states the channel number.

### Cited Findings
- ISDCF Doc 4 ("16-Channel Audio Packaging Guide", 2017-06-29) channel table, 1-based "Channel in package": 1 L, 2 R, 3 C, 4 LFE, 5 Ls/Lss, 6 Rs/Rss, 7 HI, 8 VI-N, 9 Lc (SDDS only), 10 Rc (SDDS only), 11 Lrs (7.1DS), 12 Rrs (7.1DS), **13 Motion Data ("Synchronous signal (currently used by D-Box)")**, **14 Sync Signal ("Used for external sync (e.g. FSK Sync) - only used for SMPTE-DCP - NOT INTEROP-DCP")**, 15 Sign Language Video, 16 unused. — [ISDCF Doc 4](https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf)
- ISDCF Doc 4 calls itself a recommendation "meant to capture and encourage common practice", and says "it is not guaranteed that all Compositions past, present or future follow this recommendation". It also says an even number of channels shall be used. — [ISDCF Doc 4](https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf)
- ISDCF Doc 4 says the channel order "commonly maps one-to-one to the audio outputs of a player", so the table is also the recommended cinema wiring. This is why the sync signal comes out on server/IMB output 14. — [ISDCF Doc 4](https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf)
- For SMPTE DCPs, ISDCF Doc 4 recommends MCA sub-descriptors (SMPTE ST 429-2 Annex A.2; label parameters in SMPTE ST 428-12), ChannelAssignment = Channel Configuration 4 UL (ST 429-2 Annex A.1), and a copy of the MCA sub-descriptors in the CPL (`http://isdcf.com/ns/cplmd/mca`, per ST 429-16). It also says MCA labels "should not be used for Interop-DCPs". — [ISDCF Doc 4](https://files.isdcf.com/papers/ISDCF-Doc4-Audio-channel-recommendations.pdf)
- DCP Inside (sherpadown): the Atmos MXF "can be accompanied by a synchronization track for the audio processor on track #14 within the main sound MXF file (MainSound)". It describes the data as timecodes. — [DCP Inside: Dolby Atmos (PDF)](https://sherpadown.net/dcp-inside/DolbyAtmos.en.pdf)
- DCP Inside (IAB chapter): track #14 holds an "FSK Sync Signal" for external audio hardware (Atmos or DTS:X devices). It also says "all reels containing an Immersive Audio track must include an FSK Sync signal" in one of the MainSound channels. That is a ST 429-19 constraint as sherpadown reports it; I did not read ST 429-19 itself. — [DCP Inside: IAB](https://sherpadown.net/dcp-inside/IAB.en)
- DCP-o-matic forum (user RAY-NUE, about DTS:X): "insert the time code signal into DCP at track 14 (SMPTE)". — [DCP-o-matic forum t=2243](https://dcpomatic.com/forum/viewtopic.php?t=2243)
- libdcp, the library DCP-o-matic uses, has the enum `dcp::Channel`: LEFT=0, RIGHT=1, CENTRE=2, LFE=3, LS=4, RS=5, HI=6, VI=7, LC=8, RC=9, BSL=10, BSR=11, **MOTION_DATA=12, SYNC_SIGNAL=13**, SIGN_LANGUAGE=14, 15 unused, CHANNEL_COUNT=16. These are 0-based, so SYNC_SIGNAL=13 is ISDCF channel 14. The MCA writer writes `MCAChannelID = static_cast<int>(dcp_channel) + 1`, which is 1-based in the MXF. — [libdcp types.h](https://raw.githubusercontent.com/cth103/libdcp/main/src/types.h), [libdcp sound_asset_writer.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset_writer.cc)
- libdcp sound writer: `if (j == 13 && _sync) { s = _fsk.get(); }`. The header comment says "true to ignore any signal passed to write() on channel 14 and instead write a sync track". Sync is only allowed for SMPTE and when there are at least 14 channels: `DCP_ASSERT (!_sync || _asset->channels() >= 14)` and `DCP_ASSERT (!_sync || standard == SMPTE)`. — [libdcp sound_asset_writer.h](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset_writer.h), [sound_asset_writer.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset_writer.cc)
- DCP-o-matic turns the sync track on automatically when the film has Atmos content: `film()->contains_atmos_content() ? dcp::SoundAsset::AtmosSync::ENABLED : DISABLED` (local repo `src/lib/reel_writer.cc:239`). — [libdcp sound_asset.h](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset.h)
- SMPTE ST 430-14:2015 ("Digital Sync Signal and Aux Data Transfer Protocol") says the sync signal "shall be mapped to a single AES3 channel". I found no channel number anywhere in the text: a search for "channel 13/14", "FSK" and "Dolby" returned nothing. — [SMPTE ST 430-14:2015 PDF](https://pub.smpte.org/doc/st430-14/20150801-pub/st0430-14-2015.pdf)

### Inferences
- The numbering confusion comes from mixing conventions. "Channel/track 14" (ISDCF, cinema wiring, MCAChannelID, Pro Tools/Resolve track numbers) is 1-based. It equals index 13 in 0-based code such as libdcp and asdcplib buffers. By the same rule, motion data is 1-based 13 (index 12) and sign language is 1-based 15 (index 14). "Channel 13" in code is the same thing as "channel 14" in documentation.
- Two different sync-signal families exist:
  - The **FSK sync** that libdcp generates (see Q4) and that ISDCF names ("e.g. FSK Sync").
  - The **SMPTE ST 430-14 digital sync** (lead/tail 24-bit sample pairs, marker 0xAAF0).

  They are not the same bitstream. "Sync signal on channel 14" therefore does not by itself say which one.
- A 5.1/7.1 DCP with fewer than 14 channels cannot carry the sync. Atmos DCPs are therefore in practice made with 16 channels (a DCP-o-matic forum user reported "I did change the DCP settings to 16 channels" to make Atmos work; [DCP-o-matic forum p=8081](https://dcpomatic.com/forum/viewtopic.php?p=8081)).

### Gaps
- I found no public Dolby specification (CP850/CP950/IMS3000 manuals, Atmos mastering guidelines) that names channel 14. The Dolby manuals are behind a login or not indexed.
- I did not read the text of SMPTE ST 429-19 or ST 429-2:2020 to see whether either one pins the sync to a specific channel.

## Q2. How is the Atmos MXF track file structured (frames, metadata, encryption, keys/KDMs), and how does IAB differ?

### Takeaway
- **Legacy Dolby Atmos** is a separate SMPTE MXF track file. It uses frame-wrapped "D-Cinema Data" (Aux Data, ST 429-14 style) with a Dolby-specific `DolbyAtmosSubDescriptor`, and it is referenced in the CPL through the Dolby namespace element `axd:AuxData`.
- **IAB** is the standardized successor: an ST 2098-2 bitstream in an ST 429-18 Immersive Audio track file, with ST 429-19 DCP constraints and the ISDCF Doc 15 / RDD 57 "Profile 1" limits.
- In both cases the main-sound PCM MXF is a separate asset that carries the sync on channel 14.
- Both are encrypted with the MDEK key type.

### Cited Findings
- asdcplib (Atmos reader/writer):
  - Package label: "File Package: SMPTE-GC frame wrapping of Dolby ATMOS data". Track label: "Dolby ATMOS Data Track".
  - `ATMOS_ESSENCE_CODING` (DataEssenceCoding) UL = `06.0e.2b.34.04.01.01.05.0e.09.06.04.00.00.00.00`.
  - The essence descriptor is a `PrivateDCDataDescriptor` plus a `DolbyAtmosSubDescriptor`.
  - asdcplib's MDD keeps "Old DCData UL values, needed for continued support of Atmos".

  — [asdcplib AS_DCP_ATMOS.cpp](https://raw.githubusercontent.com/cth103/asdcplib/master/src/AS_DCP_ATMOS.cpp), [asdcplib MDD.cpp](https://raw.githubusercontent.com/cth103/asdcplib/master/src/MDD.cpp)
- `DolbyAtmosSubDescriptor` properties, from asdcplib MDD:

  | Property | UL |
  |---|---|
  | AtmosVersion | `06.0e.2b.34.01.01.01.05.0e.09.05.06…` |
  | MaxChannelCount | `…0e.09.05.07…` |
  | MaxObjectCount | `…0e.09.05.08…` |
  | AtmosID (UUID) | `…0e.09.05.09…` |
  | FirstFrame | `…0e.09.05.0A…` |

  libdcp's `AtmosAsset` reads these same fields, together with EditRate and ContainerDuration. — [asdcplib MDD.cpp](https://raw.githubusercontent.com/cth103/asdcplib/master/src/MDD.cpp), [libdcp atmos_asset.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/atmos_asset.cc)
- The Atmos asset is SMPTE-only: libdcp constructs it as `MXF (Standard::SMPTE)`. — [libdcp atmos_asset.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/atmos_asset.cc)
- CPL reference for legacy Atmos (libdcp `ReelAtmosAsset`):
  - Element `axd:AuxData`, namespace `http://www.dolby.com/schemas/2012/AD`.
  - `axd:DataType` = `urn:smpte:ul:060e2b34.04010105.0e090604.00000000`.

  — [libdcp reel_atmos_asset.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/reel_atmos_asset.cc)
- DCP Inside lists the same UL `urn:smpte:ul:060e2b34.04010105.0e090604.00000000` as the CPL DataType for IAB. It gives the optional CPL ExtensionMetadata "IAB Profile" = `SMPTE-ST-2098-2:2019-P1`. — [DCP Inside: IAB](https://sherpadown.net/dcp-inside/IAB.en)
- SMPTE ST 429-18 (Immersive Audio Track File): the file contains IABitstream frames as defined in ST 2098-2 and "shall conform to SMPTE ST 429-3". All IAFrames have the same FrameRate, BitDepth and SampleRate, and samples are 24-bit. It is designed for backward compatibility with existing Dolby Atmos content and renderers, and works with the ST 429-19 operational constraints. — [SMPTE ST 429-18 page](https://pub.smpte.org/latest/st429-18/index.html), [SMPTE webcast ST 429-18/429-19](https://www.smpte.org/webcast-events/st-429-18-immersive-cinema-track-file-and-429-19-immersive-dcp-constraints)
- Standards stack as DCP Inside lists it:
  - ST 2098-1 (metadata)
  - ST 2098-2:2018/2019 (IAB bitstream)
  - ST 2098-5 (channels and soundfield groups)
  - ST 429-18 (track file)
  - ST 429-19 (DCP constraints)
  - RDD 57:2021 (IAB Application Profile 1)

  — [DCP Inside: IAB](https://sherpadown.net/dcp-inside/IAB.en)
- ISDCF Doc 15 (IAB Application Profile 1, v1.0.0, 2020-06-12). Limits:
  - Sample rate: 48 kHz.
  - Edit rate: 24, 25, 30, 48, 50 or 60 fps.
  - At most 10 bed channels, at most 118 objects, MaxRendered ≤ 128.
  - Bed: only a 7.1DS or 9.1OH bed, and only these ChannelIDs: 0x0 L, 0x2 C, 0x4 R, 0x5 Lss, 0x7 Lrs, 0x8 Rrs, 0x9 Rss, 0xB Lts, 0xC Rts, 0xD LFE.
  - No AuthoringToolInfo and no UserData; object spread is 1D only.
  - CPL ContentTitleText audio type is "IAB".
  - Packaging per ST 429-18 and ST 429-19.
  - "It is believed that legacy content (Dolby Atmos) follows the guidelines of IAB Profile 1. None of this legacy content includes the CPL identification information." A CPL without that metadata is treated as Profile 1 "Legacy".

  — [ISDCF Doc 15](https://files.isdcf.com/papers/ISDCF-Doc15-IAB-Profile-1-202006012.pdf)
- Dolby Atmos cinema capacity: 64 speakers, 128 channels, 118 objects (118 = 128 minus the 10 bed channels of 7.1.2, per the IMF IAB interoperability guidelines as DCP Inside quotes them). DCP Inside says the Dolby Atmos bitstream "doesn't add any additional elements compared to" IAB. — [DCP Inside: Dolby Atmos (PDF)](https://sherpadown.net/dcp-inside/DolbyAtmos.en.pdf)
- Encryption and KDM:
  - The SMPTE ST 430-1 2019 amendment adds IAB. The key type MDEK originally meant the AuxData essence key and now also covers the Immersive Audio essence key.
  - A DCP can need two KDMs: one for the media block (MDIK/MDAK/MDSK keys) and one for the immersive audio processor (MDEK keys).
  - Forensic-marking flag `mrkflg-audio-disable-above-channel-12` exempts channels 13 and above (motion data and sync) from audio watermarking.

  — [DCP Inside: IAB](https://sherpadown.net/dcp-inside/IAB.en)
- Main sound in an IAB DCP: DCP Inside says the MainSound MXF must not contain IAB data, and its channels are "empty except for accessibility tracks (HI, VIN) and motion data", i.e. no fallback audio. — [DCP Inside: IAB](https://sherpadown.net/dcp-inside/IAB.en). This contradicts common practice seen in the DCP-o-matic community, where full 5.1/7.1 fallback is combined with Atmos ([DCP-o-matic forum: Atmos with 5.1 fallback](https://test.dcpomatic.com/forum/viewtopic.php?t=2321)). It may be a ST 429-19 rule for IAB-only CPLs that sherpadown paraphrased; I could not verify this.

### Inferences
- The legacy Atmos track file and the IAB track file share the same DataEssenceCoding/DataType UL (`…0e.09.06.04…`), so the difference is mostly in the wrapping. Legacy uses Dolby's `axd:AuxData` CPL element and `DolbyAtmosSubDescriptor`. IAB uses the ST 429-18 descriptors and the SMPTE CPL extension, plus the "IAB" naming/profile metadata. This matches ST 429-18's stated goal of backward compatibility.
- Each Atmos frame corresponds to one picture edit unit. DCP-o-matic requires the Atmos edit rate to equal the DCP rate ("There's no fiddling with frame rates when we are using Atmos", local `src/lib/atmos_decoder.cc`).

### Gaps
- I did not read the byte-level Atmos frame payload layout (Dolby's "Atmos Cinema bitstream" document), which is not public.
- I did not read the text of ST 429-18 / ST 429-19; the essence UL and descriptor details of ST 429-18 were not verified.
- The sherpadown MXF-Dolby Atmos chapter URL returned 404.

## Q3. Can the Atmos essence be extracted/decoded/rendered by third-party tools? Is the format publicly documented?

### Takeaway
- **Wrapping and unwrapping** is open: asdcplib and libdcp read and write Atmos MXF, and DCP-o-matic can carry an existing Atmos MXF into a DCP.
- **Rendering** is open only through the SMPTE IAB route: the ST 2098-2 bitstream is a SMPTE standard with an open reference library, and legacy Atmos is believed to be Profile 1 compatible.
- **Creating** a theatrical Atmos MXF still requires Dolby tools.
- Dolby's own cinema bitstream document is not public.

### Cited Findings
- asdcplib implements `ASDCP::ATMOS::MXFReader`/`MXFWriter` (ReadFrame with AES decryption and HMAC contexts). — [asdcplib AS_DCP_ATMOS.cpp](https://raw.githubusercontent.com/cth103/asdcplib/master/src/AS_DCP_ATMOS.cpp)
- libdcp has `AtmosAsset`, with a reader and a writer. — [libdcp atmos_asset.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/atmos_asset.cc), [libdcp atmos_asset_writer.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/atmos_asset_writer.cc)
- DCP-o-matic can include pre-encoded Atmos but "cannot encode an Atmos DCP by feeding it with some Dolby Atmos file, like ADM BWF, .atmos file or just Dolby Digital Plus JOC audio". — [DCP-o-matic forum t=2110](https://dcpomatic.com/forum/viewtopic.php?t=2110)
- Carsten (DCP-o-matic forum): DCP-o-matic "has no specific features to generate or convert ATMOS files for cinema (except the ability to create an ATMOS timecode track)". — [DCP-o-matic forum t=2243](https://dcpomatic.com/forum/viewtopic.php?t=2243)
- Jaume (Dolby), DCP-o-matic forum, 2022-12-29:
  - "The only way to create a Theatrical Atmos MXF *for DCP* is using a *loan* software in certified Hardware delivered ONLY to Cinema Mixing DOLBY CERTIFIED Theatrical mix stage."
  - Also: "no fee or agreement signed with Dolby is needed anymore as before. from 28 Nov 22", with exceptions for wide theatrical distribution.

  — [DCP-o-matic forum p=8081](https://dcpomatic.com/forum/viewtopic.php?p=8081)
- ST 2098-2 is published by SMPTE on GitHub (`SMPTE/st2098-2`). The search results also mention IABLib, an open-source C++ library for "creating, parsing and rendering IAB essence" per ST 2098-2 (from search snippets only; not fetched). — [GitHub SMPTE/st2098-2](https://github.com/SMPTE/st2098-2), [amazing-digital-cinema list](https://github.com/avtools-io/amazing-digital-cinema)
- CineIA_CLI converts IMF IAB (ST 2067-201) to DCP IAB. — [GitHub izwb003/CineIA_CLI](https://github.com/izwb003/CineIA_CLI) (search result, not fetched)
- A film-tech thread (summarized in search): ST 2098-2 "was adapted from Dolby's Atmos Cinema content document", so the bitstreams are very similar. IAB extension features not in Atmos may fail on Atmos processors. — [Film-Tech: Dolby Atmos vs SMPTE 2098-2 / IAB](https://www.film-tech.com/vbb/forum/main-forum/26748-dolby-atmos-vs-smpte-2098-2-iab-format) (redirects to ft-forum.com; I read only the search summary)
- DCP-o-matic forum: IAB is license-free (ST 2098-2:2019). At the time of that thread, no commercial software exported SMPTE IAB for DCP, only for IMF. That is old information and may no longer hold. — [DCP-o-matic forum t=2110](https://dcpomatic.com/forum/viewtopic.php?t=2110)

### Inferences
- Because legacy Atmos is said to follow IAB Profile 1 and uses the same DataType UL, an ST 2098-2 parser/renderer (IABLib) should in principle be able to parse unencrypted legacy Atmos frames once they are unwrapped with asdcplib. I did not verify this in these sources.
- Encrypted Atmos needs the MDEK key from a KDM addressed to a certificate the tool holds. Encrypted Atmos is therefore not practically decodable by third parties.

### Gaps
- I found no public Dolby specification for the legacy Atmos frame payload.
- I did not confirm that IABLib renders legacy Dolby Atmos DCP frames.
- I did not check which commercial tools (Resolve, easyDCP, Clipster) export ST 429-18 IAB DCPs as of 2026.

## Q4. What role does the sync signal play in linking the Atmos track to the picture/PCM at playback?

### Takeaway
- The immersive processor (Dolby CP850/CP950/IMS3000 audio, DTS:X, Auro) receives channel 14 of the server's audio output. From it the processor decodes a timecode-like stream: frame count plus an asset identifier.
- It uses that stream to find and play the right immersive frames in step with the picture and main sound. It gets those frames either as a file pushed or ingested separately, or over the LAN (ST 430-14/430-10).
- The signal lives in the main-sound PCM and follows it sample-exactly, so any edit, SRC or gain on channel 14 breaks it.

### Cited Findings
- libdcp's FSK sync (the implementation DCP-o-matic uses):
  - Timing: 48 kHz only, 4 samples per bit, i.e. 12 kbit/s. Samples come from a fixed lookup table at amplitude about 0.038–0.092 of full scale, with polarity flipping.
  - Packets per edit unit: 4 at 24/25/30 fps, 2 at 48/50/60 fps, 1 at 96/100/120 fps.
  - Packet layout:
    - 0x4D 0x56 marker
    - 4-bit edit-rate code (0=24 … 8=120)
    - 2 zero bits
    - 2-bit packet index (0–3)
    - 4 bytes of the **main sound asset's UUID** (bytes i*4…i*4+3, so the UUID spans 4 packets)
    - 24-bit frame counter (`_frames_written`)
    - CRC-16 (polynomial 0x1021)
    - 4 zero bits and padding
  - A new packet set is generated for every frame written.

  — [libdcp sound_asset_writer.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset_writer.cc), [libdcp fsk.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/fsk.cc)
- SMPTE ST 430-14:2015, a different and standardized sync. It is an "audio sample-accurate binary signal generated by an Emitter, typically the Image Media Block, at time of playback", on one AES3 channel.
  - Signal: 24-bit Lead/Tail sample pairs (the tail is the two's complement of the lead; peaks are limited to about −36 dBFS to protect speakers). Payload starts with marker 0xAAF0.
  - Fields: Flags/Status (Stopped/Paused/Playing), Timeline Edit Unit Index, Playout ID, Edit Unit Duration (2000 samples at 24 fps/48 kHz), sample duration, Primary Picture Output and Screen Offsets (±500 ms), picture and sound track-file edit unit plus UUID, CPL UUID.
  - The signal is invalid if no valid packet arrives within 3 s.
  - "The first Sample of the Packet shall be output simultaneously with the first audio sample of the Primary Sound Track File of the Timeline Edit Unit".
  - The processor fetches aux data items (ST 429-14 Aux Data track files) from the server over HTTP, after discovery via ST 430-10 ACS.

  — [SMPTE ST 430-14:2015](https://pub.smpte.org/doc/st430-14/20150801-pub/st0430-14-2015.pdf)
- ST 430-14 also notes that processing "e.g. sample rate conversion and/or gain, between Emitter and Processor can result in a corrupted signal". — [SMPTE ST 430-14:2015](https://pub.smpte.org/doc/st430-14/20150801-pub/st0430-14-2015.pdf)
- Who generates the sync: sources conflict.
  - A Knut Erik Evensen guide (search snippet) says that when a Dolby Atmos DCP is created, track 14 is silent and "when the DCP is played it will be an automatically generated sync track from the Dolby Atmos MXF track". — [knuterikevensen.com (easyDCP 7.1 VF guide)](https://www.knuterikevensen.com/2021/07/13/how-to-make-a-7-1-vf-dcp-in-easydcp-creator-plus/) (snippet only)
  - Search snippets say that "as long as the sync signal is created at the time the main audio track is wrapped into MXF, it will be valid" (source among the DCP-o-matic forum results above; the exact page was not fetched).
  - libdcp/DCP-o-matic always bakes the FSK sync into channel 14 when the film contains Atmos. — [libdcp sound_asset_writer.cc](https://raw.githubusercontent.com/cth103/libdcp/main/src/sound_asset_writer.cc)
  - ST 430-14 assumes the IMB emits the sync live at playback. — [SMPTE ST 430-14:2015](https://pub.smpte.org/doc/st430-14/20150801-pub/st0430-14-2015.pdf)
- DCP Inside says the Atmos sync data are timecodes (its spectral analysis is still marked "TODO"). — [DCP Inside: Dolby Atmos (PDF)](https://sherpadown.net/dcp-inside/DolbyAtmos.en.pdf)

### Inferences
- The libdcp packet puts the main sound MXF UUID and a frame counter into the signal. A processor that has ingested the CPL can map "sound asset X, frame N" to the reel's Atmos track file and frame N. This link to a specific asset ID is why a baked sync must be regenerated whenever the main sound MXF is re-wrapped (new UUID) or retimed.
- ISDCF's "only SMPTE-DCP" note and libdcp's SMPTE-only assertion agree: Atmos and FSK sync are SMPTE-only. The Atmos track file itself is SMPTE-only too.
- Two practices seem to coexist:
  1. The sync is baked into the main sound at mastering time (DCP-o-matic, apparently Dolby mastering).
  2. The server/IMB generates it or overwrites channel 14 at playback (ST 430-14 model; the Evensen snippet).

  Which applies depends on the server/IMB and processor generation. I found no authoritative source that settles it for Dolby IMS3000/CP950.

### Gaps
- I found no Dolby document that specifies the FSK sync format, or says whether Dolby servers regenerate channel 14 at playback. The origin of libdcp's 0x4D56 FSK packet format (Dolby-supplied? reverse-engineered?) is not documented in the fetched code.
- I did not check whether current (2026) Dolby processors accept ST 430-14 digital sync in addition to FSK.
- SMPTE ST 2067-201 (IMF IAB plug-in) was not researched in depth; I have only its mention as the IMF counterpart (CineIA_CLI converts IMF IAB to DCP IAB).
