ADC trace waveforms
-------------------
The Dante can collect ADC trace waveforms, which is effectively a digital oscilloscope of the pre-amp input signal.
This very useful for setting the AnalogOffset record, and for diagnosing issues with the input.

These are the records to control ADC traces. All of the records except TraceData affect all boards and are in dante.template.
TraceData is specific to each board and is in danteN.template.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - EPICS record names
     - Record types
     - drvInfo string
     - Description
   * - ReadTrace
     - bo
     - DanteReadTrace
     - Arms the system to capture trace data on the next trigger event.
   * - TraceTimeArray
     - waveform
     - DanteTraceTimeArray
     - Waveform record containing the time values for each point in TraceData. 64-bit float data type.
   * - TraceTime, TraceTime_RBV
     - ao, ai
     - DanteTraceTime
     - Time per sample of the ADC trace data in microseconds. Allowed range is 0.016 to 0.512.
   * - TraceLength, TraceLength_RBV
     - longout, longin
     - DanteTraceLength
     - The number of samples to read in the ADC trace.  This must be a multiple of 16384, and will be limited by the 
       NELM field of the TraceData and TraceTimeArray waveform records.
   * - TraceTriggerLevel, TraceTriggerLevel_RBV
     - longout, longin
     - DanteTraceTriggerLevel
     - The trigger level in ADC units (0 to 65535).
   * - TraceTriggerRising, TraceTriggerRising_RBV
     - bo, bi
     - DanteTraceTriggerRising
     - Trigger the ADC trace as it rises through TraceTriggerLevel. Choices are "No" (0) and "Yes" (1).
   * - TraceTriggerFalling, TraceTriggerFalling_RBV
     - bo, bi
     - DanteTraceTriggerFalling
     - Trigger the ADC trace as it fals through TraceTriggerLevel. Choices are "No" (0) and "Yes" (1).
   * - TraceTriggerInstant, TraceTriggerInstant_RBV
     - bo, bi
     - DanteTraceTriggerInstant
     - Trigger the ADC trace even if a rising or falling trigger is not detected. Choices are "No" (0) and "Yes" (1).
   * - TraceTriggerWait, TraceTriggerWait_RBV
     - ao, ai
     - DanteTraceTriggerWait
     - The delay time after the trigger condition is satisfied before beginning the ADC trace.
   * - TraceData
     - waveform
     - DanteTraceData
     - Waveform record containing the ADC trace data. 32-bit integer data type.

The following are the MEDM screen danteTrace.adl displaying two ADC traces. These were done with a Vortex SDD detctor and a Cd109 source,
which produces Ag K x-rays.  The traces were captured with TraceTriggerRising=Yes and TraceTriggerLevel=50000.
The first trace was done with TraceTime=0.512 microseconds, so the total time is 8192 microseconds. 2 resets are visible on this trace. 
The second trace was done with TraceTime=0.016 microseconds, so the total time is 256 microseconds.  The individual 22 keV Ag x-ray steps
can be seen in this trace.


.. figure:: dante_trace1.png
    :align: center

.. figure:: dante_trace2.png
    :align: center

The following is the MEDM screen dante8Trace.adl. This screen is used with the Dante8.

.. figure:: dante8Trace.png
    :align: center
