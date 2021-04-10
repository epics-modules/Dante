program danteMED("P=Dante:,DANTE=dante,MCA=mca,N_DETECTORS=8")

/*  This sequencer program works with danterMultiChannel.template and dante.template. 
 *  It supports Copying PVs from the first channel to all of the channels
 *  and multi-element MCA operations
 *
 *  Mark Rivers
 *  April 2, 2021
 *
 * This program must be compiled with the recursive option so that multiple copies
 * can run at once */
option +r;

/* Allow the program to start, even if not all assigned channels are connected.
 * This could be useful if danteMED is running on a different IOC. */
option -c;

%%#include <stdlib.h>
%%#include <errlog.h>
%%#include <string.h>

/* Maximum number of detectors supported */
#define MAX_DETECTORS 100
/* Maximum number of ROIs per MCA record */
#define MAX_ROIS 32
/* TOTAL_ROIS must be defined as MAX_DETECTORS * MAX_ROIS.
   It can't be done in SNL because of syntax limitations. */
#define TOTAL_ROIS 3200

int i;
int j;
int k;
int m;
int chan;
int det;
int nDetectors;
int numEnabled;
int allDone;
char temp[100];
double energy;
double slope;

char *prefix;
char *dante;
char *mca;

int     EnableBoard[MAX_DETECTORS];
assign  EnableBoard to {};

double  FastPeakingTime[MAX_DETECTORS];
assign  FastPeakingTime to {};
int     CopyFastPeakingTime; assign CopyFastPeakingTime to "{P}{DANTE}:CopyFastPeakingTime";
monitor CopyFastPeakingTime; evflag CopyFastPeakingTimeMon; sync CopyFastPeakingTime CopyFastPeakingTimeMon;

double  FastThreshold[MAX_DETECTORS]; 
assign  FastThreshold to {};
int     CopyFastThreshold; assign CopyFastThreshold to "{P}{DANTE}:CopyFastThreshold";
monitor CopyFastThreshold; evflag CopyFastThresholdMon; sync CopyFastThreshold CopyFastThresholdMon;

double  FastFlatTopTime[MAX_DETECTORS];
assign  FastFlatTopTime to {};
int     CopyFastFlatTopTime; assign CopyFastFlatTopTime to "{P}{DANTE}:CopyFastFlatTopTime";
monitor CopyFastFlatTopTime; evflag CopyFastFlatTopTimeMon; sync CopyFastFlatTopTime CopyFastFlatTopTimeMon;

double  PeakingTime[MAX_DETECTORS]; 
assign  PeakingTime to {};
int     CopyPeakingTime; assign CopyPeakingTime to "{P}{DANTE}:CopyPeakingTime";
monitor CopyPeakingTime; evflag CopyPeakingTimeMon; sync CopyPeakingTime CopyPeakingTimeMon;

double  MaxPeakingTime[MAX_DETECTORS]; 
assign  MaxPeakingTime to {};
int     CopyMaxPeakingTime; assign CopyMaxPeakingTime to "{P}{DANTE}:CopyMaxPeakingTime";
monitor CopyMaxPeakingTime; evflag CopyMaxPeakingTimeMon; sync CopyMaxPeakingTime CopyMaxPeakingTimeMon;

double  FlatTopTime[MAX_DETECTORS]; 
assign  FlatTopTime to {};
int     CopyFlatTopTime; assign CopyFlatTopTime to "{P}{DANTE}:CopyFlatTopTime";
monitor CopyFlatTopTime; evflag CopyFlatTopTimeMon; sync CopyFlatTopTime CopyFlatTopTimeMon;

double  EnergyThreshold[MAX_DETECTORS]; 
assign  EnergyThreshold to {};
int     CopyEnergyThreshold; assign CopyEnergyThreshold to "{P}{DANTE}:CopyEnergyThreshold";
monitor CopyEnergyThreshold; evflag CopyEnergyThresholdMon; sync CopyEnergyThreshold CopyEnergyThresholdMon;

double  BaselineThreshold[MAX_DETECTORS]; 
assign  BaselineThreshold to {};
int     CopyBaselineThreshold; assign CopyBaselineThreshold to "{P}{DANTE}:CopyBaselineThreshold";
monitor CopyBaselineThreshold; evflag CopyBaselineThresholdMon; sync CopyBaselineThreshold CopyBaselineThresholdMon;

double  MaxRiseTime[MAX_DETECTORS]; 
assign  MaxRiseTime to {};
int     CopyMaxRiseTime; assign CopyMaxRiseTime to "{P}{DANTE}:CopyMaxRiseTime";
monitor CopyMaxRiseTime; evflag CopyMaxRiseTimeMon; sync CopyMaxRiseTime CopyMaxRiseTimeMon;

double  ResetRecoveryTime[MAX_DETECTORS]; 
assign  ResetRecoveryTime to {};
int     CopyResetRecoveryTime; assign CopyResetRecoveryTime to "{P}{DANTE}:CopyResetRecoveryTime";
monitor CopyResetRecoveryTime; evflag CopyResetRecoveryTimeMon; sync CopyResetRecoveryTime CopyResetRecoveryTimeMon;

double  ZeroPeakFreq[MAX_DETECTORS]; 
assign  ZeroPeakFreq to {};
int     CopyZeroPeakFreq; assign CopyZeroPeakFreq to "{P}{DANTE}:CopyZeroPeakFreq";
monitor CopyZeroPeakFreq; evflag CopyZeroPeakFreqMon; sync CopyZeroPeakFreq CopyZeroPeakFreqMon;

double  BaselineSamples[MAX_DETECTORS]; 
assign  BaselineSamples to {};
int     CopyBaselineSamples; assign CopyBaselineSamples to "{P}{DANTE}:CopyBaselineSamples";
monitor CopyBaselineSamples; evflag CopyBaselineSamplesMon; sync CopyBaselineSamples CopyBaselineSamplesMon;

double  Gain[MAX_DETECTORS]; 
assign  Gain to {};
int     CopyGain; assign CopyGain to "{P}{DANTE}:CopyGain";
monitor CopyGain; evflag CopyGainMon; sync CopyGain CopyGainMon;

double  InputPolarity[MAX_DETECTORS]; 
assign  InputPolarity to {};
int     CopyInputPolarity; assign CopyInputPolarity to "{P}{DANTE}:CopyInputPolarity";
monitor CopyInputPolarity; evflag CopyInputPolarityMon; sync CopyInputPolarity CopyInputPolarityMon;

double  AnalogOffset[MAX_DETECTORS]; 
assign  AnalogOffset to {};
int     CopyAnalogOffset; assign CopyAnalogOffset to "{P}{DANTE}:CopyAnalogOffset";
monitor CopyAnalogOffset; evflag CopyAnalogOffsetMon; sync CopyAnalogOffset CopyAnalogOffsetMon;

double  MaxEnergy[MAX_DETECTORS]; 
assign  MaxEnergy to {};
int     CopyMaxEnergy; assign CopyMaxEnergy to "{P}{DANTE}:CopyMaxEnergy";
monitor CopyMaxEnergy; evflag CopyMaxEnergyMon; sync CopyMaxEnergy CopyMaxEnergyMon;

double  ResetThreshold[MAX_DETECTORS]; 
assign  ResetThreshold to {};
int     CopyResetThreshold; assign CopyResetThreshold to "{P}{DANTE}:CopyResetThreshold";
monitor CopyResetThreshold; evflag CopyResetThresholdMon; sync CopyResetThreshold CopyResetThresholdMon;

double  InputMode[MAX_DETECTORS]; 
assign  InputMode to {};
int     CopyInputMode; assign CopyInputMode to "{P}{DANTE}:CopyInputMode";
monitor CopyInputMode; evflag CopyInputModeMon; sync CopyInputMode CopyInputModeMon;

double  TimeConstant[MAX_DETECTORS]; 
assign  TimeConstant to {};
int     CopyTimeConstant; assign CopyTimeConstant to "{P}{DANTE}:CopyTimeConstant";
monitor CopyTimeConstant; evflag CopyTimeConstantMon; sync CopyTimeConstant CopyTimeConstantMon;

double  BaseOffset[MAX_DETECTORS]; 
assign  BaseOffset to {};
int     CopyBaseOffset; assign CopyBaseOffset to "{P}{DANTE}:CopyBaseOffset";
monitor CopyBaseOffset; evflag CopyBaseOffsetMon; sync CopyBaseOffset CopyBaseOffsetMon;

int     ReadMCA[MAX_DETECTORS]; 
assign  ReadMCA to {};
int     ReadMCAs; assign ReadMCAs to "{P}{DANTE}:DoReadAll";
monitor ReadMCAs; evflag ReadMCAsMon; sync ReadMCAs ReadMCAsMon;

int     statusMCA[MAX_DETECTORS];
assign  statusMCA to {};
/*
int     StatusMCAs; assign StatusMCAs to "{P}{DANTE}:DoStatusAll";
monitor StatusMCAs; evflag StatusMCAsMon; sync StatusMCAs StatusMCAsMon;
*/

int     MCAAcquiring; assign MCAAcquiring to "{P}{DANTE}:MCAAcquiring";
monitor MCAAcquiring; evflag MCAAcquiringMon; sync MCAAcquiring MCAAcquiringMon;

double  mcaPresetReal[MAX_DETECTORS]; 
assign  mcaPresetReal to {};
double  PresetReal; assign PresetReal to "{P}{DANTE}:PresetReal";
monitor PresetReal; evflag PresetRealMon; sync PresetReal PresetRealMon;

double  mcaDeadTime[MAX_DETECTORS]; 
assign  mcaDeadTime to {};
double  DeadTime; assign DeadTime to "{P}{DANTE}:DeadTime";
evflag  DeadTimeMon; sync mcaDeadTime DeadTimeMon;

double  mcaIDeadTime[MAX_DETECTORS]; 
assign  mcaIDeadTime to {};
double  IDeadTime; assign IDeadTime to "{P}{DANTE}:IDeadTime";
evflag  IDeadTimeMon; sync mcaIDeadTime IDeadTimeMon;

int     mcaNumChans[MAX_DETECTORS]; 
assign  mcaNumChans to {};

double  mcaCalOffset[MAX_DETECTORS]; 
assign  mcaCalOffset to {};

double  mcaCalSlope[MAX_DETECTORS]; 
assign  mcaCalSlope to {};

int     mcaROILow[TOTAL_ROIS]; 
assign  mcaROILow to {};

int    mcaROIHigh[TOTAL_ROIS]; 
assign mcaROIHigh to {};

string mcaROIName[TOTAL_ROIS]; 
assign mcaROIName to {};

int     CopyROIChannel; assign CopyROIChannel     to "{P}{DANTE}:CopyROIChannel";
monitor CopyROIChannel; evflag CopyROIChannelMon; sync CopyROIChannel CopyROIChannelMon;
int     CopyROIEnergy;  assign CopyROIEnergy      to "{P}{DANTE}:CopyROIEnergy";
monitor CopyROIEnergy;  evflag CopyROIEnergyMon;  sync CopyROIEnergy CopyROIEnergyMon;
int     SNL_Connected;  assign SNL_Connected      to "{P}{DANTE}:SNL_Connected";
int     AsynDebug;      assign AsynDebug          to "{P}{DANTE}:AsynIO.TB1";
monitor AsynDebug;

int MCAAcquireBusy; assign MCAAcquireBusy to "{P}{DANTE}:MCAAcquireBusy";

ss dante_control {
    state init {
        when() {
            /* We set the following event flags to force these modes and
               labels to be set correctly initially */
            prefix = macValueGet("P");
            dante = macValueGet("DANTE");
            mca = macValueGet("MCA");
            nDetectors = atoi(macValueGet("N_DETECTORS"));
            if ((nDetectors <= 0) || (nDetectors > MAX_DETECTORS)) {
                printf ("N_DETECTORS is illegal.\n");
                nDetectors = 0;
            }
            if (TOTAL_ROIS != (MAX_DETECTORS * MAX_ROIS)) {
                printf ("N_DETECTORS, MAX_ROIS and TOTAL_ROIS are inconsistent.\n");
                nDetectors = 0;
            }
            for (i=0; i<nDetectors; i++) {
                det = i+1;
                sprintf(temp, "%s%s%d:EnableBoard", prefix, dante, det);
                pvAssign(EnableBoard[i], temp);
                pvMonitor(EnableBoard[i]);

                sprintf(temp, "%s%s%d:FastPeakingTime", prefix, dante, det);
                pvAssign(FastPeakingTime[i], temp);

                sprintf(temp, "%s%s%d:FastThreshold", prefix, dante, det);
                pvAssign(FastThreshold[i], temp);

                sprintf(temp, "%s%s%d:FastFlatTopTime", prefix, dante, det);
                pvAssign(FastFlatTopTime[i], temp);

                sprintf(temp, "%s%s%d:PeakingTime", prefix, dante, det);
                pvAssign(PeakingTime[i], temp);

                sprintf(temp, "%s%s%d:MaxPeakingTime", prefix, dante, det);
                pvAssign(MaxPeakingTime[i], temp);

                sprintf(temp, "%s%s%d:FlatTopTime", prefix, dante, det);
                pvAssign(FlatTopTime[i], temp);

                sprintf(temp, "%s%s%d:EnergyThreshold", prefix, dante, det);
                pvAssign(EnergyThreshold[i], temp);

                sprintf(temp, "%s%s%d:BaselineThreshold", prefix, dante, det);
                pvAssign(BaselineThreshold[i], temp);

                sprintf(temp, "%s%s%d:MaxRiseTime", prefix, dante, det);
                pvAssign(MaxRiseTime[i], temp);

                sprintf(temp, "%s%s%d:ResetRecoveryTime", prefix, dante, det);
                pvAssign(ResetRecoveryTime[i], temp);

                sprintf(temp, "%s%s%d:ZeroPeakFreq", prefix, dante, det);
                pvAssign(ZeroPeakFreq[i], temp);

                sprintf(temp, "%s%s%d:BaselineSamples", prefix, dante, det);
                pvAssign(BaselineSamples[i], temp);

                sprintf(temp, "%s%s%d:Gain", prefix, dante, det);
                pvAssign(Gain[i], temp);

                sprintf(temp, "%s%s%d:InputPolarity", prefix, dante, det);
                pvAssign(InputPolarity[i], temp);

                sprintf(temp, "%s%s%d:AnalogOffset", prefix, dante, det);
                pvAssign(AnalogOffset[i], temp);

                sprintf(temp, "%s%s%d:MaxEnergy", prefix, dante, det);
                pvAssign(MaxEnergy[i], temp);

                sprintf(temp, "%s%s%d:ResetThreshold", prefix, dante, det);
                pvAssign(ResetThreshold[i], temp);

                sprintf(temp, "%s%s%d:InputMode", prefix, dante, det);
                pvAssign(InputMode[i], temp);

                sprintf(temp, "%s%s%d:TimeConstant", prefix, dante, det);
                pvAssign(TimeConstant[i], temp);

                sprintf(temp, "%s%s%d:BaseOffset", prefix, dante, det);
                pvAssign(BaseOffset[i], temp);

                sprintf(temp, "%s%s%d.READ", prefix, mca, det);
                pvAssign(ReadMCA[i], temp);

                sprintf(temp, "%s%s%d.PROC", prefix, mca, det);
                pvAssign(statusMCA[i], temp);

                sprintf(temp, "%s%s%d.PRTM", prefix, mca, det);
                pvAssign(mcaPresetReal[i], temp);

                sprintf(temp, "%s%s%d.DTIM", prefix, mca, det);
                pvAssign(mcaDeadTime[i], temp);
                pvMonitor(mcaDeadTime[i]);

                sprintf(temp, "%s%s%d.IDTIM", prefix, mca, det);
                pvAssign(mcaIDeadTime[i], temp);
                pvMonitor(mcaIDeadTime[i]);

                sprintf(temp, "%s%s%d.CALO", prefix, mca, det);
                pvAssign(mcaCalOffset[i], temp);
                pvMonitor(mcaCalOffset[i]);

                sprintf(temp, "%s%s%d.CALS", prefix, mca, det);
                pvAssign(mcaCalSlope[i], temp);
                pvMonitor(mcaCalSlope[i]);

                sprintf(temp, "%s%s%d.NUSE", prefix, mca, det);
                pvAssign(mcaNumChans[i], temp);
                pvMonitor(mcaNumChans[i]);

                for (j=0; j<MAX_ROIS; j++) {
                    k = i*MAX_ROIS + j;

                    sprintf(temp, "%s%s%d.R%dLO", prefix, mca, det, j);
                    pvAssign(mcaROILow[k], temp);

                    sprintf(temp, "%s%s%d.R%dHI", prefix, mca, det, j);
                    pvAssign(mcaROIHigh[k], temp);

                    sprintf(temp, "%s%s%d.R%dNM", prefix, mca, det, j);
                    pvAssign(mcaROIName[k], temp);
                }
           }
        } state waitConnected
    }
    state waitConnected {

        when (nDetectors <= 0) {
          printf (">>>>>>>>>>>>>> danteMED is dead. <<<<<<<<<<<<<<\n");
        } state dead

        when (pvAssignCount () == pvConnectCount ()) {
          printf ("danteMED: All channels connected.\n");
          SNL_Connected = 1;
          pvPut(SNL_Connected);
        } state monitor_changes

    }
    state dead {
      when (delay (3600.0)) {
      } state dead
    }

    state monitor_changes {

        when (pvAssignCount () != pvConnectCount ()) {
          printf ("danteMED: Not all channels connected.\n");
        } state waitConnected

        when(efTestAndClear(CopyFastPeakingTimeMon) && (CopyFastPeakingTime == 1)) {
            pvGet(FastPeakingTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                FastPeakingTime[i] = FastPeakingTime[0];
                pvPut(FastPeakingTime[i]);
            }
            CopyFastPeakingTime = 0;
            pvPut(CopyFastPeakingTime);
        } state monitor_changes

        when(efTestAndClear(CopyFastThresholdMon) && (CopyFastThreshold == 1)) {
            pvGet(FastThreshold[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                FastThreshold[i] = FastThreshold[0];
                pvPut(FastThreshold[i]);
            }
            CopyFastThreshold = 0;
            pvPut(CopyFastThreshold);
        } state monitor_changes

        when(efTestAndClear(CopyFastFlatTopTimeMon) && (CopyFastFlatTopTime == 1)) {
            pvGet(FastFlatTopTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                FastFlatTopTime[i] = FastFlatTopTime[0];
                pvPut(FastFlatTopTime[i]);
            }
            CopyFastFlatTopTime = 0;
            pvPut(CopyFastFlatTopTime);
        } state monitor_changes

        when(efTestAndClear(CopyPeakingTimeMon) && (CopyPeakingTime == 1)) {
            pvGet(PeakingTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                PeakingTime[i] = PeakingTime[0];
                pvPut(PeakingTime[i]);
            }
            CopyPeakingTime = 0;
            pvPut(CopyPeakingTime);
        } state monitor_changes

        when(efTestAndClear(CopyMaxPeakingTimeMon) && (CopyMaxPeakingTime == 1)) {
            pvGet(MaxPeakingTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                MaxPeakingTime[i] = MaxPeakingTime[0];
                pvPut(MaxPeakingTime[i]);
            }
            CopyMaxPeakingTime = 0;
            pvPut(CopyMaxPeakingTime);
        } state monitor_changes

        when(efTestAndClear(CopyFlatTopTimeMon) && (CopyFlatTopTime == 1)) {
            pvGet(FlatTopTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                FlatTopTime[i] = FlatTopTime[0];
                pvPut(FlatTopTime[i]);
            }
            CopyFlatTopTime = 0;
            pvPut(CopyFlatTopTime);
        } state monitor_changes

        when(efTestAndClear(CopyEnergyThresholdMon) && (CopyEnergyThreshold == 1)) {
            pvGet(EnergyThreshold[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                EnergyThreshold[i] = EnergyThreshold[0];
                pvPut(EnergyThreshold[i]);
            }
            CopyEnergyThreshold = 0;
            pvPut(CopyEnergyThreshold);
        } state monitor_changes

        when(efTestAndClear(CopyBaselineThresholdMon) && (CopyBaselineThreshold == 1)) {
            pvGet(BaselineThreshold[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                BaselineThreshold[i] = BaselineThreshold[0];
                pvPut(BaselineThreshold[i]);
            }
            CopyBaselineThreshold = 0;
            pvPut(CopyBaselineThreshold);
        } state monitor_changes

        when(efTestAndClear(CopyMaxRiseTimeMon) && (CopyMaxRiseTime == 1)) {
            pvGet(MaxRiseTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                MaxRiseTime[i] = MaxRiseTime[0];
                pvPut(MaxRiseTime[i]);
            }
            CopyMaxRiseTime = 0;
            pvPut(CopyMaxRiseTime);
        } state monitor_changes

        when(efTestAndClear(CopyResetRecoveryTimeMon) && (CopyResetRecoveryTime == 1)) {
            pvGet(ResetRecoveryTime[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                ResetRecoveryTime[i] = ResetRecoveryTime[0];
                pvPut(ResetRecoveryTime[i]);
            }
            CopyResetRecoveryTime = 0;
            pvPut(CopyResetRecoveryTime);
        } state monitor_changes

        when(efTestAndClear(CopyZeroPeakFreqMon) && (CopyZeroPeakFreq == 1)) {
            pvGet(ZeroPeakFreq[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                ZeroPeakFreq[i] = ZeroPeakFreq[0];
                pvPut(ZeroPeakFreq[i]);
            }
            CopyZeroPeakFreq = 0;
            pvPut(CopyZeroPeakFreq);
        } state monitor_changes

        when(efTestAndClear(CopyBaselineSamplesMon) && (CopyBaselineSamples == 1)) {
            pvGet(BaselineSamples[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                BaselineSamples[i] = BaselineSamples[0];
                pvPut(BaselineSamples[i]);
            }
            CopyBaselineSamples = 0;
            pvPut(CopyBaselineSamples);
        } state monitor_changes

        when(efTestAndClear(CopyGainMon) && (CopyGain == 1)) {
            pvGet(Gain[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                Gain[i] = Gain[0];
                pvPut(Gain[i]);
            }
            CopyGain = 0;
            pvPut(CopyGain);
        } state monitor_changes

        when(efTestAndClear(CopyInputPolarityMon) && (CopyInputPolarity == 1)) {
            pvGet(InputPolarity[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                InputPolarity[i] = InputPolarity[0];
                pvPut(InputPolarity[i]);
            }
            CopyInputPolarity = 0;
            pvPut(CopyInputPolarity);
        } state monitor_changes

        when(efTestAndClear(CopyAnalogOffsetMon) && (CopyAnalogOffset == 1)) {
            pvGet(AnalogOffset[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                AnalogOffset[i] = AnalogOffset[0];
                pvPut(AnalogOffset[i]);
            }
            CopyAnalogOffset = 0;
            pvPut(CopyAnalogOffset);
        } state monitor_changes

        when(efTestAndClear(CopyMaxEnergyMon) && (CopyMaxEnergy == 1)) {
            pvGet(MaxEnergy[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                MaxEnergy[i] = MaxEnergy[0];
                pvPut(MaxEnergy[i]);
            }
            CopyMaxEnergy = 0;
            pvPut(CopyMaxEnergy);
        } state monitor_changes

        when(efTestAndClear(CopyResetThresholdMon) && (CopyResetThreshold == 1)) {
            pvGet(ResetThreshold[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                ResetThreshold[i] = ResetThreshold[0];
                pvPut(ResetThreshold[i]);
            }
            CopyResetThreshold = 0;
            pvPut(CopyResetThreshold);
        } state monitor_changes

        when(efTestAndClear(CopyInputModeMon) && (CopyInputMode == 1)) {
            pvGet(InputMode[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                InputMode[i] = InputMode[0];
                pvPut(InputMode[i]);
            }
            CopyInputMode = 0;
            pvPut(CopyInputMode);
        } state monitor_changes

        when(efTestAndClear(CopyTimeConstantMon) && (CopyTimeConstant == 1)) {
            pvGet(TimeConstant[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                TimeConstant[i] = TimeConstant[0];
                pvPut(TimeConstant[i]);
            }
            CopyTimeConstant = 0;
            pvPut(CopyTimeConstant);
        } state monitor_changes

        when(efTestAndClear(CopyBaseOffsetMon) && (CopyBaseOffset == 1)) {
            pvGet(BaseOffset[0]);
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                BaseOffset[i] = BaseOffset[0];
                pvPut(BaseOffset[i]);
            }
            CopyBaseOffset = 0;
            pvPut(CopyBaseOffset);
        } state monitor_changes

        when(efTestAndClear(ReadMCAsMon) && (ReadMCAs == 1)) {
            if (AsynDebug) printf("danteMED.st: Read MCA data\n");
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                ReadMCA[i] = 1;
                pvPut(ReadMCA[i]);
            }
            ReadMCAs = 0;
            pvPut(ReadMCAs);
        } state monitor_changes

        when(efTestAndClear(MCAAcquiringMon)) {
            if (AsynDebug) printf("danteMED.st: MCAAcquiringMon, MCAAcquiring=%d\n", MCAAcquiring);
            /* If the detector is acquiring then force each MCA record to read status */
            if (MCAAcquiring) {
                for (i=0; i<nDetectors; i++) {
                    if (!EnableBoard[i]) continue;
                    statusMCA[i] = 1;
                    /* pvPut(statusMCA[i]); */
                }
            } else {
                /* If the detector is done then force each MCA record to read data */
                for (i=0; i<nDetectors; i++) {
                    if (!EnableBoard[i]) continue;
                    ReadMCA[i] = 1;
                    pvPut(ReadMCA[i], SYNC);
                }
                /* Clear the MCAAcquireBusy record */
                MCAAcquireBusy = 0;
                pvPut(MCAAcquireBusy);
            }
        } state monitor_changes

        when(efTestAndClear(PresetRealMon)) {
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                mcaPresetReal[i] = PresetReal;
                pvPut(mcaPresetReal[i]);
            }
        } state monitor_changes

        when(efTestAndClear(DeadTimeMon)) {
            DeadTime = 0.;
            numEnabled=0;
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                DeadTime += mcaDeadTime[i];
                numEnabled++;
            }
            DeadTime /= numEnabled;
            pvPut(DeadTime);
        } state monitor_changes
 
        when(efTestAndClear(IDeadTimeMon)) {
            IDeadTime = 0.;
            numEnabled=0;
            for (i=0; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                IDeadTime += mcaIDeadTime[i];
                numEnabled++;
            }
            IDeadTime /= numEnabled;
            pvPut(IDeadTime);
        } state monitor_changes
 
        /* Copy all detector 1 ROIs to all detectors by channel */
        when(efTestAndClear(CopyROIChannelMon) && (CopyROIChannel == 1)) {
            for (i=1; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                for (j=0; j<MAX_ROIS; j++) {
                    pvGet(mcaROILow[j]);
                    pvGet(mcaROIHigh[j]);
                    pvGet(mcaROIName[j]);
                    k = i*MAX_ROIS + j;
                    chan = mcaROILow[j];
                    if (chan < 0) chan = 0;
                    mcaROILow[k] = chan;
                    pvPut(mcaROILow[k]);
                    chan = mcaROIHigh[j];
                    if (chan < 0) chan = 0;
                    mcaROIHigh[k] = chan;
                    pvPut(mcaROIHigh[k]);
                    strcpy(mcaROIName[k], mcaROIName[j]);
                    pvPut(mcaROIName[k]);
                }
            }
            CopyROIChannel = 0;
            pvPut(CopyROIChannel);
        } state monitor_changes 

        /* Copy all detector 1 ROIs to all detectors by energy */
        when(efTestAndClear(CopyROIEnergyMon) && (CopyROIEnergy == 1)) {
            for (i=1; i<nDetectors; i++) {
                if (!EnableBoard[i]) continue;
                for (j=0; j<MAX_ROIS; j++) {
                    pvGet(mcaROILow[j]);
                    pvGet(mcaROIHigh[j]);
                    pvGet(mcaROIName[j]);
                    k = i*MAX_ROIS + j;
                    energy = mcaCalOffset[0] + mcaROILow[j] * mcaCalSlope[0];
                    slope = mcaCalSlope[i];
                    if (slope == 0.0) slope = 1;
                    chan = 0.5 + ((energy - mcaCalOffset[i]) / slope);
                    if (chan < 0) chan = 0;
                    if (chan > mcaNumChans[i]-1) chan = mcaNumChans[i]-1;
                    mcaROILow[k] = chan;
                    pvPut(mcaROILow[k]);
                    energy = mcaCalOffset[0] + mcaROIHigh[j] * mcaCalSlope[0];
                    chan = 0.5 + ((energy - mcaCalOffset[i]) / slope);
                    if (chan < 0) chan = 0;
                    if (chan > mcaNumChans[i]-1) chan = mcaNumChans[i]-1;
                    mcaROIHigh[k] = chan;
                    pvPut(mcaROIHigh[k]);
                    strcpy(mcaROIName[k], mcaROIName[j]);
                    pvPut(mcaROIName[k]);
                }
            }
            CopyROIEnergy = 0;
            pvPut(CopyROIEnergy);
        } state monitor_changes 
    }
}

