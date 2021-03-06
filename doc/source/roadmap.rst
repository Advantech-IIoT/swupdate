.. SPDX-FileCopyrightText: 2013-2021 Stefano Babic <sbabic@denx.de>
.. SPDX-License-Identifier: GPL-2.0-only

==================
Project's road-map
==================

Please take into account that most of the items here are *proposals*.
I get some ideas talking with customers, some ideas are my own thoughts.
There is no plan when these features will be implemented - this depends
if there will be contribution to the project in terms of patches or
financial contributions to develop a feature.

Thanks again to all companies that have supported my work up now and to
everybody who has contributed to the project, let me bring SWUpdate
to the current status !

Main goal
=========

First goal is to reach a quite big audience, making
SWUpdate suitable for a large number of products.
This will help to build a community around the project
itself.

Core features
=============

Support for further compressors
-------------------------------

SWUpdate supports image compressed with following formats: zlib, zstd. This is
a compromise between compression rate and speed to decompress the single artifact.
To reduce bandwidth or for big images, a stronger compressor could help.
Adding a new compressor must be careful done because it changes the core of
handling an image.

Support for OpenWRT
-------------------

OpenWRT is used on many routers and has its own way for updating that is not power-cut safe.

Selective downloading
---------------------

Bandwidth can be saved not only via delta, but identifying which part of the SWu must be
loaded and skipping the rest. For example, SWUpdate can detect the versions for artifact before
downloading them and ask the servers to send just the relevant artifacts.

Software-Software compatibility
-------------------------------

SWUpdate has from the early stage a hardware to software compatibility check. In case
software is split in several components (like OS and application), it is desirable to have
a sort of software compatibility check. For example, SWUpdate verifies if a component
(like an application) is compatible with a runningOS and reject the update in case of
mismatch.

Parser
======

SWUpdate supports two parsers : libconfig and JSON. It would be nice if tools can
be used to convert from one format to the other one. Currently, due to some specialties
in libconfig, a manual conversion is still required.

Fetcher and interfaces
======================

Downloader
----------

The downloader is a one-shot command: when -d is set, SWUpdate loads the SWU from the provided
URL. This behavior is high requested and must be even supported in future, but another
use case is to run the downloader as daemon (like suricatta) and checks if a new SWU is
available at the specified URL. It should be as an alternative server for suricatta and
this allows to control it via IPC (enable/disable/trigger).

Tools and utilities
===================

Self contained tool to generate Update Packages (SWU)
-----------------------------------------------------

Generation of SWUs is fully supported inside OE via meta-swupdate, but there is no
support at all with other buildsystems (Buildroot, Debian). The user have a not preordered
bunch of programs and scripts to generate the SWU, and mostly they are not generic enough.
It will be interesting to create a `buildswu` tool, running on host system, that can create
form a configuration a SWU. The tool must support all features, that means it should be able
to pack artfact, generate sw-description from templates, sign the SWU, encrypt the artifact,
etc.


Lua
===

- API between SWUpdate and Lua is poorly documented.
- Extend Lua to load modules at startup with functions that are globally visible
  and can be used by own Lua scripts or by the embedded-script in sw-description.
- Store in SWUpdate's repo Lua libraries and common functions to be reused by projects.

Handlers:
=========

New Handlers
------------

Users develop own custom handlers - I just enforce and encourage everyone
to send them and discuss how to integrate custom handler in mainline.

Some ideas for new handlers:
        - FPGA updater for FPGA with Flash
        - Package handler to install packages (ipk, deb)
          Packages can be inserted into the SWU and the atomicity is
          guaranteed by SWUpdate.
        - Lua handlers should be added if possible to the project
          to show how to solve custom install.

Handlers installable as plugin at runtime
------------------------------------------

The project supports Lua as script language for pre- and postinstall
script. It will be easy to add a way for installing a handler at run-time
written in Lua, allowing to expand SWUpdate to the cases not covered
in the design phase of a product.

Of course, this issue is related to the security features: it must be
ensured that only verified handlers can be added to the system to avoid
that malware can get the control of the target.

Current release supports verified images. That means that a handler
written in Lua could be now be part of the compound image, because
a unauthenticated handler cannot run.

Support for BTRFS snapshot
--------------------------

BTRFS supports subvolume and delta backup for volumes - supporting subvolumes is a way
to move the delta approach to filesystems, while SWUpdate should apply the deltas
generated by BTRFS utilities.

Security
========

- add support for asymmetryc decryption

Support for evaluation boards
=============================

meta-swupdate-boards contains examples with evaluation boards.
Currently, there are examples using Beaglebone Black,
Raspberri PI 3 and Wandboard. The repo is a community driven project:
patches welcome.

Back-end support (suricatta mode)
=================================

Back-end: responsiveness for IPC
--------------------------------

Suricatta is implemented as process that launches functions for the selected module.
This means that the IPC does not answer if Suricatta is doing something, specially if it is
downloading and upgrading the system. This can be enhanced adding a separate thread for IPC and of course
all required synchronization with the main modules.

Back-end: check before installing
---------------------------------

In some cases (for example, where bandwidth is important), it is better to check
if an update must be installed instead of installing and performs checks later.
If SWUpdate provides a way to inform a checker if an update can be accepted
before downloading, a download is only done when it is really necessary.

Back-end: hawkBit Offline support
---------------------------------

There are several discussions on hawkBit's ML about how to synchronize
an offline update (done locally or via the internal Web-server) with
the hawkBit's server. Currently, hawkBit thinks to be the only one
deploying software. hawkBit DDI API should be extended, and afterwards
changes must be implemented in SWUpdate.

Back-end: support for generic down-loader 
-----------------------------------------

SWUpdate in down-loader mode works as one-shot: it simply try to download a SWU
from a URL. For simple applications, it could be moved into `suricatta` to detect
if a new version is available before downloading and installing.

Back-end: support for Mender
----------------------------

There was several discussion how to make a stronger collaboration between
different update solution and a proposal discussed previously is to use SWUpdate as client
to upgrade from a Mender server, see `BOF at ELCE 2017 <https://elinux.org/images/0/0c/BoF_secure_ota_linux.pdf>`_

Support for multiple Servers simultaneously
-------------------------------------------

Currently, suricatta's server backends are a mutually exclusive
compile-time choice. There is no interest to have multiple OTA at the same time.
This feature won't be implemented and I will remove this from roadmap if no
interest will be waked up.

Test and Continuous Integration
===============================

The number of configurations and features in SWUpdate is steadily increasing and
it becomes urgent to find a way to test all incoming patch to fix regression issues.
One step in this direction is the support for Travis build - a set of configuration
files is stored with the project and should help to find fast breakages in the build.
More in this direction must be done to perform test on targets. A suitable test framework
should be found. Scope is to have a "SWUpdate factory" where patches are fast integrated
and tested on real hardware.

Documentation
=============

Documentation is a central point in SWUpdate - maintaining it up to date is a must in this project. 
Help from any user fixing wrong sentence, bad english, adding missing topics is high
appreciated.
