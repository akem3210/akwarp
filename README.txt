AKWARP(1)
=========
Mathias Gumz <akira@fluxbox.org>
v1.0rc1, 07 Oct 2005

NAME
----
akwarp - switches workspaces when the mouse enters certain areas.

SYNOPSIS
--------
'akwarp' [-h] [-v] [-pws <geometry>] [-nws <geometry>] [-t <timeout>]

DESCRIPTION
-----------
if the mousepointer enters one of either nextworkspace-area or
prevworkspacearea and stay there for longer than a given 
activation_time, akwarp will change to the next/prev workspace,
using ewmh_standards... which means you need an ewmh-compatible
windowmanager (eg fluxbox)

mouseposition is checked at ~ 25fps, that should be good enough

BUILD and INSTALL
-----------------
to build 'akwarp' just enter

    $> gmake

if you want shading support for 'akwarp' enter

    $> gmake xrender

if you want use some shape-file for the buttons:

    $> gmake xshape

and for a combination of shaded and shaped buttons:

    $> gmake xrender-xshape

to install 'akwarp' enter:

    $> gmake install

OPTIONS
-------
-h :: 
  displays help
-v ::
  displays version
-pws <geometry> ::
  define area for "previous workspace",
  default is 10xheight+0+0
-nws <geometry> ::
  define area for "next workspace",
  default is 10xheight-10+0
-b ::
  show buttons
-c <color> ::
  color the buttons with <color>,
  default is "white"
-s <int> ::
  shade the buttons,
  default is 0 aka no shading, only available if compiled
  with shade-support.
-t <time> ::
  define timeout as float
-spws <file> ::
  use given <file> in bitmap-format for 'prev' button,
  only available if compiled with shape-support.
-snew <file> ::
  use given <file> in bitmap-format for 'next' button,
  only available if compiled with shape-support.
           
see XParseGeometry(3) for details about <geometry>

EXAMPLES
--------
Use bottomleft.xbm and bottomright.xbm as shapefiles, shade them 50% red,
put them into the lower left and lower right corner of the screen and
wait until the mouse was at least 5 seconds above the regions specified:

    $> akwarp -b -c red -s 50 -spws bottomleft.xbm -pws 32x32+0-32 -snws bottomright.xbm -nws 32x32-32-32 -t 5

USED SOFTWARE
-------------
fluxbox, gvim, gcc, xorg-x11, gmake, asciidoc, bitmap

AUTHOR
------
* Written by Mathias Gumz <akira at fluxbox dot org>
* Thanx to Mark Tiefenbruck <pythagosaurus at gmail dot com> for little
  cleanup.

COPYING
-------
Copyright (C) 2005 Mathias Gumz. Free use of this software is
granted under the terms of the MIT. See LICENSE provided in the
distribution.
