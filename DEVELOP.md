# DCP-o-matic development notes

This file collects a few notes relevant to DCP-o-matic developers.  There is also some information
[on the web site](https://dcpomatic.com/development).


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
- Add to `platform/windows/wscript`, `platform/osx/make_dmg.sh`, `cscript`.
- ./waf pot
- cp build/src/lib/libdcpomatic.pot src/lib/po/$LANG.po
- cp build/src/wx/libdcpomatic-wx.pot src/wx/po/$LANG.po
- cp build/src/tools/libdcpomatic-wx.pot src/tools/po/$LANG.po
- sed "s/CHARSET/UTF-8/" src/tools/po/$LANG.po
- Commit / push
- Add credit to `src/wx/about_dialog.cc` and database.
- Add to `i18n.php` on website and `update-i18n-stats` script, then run `update-i18n-stats` script.

