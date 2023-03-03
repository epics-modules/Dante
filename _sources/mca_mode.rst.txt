MCA mode
--------
The MCA mode collects a single MCA record at a time.  It is compatible with the MCA record, and is the same
as MCA operation on many other EPICS MCAs, e.g. Canberra AIM, Amptek, XIA (Saturn, Mercury, xMAP, FalconX), SIS38XX, and others.

It only supports counting for a preset real time, or counting indefinitely (PresetReal=0).
It does not support PresetLive or PresetCounts which some other MCAs do.

The following is the MEDM screen mca.adl displaying the MCA spectrum as it is acquiring.

.. figure:: dante_mca.png
    :align: center

The following is the IDL MCA Display program showing the MCA spectrum as it is acquiring. This GUI allows defining ROIs
graphically, fitting peaks and background, and many other features.

.. figure:: dante_idl_mca.png
    :align: center

