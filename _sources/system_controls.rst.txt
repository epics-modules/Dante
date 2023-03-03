System controls
---------------
These records are in the file ``dante.template``. This database is loaded once for the Dante system.  It provides
control of the system-wide settings for the system.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - EPICS record names
     - Record types
     - drvInfo string
     - Description
   * - CollectMode, CollectMode_RBV
     - mbbo, mbbi
     - DanteCollectMode
     - Controls the data collection mode.
       Choices are "MCA" (0), "MCA Mapping" (1) and "List" (2).
   * - GatingMode, GatingMode_RBV
     - mbbo, mbbi
     - DanteGatingMode
     - Controls the gating mode.
       Choices are "Free running" (0), "Trig rising" (1), "Trig falling" (2), "Trig both" (3), "Gate high" (4), "Gate low" (5).
   * - NumMCAChannels, NumMCAChannels_RBV
     - mbbo, mbbi
     - MCA_NUM_CHANNELS
     - The number of MCA channels to use.  Choices are 1024, 2048, 4096.
   * - PollTime, PollTime_RBV
     - ao, ai
     - DantePollTime
     - The time between polls when reading completion status, MCA mapping data, and list mode data from the driver.
       0.01 second is a reasonable value that will provide good response and resource utilization.
   * - PresetReal
     - ao
     - MCA_PRESET_REAL
     - Sets the preset real time.  Set this to 0 to count forever in MCA mode or List mode.
   * - EraseStart
     - bo
     - N.A.
     - Processing this record starts acquisition for all boards in the selected CollectMode.
   * - StartAll
     - bo
     - MCA_START_ACQUIRE
     - Processing this record starts acquisition for all boards in the selected CollectMode. This record should not
       be used by higher-level software, it is processed by EraseStart.
   * - MCAAcquireBusy
     - busy
     - N.A.
     - This record goes to 1 ("Collecting") when EraseStart is processed. It goes back to 0 ("Done")when 3 conditions
       are satisfied. 1) MCAAcquiring is 0; 2) All MCA records have .ACQG field=0; 3)AcquireBusy from areaDetector=0.
       The last condition can ensure that all plugins are done processing if WaitForPlugins is set.
   * - MCAAcquiring
     - bi
     - MCA_ACQUIRING
     - This record is 1 when the Dante driver itself is acquiring, and 0 when it is done. This record is generally not used
       by higher level software, use MCAAcquireBusy instead, since it indicates when all components are done.
   * - StopAll
     - bo
     - MCA_STOP_ACQUIRE
     - Processing this record stops acquisition for all boards in the selected CollectMode. This only needs to be used
       to terminate acquisition before it would otherwise stop because PresetReal or NumMappingPoints have been reached.
   * - ReadAll
     - bo
     - N.A.
     - Processing this record reads the MCA data and statistics for all boards.  This .SCAN field of this record is typically
       set to periodic, i.e. "1 second", ".1 second", etc. to provide user feedback while acquisition is in progress.
       It can be set to "Passive" and the system will still read the data once when acquisition completes. 
       This can be used to improve performance at very short PresetReal times. 
       This record is disabled when acquisition is complete to reduce unneeded resource usage.
   * - ReadAllOnce
     - bo
     - N.A.
     - Processing this record reads the MCA data and statistics for all boards.  This record is processed by ReadAll. It can be
       manually processed to read the data even when acquisition is complete.
   * - ElapsedReal
     - ai
     - MCA_ELAPSED_REAL
     - The elapsed real time. 
   * - ElapsedLive
     - ai
     - MCA_ELAPSED_LIVE
     - The elapsed live time.
   * - DeadTime
     - ai
     - DanteDeadTime
     - The cummulative deadtime.
   * - IDeadTime
     - ai
     - DanteIDeadTime
     - The "instantaneous" deadtime since the previous readout.
          
