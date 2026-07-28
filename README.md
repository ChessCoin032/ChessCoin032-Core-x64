[![ChessCoin](https://i.ibb.co/WvmBzvJB/chesscoin.png)](https://bitcointalk.org/index.php?topic=5275402.0)


ChessCoin 0.32% development tree

ChessCoin 0.32% is a PoS-based cryptocurrency.


Development process
===========================

Developers work in their own trees, then submit pull requests when
they think their feature or bug fix is ready.

The patch will be accepted if there is broad consensus that it is a
good thing.  Developers should expect to rework and resubmit patches
if they don't match the project's coding conventions (see coding.txt)
or are controversial.

The master branch is regularly built and tested, but is not guaranteed
to be completely stable. Tags are regularly created to indicate new
stable release versions of ChessCoin 0.32%.

Feature branches are created when there are major new features being
worked on by several people.

From time to time a pull request will become outdated. If this occurs, and
the pull is no longer automatically mergeable; a comment on the pull will
be used to issue a warning of closure. The pull will be closed 15 days
after the warning if action is not taken by the author. Pull requests closed
in this manner will have their corresponding issue labeled 'stagnant'.

Issues with no commits will be given a similar warning, and closed after
15 days from their last activity. Issues closed in this manner will be 
labeled 'stale'.

How to install the dependencies on Ubuntu
======================================================
sudo apt install libxcb1-dev libxcb-glx0-dev libxcb-icccm4-dev \
  libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-render0-dev libxcb-render-util0-dev libxcb-shape0-dev \
  libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev \
  libxcb-xinerama0-dev libxcb-xkb-dev libxcb-xinput-dev \
  libxkbcommon-dev libxkbcommon-x11-dev \
  libx11-dev libx11-xcb-dev libxext-dev libxrender-dev \
  libfontconfig1-dev libfreetype-dev \
  libpcre2-16-0

How to build ChessCoin GUI application on Windows
======================================================

See : `doc/build-window.txt`.


How to build ChessCoin GUI application on Mac
======================================================

See : `doc/build-mac.txt`.


How to build ChessCoin GUI application on Ubuntu
======================================================

See : `doc/build-ubuntu.txt`.
