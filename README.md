# Thalia Gameboy Emulator

Thalia is a Gameboy emulator written in C using GLib/GObject 2.0. The graphical
user interface makes use of [GTK+](http://www.gtk.org/) 2.0 and uses
[Cairo](http://www.cairographics.org/) for drawing. Thalia emulates the
[classic gameboy](http://en.wikipedia.org/wiki/Game_Boy) (DMG). The project is
focused on emulator accuracy and readability of code, much less than it is on
attaining speeds much faster than the machine itself.

For licensing information, please see the LICENSE file.

### Current features

* Graphics emulation (background, window and sprites).
* Correct CPU emulation as tested by Blargg's
 [cpu_instrs.gb](http://slack.net/~ant/old/gb-tests/).
* Timer emulation and interrupts (vblank, lcd, timer).
* ROM bank switching (MBC1).

### To be implemented

* Sound emulation.
* Color support (CGB).
* RAM bank switching (MBC1).
* More memory bank controller types.
* Serial I/O (link cable) support.
* Emulation slowdown to match machine speed.

## Instructions

Install [SCons](http://www.scons.org/), for example on Debian/Ubuntu:

    sudo apt-get install scons

You're need the development headers for GDK 2.0, GTK 2.0, GLib 2.0 and Cairo.
On Ubuntu, these are all installed by typing:

    sudo apt-get install libgtk2.0-dev

Run SCons to build the source code:

    scons

Use the executable with the ROM file as argument, for instance:

    ./thalia tests/ttt.gb
