Performance
-----------

Dante8 free-running mapping mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following table shows the maximum number of pixels/s for MCA mapping mode as a function of the number of boards enabled
and the number of MCA channels on the Dante8. The tests were done under the following conditions:

- MappingPoints = 2000
- PollTime = 0.01
- ArrayCallbacks = Enable
- WaitForPlugins = Yes
- TriggerMode = FreeRunning

The PresetReal time was decreased in 1 ms steps until the mapping mode acquisition no longer collected the requested number of pixels.

The PresetReal time on the Dante is limited to multiples of 1 ms,
so the pixel rate in FreeRun mode is limited to 1000, 500, 333, 250, etc.

.. cssclass:: table-bordered table-striped table-hover
.. list-table:: Maximum pixel rate in Hz (spectra/board/second) for TriggerMode=FreeRunning
   :header-rows: 1
   :widths: auto

   * - MCA Channels
     - 1 board  enabled
     - 2 boards enabled
     - 4 boards enabled
     - 8 boards enabled
   * - 1024
     - 1000
     - 1000
     - 1000
     - 500
   * - 2048
     - 1000
     - 1000
     - 500
     - 333
   * - 4096
     - 1000
     - 1000
     - 1000
     - 1000

Dante8 externally triggered mapping mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following table shows the maximum number of pixels/s for MCA mapping mode as a function of the number of boards enabled
and the number of MCA channels on the Dante8. The tests were done under the following conditions:

- MappingPoints = 2000
- PollTime = 0.01
- ArrayCallbacks = Enable
- WaitForPlugins = Yes
- TriggerMode = Trig Rising
- PresetReal = 0.1 (does not matter)

The Dante8 was triggered by an external programmable pulse generator.  The pulse width was 10 microseconds.
The pulse generator was programmed to output 2000 pulses.

The pulse frequency was increased until the mapping mode acquisition no longer collected the requested number of pixels.

.. cssclass:: table-bordered table-striped table-hover
.. list-table:: Maximum pixel rate in Hz (spectra/board/second) for TriggerMode=Trig Rising
   :header-rows: 1
   :widths: auto

   * - MCA Channels
     - 1 board  enabled
     - 2 boards enabled
     - 4 boards enabled
     - 8 boards enabled
   * - 1024
     - 6200
     - 2300
     - 900
     - 350
   * - 2048
     - 1500
     - 750
     - 340
     - 150
   * - 4096
     - 8060
     - 8060
     - 8060
     - 8060

The same results as above were obtained for TriggerMode=Gate High.  

In 4096 channel mode all spectra are eventually collected for trigger frequencies up to 8000 Hz.
However, in 2048 and 1024 channel mode the maximum trigger frequency is much less before spectra are lost,
and the EPICS IOC needs to be restarted.