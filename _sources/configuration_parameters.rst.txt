Configuration parameters
------------------------
These records control the configuration of the digital signal processing. The readback (_RBV) values may differ slightly
from the output values because of the discrete nature of the system clocks and MCA bins.

These parameters are specific to a single board, and are contained in DanteN.template.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - EPICS record names
     - Record types
     - drvInfo string
     - Description
   * - EnableBoard, EnableBoard_RBV
     - bo, bi
     - DanteEnableBoard
     - Enables (1) or disables (0) a board in a Dante8.  This allows using fewer than 8 channels on a Dante8.
   * - InputMode, InputMode_RBV
     - mbbo, mbbi
     - DanteInputMode
     - The analog input mode. Choices are "DC_HiImp" (0), "DC_LoImp" (1). "AC_Slow" (2), and "AC_Fast" (3).
   * - InputPolarity, InputPolarity_RBV
     - bo, bi
     - DanteInvertedInput
     - The pre-amp output polarity. Choices are "Pos." (0) and "Neg." (1).
   * - MaxEnergy, MaxEnergy_RBV
     - ao, ai
     - DanteMaxEnergy
     - The actual energy of the last channel.  The user must provide this value based on the energy calibration.
       It is used to provide meaningful units for FastThreshold, EnergyThreshold, and BaselineThreshold.
   * - AnalogOffset, AnalogOffset_RBV
     - longout, longin
     - DanteAnalogOffset
     - The analog offset applied to the input signal, 0 to 255. 
       This offset must be adjusted to keep the input signal within the range of the ADC.
       This should be adjusted using the ADC Trace plot with a long sampling to see the range of the input
       signal through a reset event.
   * - ResetThreshold, ResetThreshold_RBV
     - longout, longin
     - DanteResetThreshold
     - The reset threshold in ADC units per N 8 ns sample intervals. The Dante detects a reset the signal changes by more than this amount. 
       The standard firmware uses N=6 and this ResetThreshold value.
       The high-rate firmware uses N=1 and fixes ResetThreshold=256, so this parameter has no effect.
   * - ResetRecoveryTime, ResetRecoveryTime_RBV
     - ao, ai
     - DanteResetRecoveryTime
     - The time in microseconds to wait after a reset event.
   * - Gain, Gain_RBV
     - ao, ai
     - DanteGain
     - The gain which controls the number of ADC units per MCA bin.  Gains of 1.0-8.0 are typical.
   * - FastThreshold, FastThreshold_RBV
     - ao, ai
     - DanteFastFilterThreshold
     - The fast filter threshold in keV.
   * - FastPeakingTime, FastPeakingTime_RBV
     - ao, ai
     - DanteEdgePeakingTime
     - The peaking time of the fast filter in microseconds.
   * - FastFlatTopTime, FastFlatTopTime_RBV
     - ao, ai
     - DanteEdgeFlatTop
     - The flat top time of the fast filter in microseconds.
   * - EnergyThreshold, EnergyThreshold_RBV
     - ao, ai
     - DanteEnergyFilterThreshold
     - The energy filter threshold in keV.
   * - PeakingTime, PeakingTime_RBV
     - ao, ai
     - DantePeakingTime
     - The peaking time of the slow filter in microseconds.
   * - MaxPeakingTime, MaxPeakingTime_RBV
     - ao, ai
     - DanteMaxPeakingTime
     - The maximum peaking time of the slow filter in microseconds. Used only with the high-rate firmware.
       Must be set to 0 when using the standard firmware.
   * - FlatTopTime, FlatTopTime_RBV
     - ao, ai
     - DanteFlatTop
     - The flat top time of the slow filter in microseconds.
   * - BaselineThreshold, BaselineThreshold_RBV
     - ao, ai
     - DanteEnergyBaselineThreshold
     - The baseline filter threshold in keV.
   * - MaxRiseTime, MaxRiseTime_RBV
     - ao, ai
     - DanteMaxRiseTime
     - The maximum rise time in usec. Pulses with a longer rise time will be pileup rejected.
   * - ZeroPeakFreq, ZeroPeakFreq_RBV
     - ao, ai
     - DanteZeroPeakFreq
     - The frequency of the zero-energy peak in Hz.
   * - BaselineSamples, BaselineSamples_RBV
     - longout, longin
     - DanteBaselineSamples
     - The number of baseline samples.  Typical value is 64.
   * - TimeConstant, TimeConstant_RBV
     - ao, ai
     - DanteTimeConstant
     - The time constant. Used for digital deconvolution in the case of continuous reset signals.
   * - BaseOffset, BaseOffset_RBV
     - longout, longin
     - DanteBaseOffset
     - The base offset. Used for digital deconvolution in the case of continuous reset signals.

