# Dante Releases

The versions of EPICS base, asyn, mca, and other synApps modules used for each release can be obtained from 
the configure/RELEASE file in each release of Dante.

## R1-1 (November 18, 2021)
- Adds support for the Dante8, the 8-channel version of the Dante.
- Based on version 3.7.19 of the XGL_DPP library.
- Requires firmware version 4.1.x
- Adds a KeepAlive record required to keep the socket connected to the Dante
  from closing due to inactivity in firmare 4.1.x.
- Changed documentation to rst, moved to https://epics-dante.readthedocs.io.

## R1-0 (April 9, 2021)
- Initial release.  This has only been tested with a 1-channel Dante, and needs work for the 8-channel Dante.
- Based on version 3.3.2 of the XGL_DPP library.
- Requires firmware version 3.3.2 (standard) or 3.3.5 (fast).
