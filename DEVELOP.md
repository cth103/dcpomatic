# DCP-o-matic development notes

This file collects a few notes relevant to DCP-o-matic developers.  There is also some information
[on the web site](https://dcpomatic.com/development).


## Building on macOS/arm64

Build `osx-environment` in `$HOME`
```
bash platform/osx/copy_resources.sh
source platform/osx/set_paths.sh
./waf configure --target-macos-arm64
```


## Disk writer logging

As we have no `film' folder to log to during disk writes, the logs end up:

### macOS

* Disk writer backend: `/var/log/dcpomatic_disk_writer_{out,err}.log`
* Disk writer frontend: `/Users/$USER/Library/Preferences/com.dcpomatic/2/2.16/disk.log`

### Windows

* Disk writer backend: `c:\Users\$USER\AppData\Local\dcpomatic\2.16.0\disk_writer.log`
* Disk writer frontend: `c:\Users\$USER\AppData\Local\dcpomatic\2.16.0\disk.log`

### Linux

* Disk writer backend: `/home/$USER/.config/dcpomatic/2.16.0/disk_writer.log`
* Disk writer frontend: `/home/$USER/.config/dcpomatic/2.16.0/disk.log`


## Branches

The `test/data` submodule has the following branches:

* `v2.16.x` - branch for use with v2.16.x versions
* `v2.18.x` - branch for use with v2.18.x versions


## Player stress testing

If you configure DCP-o-matic with `--enable-player-stress-test` you can make a script which
will run and manipulate the player in predictable ways.  The script is a series of commands
read line-by-line, and each line can be one of:

* `O <path>`

Open a DCP, for example

```O /home/carl/DCP/MyTestDCP```

* `P`

Start playing the currently-loaded DCP.

* `W <time-in-milliseconds>`

Wait for approximately the given time before carrying on, for example

```W 14000```

to wait for 14 seconds.

* `S`

Stop any current playback.

* `K <position>`

Seek to some point in the current DCP, where 0 is the start and 4095 is the end; for example

```K 2048```

seeks half-way through the DCP.

* `E`

Stops playback and closes the player.

The script can be run using something like

```dcpomatic2_player -s stress```

to load a script file called `stress` and start executing it.


## Adding a new language

- Edit `src/wx/config_dialog.cc` to add the language to languages.
- Add to `platform/windows/wscript`, `platform/osx/make_dmg.sh`, `cscript.py`.
- ./waf pot
- cp build/src/lib/libdcpomatic.pot src/lib/po/$LANG.po
- cp build/src/wx/libdcpomatic-wx.pot src/wx/po/$LANG.po
- cp build/src/tools/dcpomatic.pot src/tools/po/$LANG.po
- sed -i "s/CHARSET/UTF-8/" src/{lib,wx,tools}/po/$LANG.po
- Commit / push
- Add credit to `src/wx/about_dialog.cc` and database.
- Add to `i18n.php` on website and `update-i18n-stats` script, then run `update-i18n-stats` script.


## Taking screenshots for the manual

The manual PDF looks nice if vector screenshots are used.  These can be taken as follows:

- Build `gtk-vector-screenshot.git` (using meson/ninja)
- Copy `libgtk-vector-screenshot.so` to `/usr/local/lib/gtk-3.0/modules/`
- Run DCP-o-matic using `run/dcpomatic --screenshot`
- Start `take-vector-screenshot`, click "Take screenshot" then click on the DCP-o-matic window.
- Find a PDF in `/tmp/dcpomatic2.pdf`
- Copy this to `doc/manual/raw-screenshots` 


## Adding a new variant

Files to edit:
- `cscript.py`
- `wscript`
- `src/lib/variant.cc`
- `src/tools/wscript`
- `platform/osx/make_dmg.sh`
- `platform/windows/wscript`
- `platform/osx/wscript`
