.. _dante:             https://github.com/epics-modules/dante
.. _mca:               https://github.com/epics-modules/mca
.. _asyn:              https://github.com/epics-modules/asyn
.. _asynNDArrayDriver: https://areadetector.github.io/master/ADCore/NDArray.html#asynndarraydriver
.. _areaDetector:      https://areadetector.github.io
.. _XGLab:             https://www.xglab.it
.. _read_nd_hdf5:      https://github.com/CARS-UChicago/IDL_Detectors/blob/master/read_nd_hdf5.pro

Overview
--------

This is an EPICS driver for the XGLab_ Dante digital x-ray spectroscopy system.
The source code is in the dante_ repository in the Github epics-modules project.
The Dante is available in single channel (Dante1) and 8-channel (Dante8) versions.
This module is intended to work with either, though it has currently only been tested on the single-channel version.
In this document NumBoards refers to the number of enabled input channels, e.g. 1 for a Dante1, up to 8 for a 
Dante8, and >8 for systems with more than one Dante8 daisy-chained together.  
If a channel is disabled then it is not counted in NumBoards.

The Dante can collect data in 3 different modes:

- Single MCA spectrum.  It acquires a single MCA spectrum on all channels.
- MCA mapping mode.  It acquires multiple spectra in rapid succession, and it often used for making an x-ray map where there is an MCA
  spectrum for each channel at each pixel.  The advance to the next pixel can come from an internal clock or an external trigger.
- List mapping mode.  It acquires each x-ray event energy and timestamp in a list buffer.

The Dante driver is derived from the base class asynNDArrayDriver_, which is part of the EPICS areaDetector_ package.
The allows the Dante driver to use all of the areaDetector plugins for file saving in MCA mapping and list modes,
and for other purposes. It also implements the mca interface from the EPICS mca_ module.
The EPICS mca record can be used to display the spectra and control the basic operation including Regions-of-Interest (ROIs).

The Dante driver can be used on both Windows and Linux. A Windows machine with a USB interface is required
to load new firmware.  Otherwise the module can be used from either Linux or Windows over Ethernet. The Linux library
provided can run on most Linux versions, including RHEL7/Centos7.

This document does not attempt to give an explanation of the principles of operation of the Dante, or a detailed explanation
of the many configuration parameters for the digital pulse processing.  The user should consult the
:download:`DanteManual <DANTE-4553 Manual rev2.3.pdf>` for this information.
