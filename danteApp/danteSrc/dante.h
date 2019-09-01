#ifndef NDDXP_H
#define NDDXP_H

#include <vector>
#include <epicsEvent.h>
#include <epicsMessageQueue.h>

typedef enum {
    DanteModeMCA, 
    DanteModeSpectraMapping,
    DanteModeListMapping,
} danteCollectMode_t;

#define MAX_DANTE_REPLY_LEN       16
#define MAX_DANTE_IDENTIFIER_LEN  16

/* Things that are in ADDriver.h that we want to use */
#define ADManufacturerString        "MANUFACTURER"          /**< (asynOctet,    r/o) Detector manufacturer name */
#define ADModelString               "MODEL"                 /**< (asynOctet,    r/o) Detector model name */
#define ADSerialNumberString        "SERIAL_NUMBER"         /**< (asynOctet,    r/o) Detector serial number */
#define ADSDKVersionString          "SDK_VERSION"           /**< (asynOctet,    r/o) Vendor SDK version */
#define ADFirmwareVersionString     "FIRMWARE_VERSION"      /**< (asynOctet,    r/o) Detector firmware version */

/* General parameters */
#define DanteCollectModeString              "DanteCollectMode"
#define DanteCurrentPixelString             "DanteCurrentPixel"
#define DanteMaxEnergyString                "DanteMaxEnergy"
#define DanteSpectrumXAxisString            "DanteSpectrumXAxis"

/* Internal asyn driver parameters */
#define DanteErasedString                   "DanteErased"
#define DanteAcquiringString                "DanteAcquiring"  /* Internal use only !!! */
#define DantePollTimeString                 "DantePollTime"
#define DanteForceReadString                "DanteForceRead"

/* Diagnostic trace parameters */
#define DanteTraceDataString                "DanteTraceData"
#define DanteTraceTimeArrayString           "DanteTraceTimeArray"
#define DanteTraceTimeString                "DanteTraceTime"
#define DanteTraceTriggerInstantString      "DanteTraceTriggerInstant"
#define DanteTraceTriggerRisingString       "DanteTraceTriggerRising"
#define DanteTraceTriggerFallingString      "DanteTraceTriggerFalling"
#define DanteTraceTriggerLevelString        "DanteTraceTriggerLevel"
#define DanteTraceTriggerWaitString         "DanteTraceTriggerWait"
#define DanteTraceLengthString              "DanteTraceLength"

/* Runtime statistics */
#define DanteInputCountRateString           "DanteInputCountRate"
#define DanteOutputCountRateString          "DanteOutputCountRate"
#define DanteLastTimeStampString            "DanteLastTimeStamp"
#define DanteTriggersString                 "DanteTriggers"
#define DanteEventsString                   "DanteEvents"
#define DanteEdgeDTString                   "DanteEdgeDT"
#define DanteFilt1DTString                  "DanteFilt1DT"
#define DanteZeroCountsString               "DanteZeroCounts"
#define DanteBaselinesValueString           "DanteBaselinesValue"
#define DantePupValueString                 "DantePupValue"
#define DantePupF1ValueString               "DantePupF1Value"
#define DantePupNotF1ValueString            "DantePupNotF1Value"
#define DanteResetCounterValueString        "DanteResetCounterValue"

/* Configuration parameters */
#define DanteFastFilterThresholdString      "DanteFastFilterThreshold"
#define DanteEnergyFilterThresholdString    "DanteEnergyFilterThreshold"
#define DanteEnergyBaselineThresholdString  "DanteEnergyBaselineThreshold"
#define DanteMaxRiseTimeString              "DanteMaxRiseTime"
#define DanteGainString                     "DanteGain"
#define DantePeakingTimeString              "DantePeakingTime"
#define DanteMaxPeakingTimeString           "DanteMaxPeakingTime"
#define DanteFlatTopString                  "DanteFlatTop"
#define DanteEdgePeakingTimeString          "DanteEdgePeakingTime"
#define DanteEdgeFlatTopString              "DanteEdgeFlatTop"
#define DanteResetRecoveryTimeString        "DanteResetRecoveryTime"
#define DanteZeroPeakFreqString             "DanteZeroPeakFreq"
#define DanteBaselineSamplesString          "DanteBaselineSamples"
#define DanteInvertedInputString            "DanteInvertedInput"
#define DanteTimeConstantString             "DanteTimeConstant"
#define DanteBaseOffsetString               "DanteBaseOffset"
#define DanteOverflowRecoveryTimeString     "DanteOverflowRecoveryTime"
#define DanteResetThresholdString           "DanteResetThreshold"
#define DanteTailCoefficientString          "DanteTailCoefficient"
#define DanteAnalogOffsetString             "DanteAnalogOffset"
#define DanteGatingModeString               "DanteGatingMode"


class Dante : public asynNDArrayDriver
{
public:
    Dante(const char *portName, const char *ipAddress, int nChannels, int maxBuffers, size_t maxMemory);

    /* virtual methods to override from asynNDArrayDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn);
    void report(FILE *fp, int details);

    /* Local methods to this class */
    void shutdown();
    void acquisitionTask();
    asynStatus pollMappingMode();
    int getChannel(asynUser *pasynUser, int *addr);
    asynStatus setDanteConfiguration(int addr);
    asynStatus getAcquisitionStatus(int addr);
    asynStatus getAcquisitionStatistics(int addr);
    asynStatus getMcaData(int addr);
    asynStatus getMappingData();
    asynStatus getTrace(int addr, epicsInt32* data, size_t maxLen, size_t *actualLen);
    asynStatus configureCollectMode();
    asynStatus startAcquiring();
    asynStatus waitReply(uint32_t callId, char *reply);
    void danteCallback(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data);
    epicsMessageQueue *msgQ_;

protected:

    /* From ADDriver.h */
    int ADManufacturer;
    int ADModel;
    int ADSerialNumber;
    int ADSDKVersion;
    int ADFirmwareVersion;

    /* General parameters */
    int DanteCollectMode;                   /** < Change mapping mode (0=mca; 1=spectra mapping; 2=sca mapping) (int32 read/write) addr: all/any */
    #define FIRST_DANTE_PARAM DanteCollectMode
    int DanteCurrentPixel;                  /** < Mapping mode only: read the current pixel that is being acquired into (int) */
    int DanteMaxEnergy;
    int DanteSpectrumXAxis;

    /* Internal asyn driver parameters */
    int DanteErased;               /** < Erased flag. (0=not erased; 1=erased) */
    int DanteAcquiring;            /** < Internal acquiring flag, not exposed via drvUser */
    int DantePollTime;             /** < Status/data polling time in seconds */
    int DanteForceRead;            /** < Force reading MCA spectra - used for mcaData when addr=ALL */

    /* Diagnostic trace parameters */
    int DanteTraceData;            /** < The trace array data (read) int32 array */
    int DanteTraceTimeArray;       /** < The trace timebase array (read) float64 array */
    int DanteTraceTime;            /** < The trace time per point, float64 */
    int DanteTraceTriggerInstant;  /** < Trigger instant int32 */
    int DanteTraceTriggerRising;   /** < Trigger rising crossing of trigger level int32 */
    int DanteTraceTriggerFalling;  /** < Trigger falling crossing of trigger level int32 */
    int DanteTraceTriggerLevel;    /** < Trigger level (0 - 65535) int32 */
    int DanteTraceTriggerWait;     /** < Time to wait for trigger float64 */
    int DanteTraceLength;          /** < Length of trace, multiples of 16K int32 */

    /* Runtime statistics */
    int DanteInputCountRate;      /* float64 */
    int DanteOutputCountRate;     /* float64 */
    int DanteLastTimeStamp;       /* float64 */
    int DanteTriggers;            /* uint32 */
    int DanteEvents;              /* uint32 */
    int DanteEdgeDT;              /* uint32 */
    int DanteFilt1DT;             /* uint32 */
    int DanteZeroCounts;          /* uint32 */
    int DanteBaselinesValue;      /* uint32 */
    int DantePupValue;            /* uint32 */
    int DantePupF1Value;          /* uint32 */
    int DantePupNotF1Value;       /* uint32 */
    int DanteResetCounterValue;   /* uint32 */

    /* Configuration parameters */
    int DanteFastFilterThreshold;         /* float64, keV */
    int DanteEnergyFilterThreshold;       /* float64, keV */
    int DanteEnergyBaselineThreshold;     /* float64, keV */
    int DanteMaxRiseTime;                 /* float64, usec */
    int DanteGain;                        /* float64, bins/ADC bit */
    int DantePeakingTime;                 /* float64, usec */
    int DanteMaxPeakingTime;              /* float64, usec */
    int DanteFlatTop;                     /* float64, usec */
    int DanteEdgePeakingTime;             /* float64, usec */
    int DanteEdgeFlatTop;                 /* float64, usec */
    int DanteResetRecoveryTime;           /* float64, usec */
    int DanteZeroPeakFreq;                /* float64, cps */
    int DanteBaselineSamples;             /* uint32 */
    int DanteInvertedInput;               /* uint32 */
    int DanteTimeConstant;                /* float64, units? */
    int DanteBaseOffset;                  /* uint32, bits */
    int DanteOverflowRecoveryTime;        /* float64, usec */
    int DanteResetThreshold;              /* uint32, bits */
    int DanteTailCoefficient;             /* float64, units? */
    int DanteAnalogOffset;                /* int32, 8-bit DAC units */
    int DanteGatingMode;                  /* int32 */

    /* Commands from MCA interface */
    int mcaData;                   /* int32Array, write/read */
    int mcaStartAcquire;           /* int32, write */
    int mcaStopAcquire;            /* int32, write */
    int mcaErase;                  /* int32, write */
    int mcaReadStatus;             /* int32, write */
    int mcaChannelAdvanceSource;   /* int32, write */
    int mcaNumChannels;            /* int32, write */
    int mcaAcquireMode;            /* int32, write */
    int mcaSequence;               /* int32, write */
    int mcaPrescale;               /* int32, write */
    int mcaPresetSweeps;           /* int32, write */
    int mcaPresetLowChannel;       /* int32, write */
    int mcaPresetHighChannel;      /* int32, write */
    int mcaDwellTime;              /* float64, write/read */
    int mcaPresetLiveTime;         /* float64, write */
    int mcaPresetRealTime;         /* float64, write */
    int mcaPresetCounts;           /* float64, write */
    int mcaAcquiring;              /* int32, read */
    int mcaElapsedLiveTime;        /* float64, read */
    int mcaElapsedRealTime;        /* float64, read */
    int mcaElapsedCounts;          /* float64, read */


private:
    /* Data */
    std::vector<configuration> configurations_;
    std::vector<statistics> statistics_;
    uint64_t **pMcaRaw_;
    unsigned long *pMapTemp_;
    epicsUInt16 *pMapRaw_;

    int nCards_;
    int nChannels_;
    int channelsPerCard_;
    
    epicsEvent *cmdStartEvent_;
    epicsEvent *cmdStopEvent_;
    epicsEvent *stoppedEvent_;
    
    char danteIdentifier_[MAX_DANTE_IDENTIFIER_LEN];
    uint32_t callId_;
    char danteReply_[MAX_DANTE_REPLY_LEN];

    uint32_t traceLength_;
    bool newTraceTime_;
    uint16_t *traceBuffer_;
    epicsFloat64 *traceTimeBuffer_;
    epicsFloat64 *spectrumXAxisBuffer_;
    
    bool polling_;

};
#ifdef __cplusplus
extern "C"
{
#endif

int danteConfig(const char *portName, const char *ipAddress, int nChannels, int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif

#endif
