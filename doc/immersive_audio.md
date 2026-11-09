# Immersive audio in DCPs: IAB and Dolby Atmos

This note collects what differs between an IAB track file and a Dolby Atmos one in a DCP, what DCP-o-matic does
with them, and how `immersive_audio_origin()` (`src/lib/immersive_audio_origin.h`) tells them apart.

Sources: SMPTE ST 2098-2:2022 (the IAB bitstream), SMPTE RDD 57:2021 (IAB Application Profile 1, the profile for
DCPs), SMPTE ST 429-18:2019 (the immersive audio track file), SMPTE RDD 29 (the Dolby Atmos bitstream), the
`DbyCinema` constraint set of DTS's open-source IAB library (IABLib / iab-validator, `IABConstants.h` and
`IABElements.cpp`), and measurements of Dolby-written track files from 2012 to 2026.


## One file format, two names

Both are SMPTE ST 429-18 track files with the same Data Essence Coding UL
(`060E2B34.04010105.0E090604.00000000`), and each KLV value is one IABitstream frame of ST 2098-2. RDD 57 calls
Dolby Atmos files "legacy" Profile 1 content: a CPL without the IAB Profile metadata, or without "IAB" in its
title, is assumed to be one. So the UL does not tell them apart, and in a DCP an IAB file is meant to be a subset
of ST 2098-2 that Dolby Atmos processors can play.


## Differences

| Feature | IAB, ST 2098-2 (general) | IAB Profile 1, RDD 57 (DCP) | Dolby Atmos (DbyCinema / Dolby files) |
|---|---|---|---|
| Frame preamble | Tag `0x01` + 32-bit length; the length may be 0; the content is out of scope (8.1.3) | No constraint | Always present: `11 01 0E` (`11 01 02` from the older encoders, with IAFrame version 0) + a 19.2 kHz mono 16-bit reference signal; length 3 + 38400 / fps (1603 at 24 fps) |
| Sample rate | 48 or 96 kHz | 48 kHz only (8.1.1) | 48 kHz only |
| Frame rates | Any defined, including 23.976, 96, 100 and 120 | 24, 25, 30, 48, 50, 60 only (8.1.2) | Those, and also 96, 100, 120 (ST 429-18 Table 1) |
| IAFrame Version | Current | Current | 1; 0 in the oldest encoders (2012 to 2013) |
| Beds | Several, simultaneous or conditional, with sub-elements | One bed, 7.1 DS or 9.1OH (6.2, 8.1.6, B1, B7); conditional only with UseCase 0xFF (B4) | One bed, always used, at most 10 channels, the same for the whole programme |
| Bed channels | The whole ChannelID table | L, C, R, Lss, Lrs, Rrs, Rss, Lts, Rts, LFE, at most 10 (B2) | The same 9.1OH set |
| Channel gain | Any prefix and gain | Prefix 0, no ChannelGain (B3) | Unity only |
| BedRemap | Allowed | Not allowed (B5) | Not allowed |
| Channel decorrelation | Allowed | Not allowed (B6) | Not allowed |
| Objects | No fixed limit | At most 118, MetaID at most 118, no sub-elements (O1, O7) | At most 118, sequential MetaIDs |
| MaxRendered | Any | At most 128 (8.1.5) | 128 |
| Object gain | Unity, silence or a gain | Prefix 0 (unity) only, no ObjectGain (O5) | The DbyCinema set allows unity only, but Dolby's files are said to code an inactive object with the **silence** prefix (0x1); none of the 239 files measured does, in the first pan sub-block |
| 9-zone gains | Any gain per zone | 0 or 1 per zone, and only the 11 sets #1a to #6 of 8.5 (O4) | Silence or unity, and the same 11 presets |
| 19-zone definition | Allowed | Not allowed (O3) | Not allowed |
| Object spread | 1D low resolution, 1D, 3D (X, Y, Z) | 1D only, no X, Y or Z (O6) | 1D high resolution only |
| Snap tolerance | Allowed | Not allowed (O2) | Not allowed |
| Object decorrelation | Prefix and coefficients | Prefix 0 or 1, no coefficients (O8) | No decorrelation or maximum |
| Conditional objects | Any use case | UseCase 0xFF only (O9) | "Always use" only |
| AuthoringToolInfo | Allowed | Not allowed (IM2) | Not allowed |
| UserData, unknown data | Allowed, or skipped | Not allowed (IM1, IM3) | Not allowed (ST 429-18 7.2.5 bans UserData too) |
| DLC coding | Any valid coding | Any valid coding | One prediction region, order at most 16, ShiftBits 8 (up to 12 in older encoders), Rice coding in every sub-block with signal, PCM only for all-zero sub-blocks; RiceRemBits up to 22 measured |
| MXF Immersive Audio sub-descriptor | - | All items optional (ST 429-18) | Present (`DolbyAtmosSubDescriptor` in asdcplib), with AtmosID and AtmosVersion |
| MXF Identification set | - | The encoder's own | CompanyName "Dolby Laboratories", Dolby's ProductName and ProductUID |
| CPL title (ISDCF Audio Type) | - | "IAB" (7.1) | "ATMOS", which ISDCF has deprecated in favour of "IAB" |
| CPL ExtensionMetadata | - | "IAB Profile" = `SMPTE-ST-2098-2:2019-P1` (7.2) | None |

So in a DCP the bitstream features that would make IAB different from Dolby Atmos (19 zones, free zone gains, 3D
spread, several beds, 96 kHz) are not allowed. What is left that differs inside the frames is small: Dolby's
preamble, the silence prefix on inactive objects, the 96, 100 and 120 fps rates and version 0 frames, all of which
are outside RDD 57.


## What says who made a file

Nothing in the frames proves who made them. The preamble content is not specified anywhere public and any encoder
can write the same bytes, and the DLC coding and the silence prefix can be imitated too. Only the MXF
Identification set names the maker, and it belongs to the file, not to the frames.

When DCP-o-matic puts immersive audio into a DCP (other than by referencing an existing DCP's track file) it writes
a new track file for each reel (`ReelWriter::write(shared_ptr<const dcp::AtmosFrame>)`): the frames are copied as
they are, but the file gets a new Identification set from libdcp (`MXF::fill_writer_info()`, CompanyName and
ProductName "libdcp"), a new asset ID and a new sub-descriptor with a new AtmosID (FirstFrame, MaxChannelCount,
MaxObjectCount and AtmosVersion are copied). If the DCP is encrypted the frames are encrypted too. The immersive
audio asset is the only one that does not get `mxf_metadata()`, so it does not get the company and product names
from the configuration either.


## immersive_audio_origin()

`immersive_audio_origin()` reads as little as it can of a track file and returns `DOLBY` or `IAB`:

1. It opens the file with `ASDCP::ATMOS::MXFReader` or, failing that, `ASDCP::DCData::MXFReader`, so it also reads
   IAB files without the Dolby Atmos sub-descriptor, which `dcp::AtmosAsset` cannot open. Encrypted files and files
   whose first frame cannot be read are refused.
2. A file is Dolby Atmos when its frames start with Dolby's preamble (PreambleTag `0x01`, the 32-bit length, then
   `11 01 0E`, or `11 01 02` from Dolby's older encoders) and also either its CompanyName starts with "Dolby", or an
   object of its frames has the silence gain prefix (looked for in the first sub-block of every ObjectDefinition),
   or its audio is coded within what Dolby's encoders use (below), both in up to 1000 frames. The last two keep a
   Dolby Atmos file wrapped again by another program, DCP-o-matic included, as Dolby Atmos.

   Dolby's DLC coding, as measured in 239 unencrypted track files (Dolby's encoders from 2012 to 2026, and files
   wrapped again by Doremi, Qube, easyDCP, Clipster and libdcp): DLCSampleRate 48 kHz, ShiftBits 8, 9 or 12, at most
   one predictor region of order at most 16, and PCM sub-blocks only with BitDepth 0 (all zero). RiceRemBits is not
   limited: Dolby's files reach 22. Every AudioDataDLC element read must be within this, and at least one must be
   read. In those 239 files, no object ever has the silence gain prefix, so in practice it is this test that keeps
   such a file Dolby Atmos. It is no proof either: any encoder can code within the same limits.
3. Anything else is IAB, which is also what is assumed when the origin cannot be seen.

Atmos content remembers the result (`AtmosContent::origin()`, saved as `ImmersiveAudioOrigin` in the metadata).
The ISDCF name has `-ATMOS` when all the immersive audio of a film is Dolby's and `-IAB` otherwise, and an Atmos
MXF's summary says `[Atmos]` or `[IAB]`.


## Open questions

- What `11 01 0E` means, and whether Dolby's cinema processors check the preamble or its reference signal. No
  public document says, and a preamble of length 0 meets ST 2098-2 and RDD 57.
- Whether ISDCF's choice of "IAB" for all immersive audio (#2482) should stay, or whether `-ATMOS` is better for
  files known to be Dolby's.
- Whether DCP-o-matic should copy the source's Identification set into the track files it writes, so that the maker
  is not lost.
