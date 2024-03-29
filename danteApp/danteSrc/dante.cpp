/* dante.cpp
 *
 * Driver for XGlab Dante spectroscopy system
 *
 * Mark Rivers
 * University of Chicago
 *
 */

/* Standard includes... */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <algorithm>
#include <math.h>

/* EPICS includes */
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsMessageQueue.h>
#include <epicsMutex.h>
#include <epicsExit.h>
#include <envDefs.h>
#include <iocsh.h>

/* MCA includes */
#include <drvMca.h>

/* Area Detector includes */
#include <asynNDArrayDriver.h>

#include "DLL_DPP_Callback.h"

#include <epicsExport.h>
#include "dante.h"

#define DRIVER_VERSION       "1.1.0"
#define MAX_CHANNELS_PER_CARD      8
#define MAX_MCA_BINS            4096
#define ALL_BOARDS              -1
#define MAX_MESSAGE_DATA          20
#define MSG_QUEUE_SIZE            50
#define MESSAGE_TIMEOUT           20.
#define MIN_TRACE_TIME         0.016
#define TRACE_LEN_INC          16384
#define NUM_MAPPING_STATS          4
#define NUM_MAPPING_ADV_STATS     24

/** Only used for debugging/error messages to identify where the message comes from*/
static const char *driverName = "Dante";

struct danteMessage {
    uint16_t type;
    uint32_t call_id;
    uint32_t length;
    uint32_t data[MAX_MESSAGE_DATA];
};
#define MESSAGE_SIZE (sizeof(struct danteMessage))

// This global is needed because their callback does not have a private pointer.
// This will work as long as a single process does not create more than 1 Dante object
Dante *pDanteGlobal;

static void danteCallback(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data)
{
    pDanteGlobal->danteCallback(type, call_id, length, data);
}

static void c_shutdown(void* arg)
{
    Dante *pDante = (Dante*)arg;
    pDante->shutdown();
    //delete pDante;
}

static void acquisitionTaskC(void *drvPvt)
{
    Dante *pDante = (Dante *)drvPvt;
    pDante->acquisitionTask();
}

extern "C" int DanteConfig(const char *portName, const char *ipAddress, int totalBoards, size_t maxMemory)
{
    new Dante(portName, ipAddress, totalBoards, maxMemory);
    return 0;
}

/* Note: we use totalBoards+1 for maxAddr because the last address is used for "all" boards" */
Dante::Dante(const char *portName, const char *ipAddress, int totalBoards, size_t maxMemory)
    : asynNDArrayDriver(portName, totalBoards + 1, 0, maxMemory,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask | asynDrvUserMask,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0),
    uniqueId_(0), traceLength_(0), newTraceTime_(true), traceBuffer_(0), traceBufferInt32_(0), traceTimeBuffer_(0)

{
    int status = asynSuccess;
    int i, ch;
    configuration config = configuration();
    statistics stats = statistics();
    const char *functionName = "Dante";

    pDanteGlobal = this;

    if (!InitLibrary()) {
        printf("%s::%s error calling InitLibrary\n", driverName, functionName);
        return;
    }

    char libraryVersion[20];
    uint32_t libSize = sizeof(libraryVersion);
    if (!libVersion(libraryVersion, libSize)) {
        printf("%s::%s error calling libVersion\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s library version=%s\n", driverName, functionName, libraryVersion);
    }

    if (!add_to_query((char *)ipAddress)) {
        printf("%s::%s error calling add_to_query\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s ipAddress added to query=%s\n", driverName, functionName, ipAddress);
    }

    // Wait 5 seconds for devices to be found
    epicsThreadSleep(5.);

    uint16_t numDevices;
    if (!get_dev_number(numDevices)) {
        printf("%s::%s error calling get_dev_number\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s number of devices=%d\n", driverName, functionName, numDevices);
    }

    uint16_t idSize = sizeof(danteIdentifier_);
    uint16_t deviceId = 0;
    if (!get_ids(danteIdentifier_, deviceId, idSize)) {
        printf("%s::%s error calling get_ids\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s danteIdentifier_=%s\n", driverName, functionName, danteIdentifier_);
    }

    uint16_t chain = 1;
    if (!get_boards_in_chain(danteIdentifier_, chain)) {
        printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s boards in chain=%d\n", driverName, functionName, chain);
    }

    // Wait a little bit for daisy chain synchronization and ask again for connected systems.
    epicsThreadSleep(1.0);
    if (!get_ids(danteIdentifier_, deviceId, idSize)) {
        printf("%s::%s error calling get_ids\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s danteIdentifier_=%s\n", driverName, functionName, danteIdentifier_);
    }

    if (!get_boards_in_chain(danteIdentifier_, chain)) {
        printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s boards in chain=%d\n", driverName, functionName, chain);
    }

    if (!register_callback(::danteCallback)) {
        printf("%s::%s error calling register_callback\n", driverName, functionName);
        return;
    } else {
        printf("%s::%s register callback OK\n", driverName, functionName);
    }

    // Create the vector of active board numbers. Assume all boards are active for now
    totalBoards_ = std::min(totalBoards, (int)chain);
    for (i=0; i<totalBoards_; i++) {
        activeBoards_.push_back(i);
    }

    // It is necessary to disable the autoScanSlaves() when configuring, in order to prevent interlock problems. Keep disabled also for acquisitions.
    autoScanSlaves(false);
    epicsThreadSleep(0.050);     // wait 50ms

    /* General parameters */
    createParam(DanteCollectModeString,            asynParamInt32,   &DanteCollectMode);
    createParam(DanteCurrentPixelString,           asynParamInt32,   &DanteCurrentPixel);
    createParam(DanteMaxEnergyString,              asynParamFloat64, &DanteMaxEnergy);
    createParam(DanteSpectrumXAxisString,          asynParamFloat64Array, &DanteSpectrumXAxis);

    /* Internal asyn driver parameters */
    createParam(DanteErasedString,                 asynParamInt32,   &DanteErased);
    createParam(DanteAcquiringString,              asynParamInt32,   &DanteAcquiring);
    createParam(DantePollTimeString,               asynParamFloat64, &DantePollTime);
    createParam(DanteForceReadString,              asynParamInt32,   &DanteForceRead);
    createParam(DanteEnableBoardString,            asynParamInt32,   &DanteEnableBoard);
    createParam(DanteEnableConfigureString,        asynParamInt32,   &DanteEnableConfigure);

    /* Diagnostic trace parameters */
    createParam(DanteTraceDataString,              asynParamInt32Array,   &DanteTraceData);
    createParam(DanteTraceTimeArrayString,         asynParamFloat64Array, &DanteTraceTimeArray);
    createParam(DanteTraceTimeString,              asynParamFloat64,      &DanteTraceTime);
    createParam(DanteTraceTriggerInstantString,    asynParamInt32,        &DanteTraceTriggerInstant);
    createParam(DanteTraceTriggerRisingString,     asynParamInt32,        &DanteTraceTriggerRising);
    createParam(DanteTraceTriggerFallingString,    asynParamInt32,        &DanteTraceTriggerFalling);
    createParam(DanteTraceTriggerLevelString,      asynParamInt32,        &DanteTraceTriggerLevel);
    createParam(DanteTraceTriggerWaitString,       asynParamFloat64,      &DanteTraceTriggerWait);
    createParam(DanteTraceLengthString,            asynParamInt32,        &DanteTraceLength);
    createParam(DanteReadTraceString,              asynParamInt32,        &DanteReadTrace);

    /* Runtime statistics */
    createParam(DanteInputCountRateString,         asynParamFloat64, &DanteInputCountRate);
    createParam(DanteOutputCountRateString,        asynParamFloat64, &DanteOutputCountRate);
    createParam(DanteLastTimeStampString,          asynParamFloat64, &DanteLastTimeStamp);
    createParam(DanteTriggersString,               asynParamInt32,   &DanteTriggers);
    createParam(DanteEventsString,                 asynParamInt32,   &DanteEvents);
    createParam(DanteFastDTString,                 asynParamInt32,   &DanteFastDT);
    createParam(DanteFilt1DTString,                asynParamInt32,   &DanteFilt1DT);
    createParam(DanteZeroCountsString,             asynParamInt32,   &DanteZeroCounts);
    createParam(DanteBaselinesValueString,         asynParamInt32,   &DanteBaselinesValue);
    createParam(DantePupValueString,               asynParamInt32,   &DantePupValue);
    createParam(DantePupF1ValueString,             asynParamInt32,   &DantePupF1Value);
    createParam(DantePupNotF1ValueString,          asynParamInt32,   &DantePupNotF1Value);
    createParam(DanteResetCounterValueString,      asynParamInt32,   &DanteResetCounterValue);

    /* Configuration parameters */
    createParam(DanteFastFilterThresholdString,         asynParamFloat64, &DanteFastFilterThreshold);
    createParam(DanteFastFilterThresholdRBVString,      asynParamFloat64, &DanteFastFilterThresholdRBV);
    createParam(DanteEnergyFilterThresholdString,       asynParamFloat64, &DanteEnergyFilterThreshold);
    createParam(DanteEnergyFilterThresholdRBVString,    asynParamFloat64, &DanteEnergyFilterThresholdRBV);
    createParam(DanteBaselineThresholdString,           asynParamFloat64, &DanteBaselineThreshold);
    createParam(DanteBaselineThresholdRBVString,        asynParamFloat64, &DanteBaselineThresholdRBV);
    createParam(DanteMaxRiseTimeString,                 asynParamFloat64, &DanteMaxRiseTime);
    createParam(DanteMaxRiseTimeRBVString,              asynParamFloat64, &DanteMaxRiseTimeRBV);
    createParam(DanteGainString,                        asynParamFloat64, &DanteGain);
    createParam(DantePeakingTimeString,                 asynParamFloat64, &DantePeakingTime);
    createParam(DantePeakingTimeRBVString,              asynParamFloat64, &DantePeakingTimeRBV);
    createParam(DanteMaxPeakingTimeString,              asynParamFloat64, &DanteMaxPeakingTime);
    createParam(DanteMaxPeakingTimeRBVString,           asynParamFloat64, &DanteMaxPeakingTimeRBV);
    createParam(DanteFlatTopString,                     asynParamFloat64, &DanteFlatTop);
    createParam(DanteFlatTopRBVString,                  asynParamFloat64, &DanteFlatTopRBV);
    createParam(DanteFastPeakingTimeString,             asynParamFloat64, &DanteFastPeakingTime);
    createParam(DanteFastPeakingTimeRBVString,          asynParamFloat64, &DanteFastPeakingTimeRBV);
    createParam(DanteFastFlatTopString,                 asynParamFloat64, &DanteFastFlatTop);
    createParam(DanteFastFlatTopRBVString,              asynParamFloat64, &DanteFastFlatTopRBV);
    createParam(DanteResetRecoveryTimeString,           asynParamFloat64, &DanteResetRecoveryTime);
    createParam(DanteResetRecoveryTimeRBVString,        asynParamFloat64, &DanteResetRecoveryTimeRBV);
    createParam(DanteZeroPeakFreqString,                asynParamFloat64, &DanteZeroPeakFreq);
    createParam(DanteBaselineSamplesString,             asynParamInt32,   &DanteBaselineSamples);
    createParam(DanteInvertedInputString,               asynParamInt32,   &DanteInvertedInput);
    createParam(DanteTimeConstantString,                asynParamFloat64, &DanteTimeConstant);
    createParam(DanteBaseOffsetString,                  asynParamInt32,   &DanteBaseOffset);
    createParam(DanteResetThresholdString,              asynParamInt32,   &DanteResetThreshold);

    /* Other parameters */
    createParam(DanteInputModeString,               asynParamInt32,   &DanteInputMode);
    createParam(DanteAnalogOffsetString,            asynParamInt32,   &DanteAnalogOffset);
    createParam(DanteGatingModeString,              asynParamInt32,   &DanteGatingMode);
    createParam(DanteMappingPointsString,           asynParamInt32,   &DanteMappingPoints);
    createParam(DanteListBufferSizeString,          asynParamInt32,   &DanteListBufferSize);
    createParam(DanteKeepAliveString,               asynParamInt32,   &DanteKeepAlive);

    /* Commands from MCA interface */
    createParam(mcaDataString,                     asynParamInt32Array, &mcaData);
    createParam(mcaStartAcquireString,             asynParamInt32,   &mcaStartAcquire);
    createParam(mcaStopAcquireString,              asynParamInt32,   &mcaStopAcquire);
    createParam(mcaEraseString,                    asynParamInt32,   &mcaErase);
    createParam(mcaReadStatusString,               asynParamInt32,   &mcaReadStatus);
    createParam(mcaChannelAdvanceSourceString,     asynParamInt32,   &mcaChannelAdvanceSource);
    createParam(mcaNumChannelsString,              asynParamInt32,   &mcaNumChannels);
    createParam(mcaAcquireModeString,              asynParamInt32,   &mcaAcquireMode);
    createParam(mcaSequenceString,                 asynParamInt32,   &mcaSequence);
    createParam(mcaPrescaleString,                 asynParamInt32,   &mcaPrescale);
    createParam(mcaPresetSweepsString,             asynParamInt32,   &mcaPresetSweeps);
    createParam(mcaPresetLowChannelString,         asynParamInt32,   &mcaPresetLowChannel);
    createParam(mcaPresetHighChannelString,        asynParamInt32,   &mcaPresetHighChannel);
    createParam(mcaDwellTimeString,                asynParamFloat64, &mcaDwellTime);
    createParam(mcaPresetLiveTimeString,           asynParamFloat64, &mcaPresetLiveTime);
    createParam(mcaPresetRealTimeString,           asynParamFloat64, &mcaPresetRealTime);
    createParam(mcaPresetCountsString,             asynParamFloat64, &mcaPresetCounts);
    createParam(mcaAcquiringString,                asynParamInt32,   &mcaAcquiring);
    createParam(mcaElapsedLiveTimeString,          asynParamFloat64, &mcaElapsedLiveTime);
    createParam(mcaElapsedRealTimeString,          asynParamFloat64, &mcaElapsedRealTime);
    createParam(mcaElapsedCountsString,            asynParamFloat64, &mcaElapsedCounts);

    /* Register the epics exit function to be called when the IOC exits... */
    epicsAtExit(c_shutdown, this);

    /* Set the parameters in param lib */
    status |= setIntegerParam(DanteCollectMode, 0);
    /* Clear the acquiring flag, must do this or things don't work right because acquisitionTask does not set till
     * acquire first starts */
    for (i=0; i<=totalBoards_; i++) {
        setIntegerParam(i, mcaAcquiring, 0);
    }

    /* Create the start and stop events that will be used to signal our
     * acquisitionTask when to start/stop polling the HW     */
    cmdStartEvent_ = new epicsEvent();
    cmdStopEvent_ = new epicsEvent();
    stoppedEvent_ = new epicsEvent();
    msgQ_ = new epicsMessageQueue(MSG_QUEUE_SIZE, MESSAGE_SIZE);

    /* Allocate memory pointers for each of the boards */
    pMcaRaw_            = (uint64_t**)        calloc(totalBoards_, sizeof(uint64_t*));
    pMappingMCAData_    = (uint16_t**)        calloc(totalBoards_, sizeof(uint16_t*));
    pListData_          = (uint64_t**)        calloc(totalBoards_, sizeof(uint64_t*));
    pMappingSpectrumId_ = (uint32_t**)        calloc(totalBoards_, sizeof(uint32_t*));
    pMappingStats_      = (mappingStats**)    calloc(totalBoards_, sizeof(mappingStats*));
    pMappingAdvStats_   = (mappingAdvStats**) calloc(totalBoards_, sizeof(mappingAdvStats*));
    /* Allocate a memory area for each spectrum */
    for (ch=0; ch<totalBoards_; ch++) {
        pMcaRaw_[ch] = (uint64_t *)calloc(MAX_MCA_BINS, sizeof(uint64_t));
    }

    // Allocate per-board memory
    for (i=0; i<totalBoards_; i++) {
        configurations_.push_back(config);
        statistics_.push_back(stats);
        numEventsAvailable_.push_back(0);
    }

    /* Allocate an internal buffer long enough to hold all the energy values in a spectrum */
    spectrumXAxisBuffer_ = (epicsFloat64*)calloc(MAX_MCA_BINS, sizeof(epicsFloat64));

    /* Start up acquisition thread */
    setDoubleParam(DantePollTime, 0.01);
    polling_ = 1;
    status = (epicsThreadCreate("acquisitionTask",
              epicsThreadPriorityMedium,
              epicsThreadGetStackSize(epicsThreadStackMedium),
              (EPICSTHREADFUNC)acquisitionTaskC, this) == NULL);
    if (status) {
        printf("%s:%s epicsThreadCreate failure for image task\n",
                driverName, functionName);
        return;
    }

    setStringParam(NDDriverVersion, DRIVER_VERSION);
    setStringParam(ADManufacturer, "XGLab");
    setStringParam(ADModel, "Dante");
    setStringParam(ADSDKVersion, libraryVersion);
    setStringParam(ADSerialNumber, danteIdentifier_);

    /* Set default values for parameters that cannot be read */
    for (i=0; i<=totalBoards_; i++) {
        char firmwareVersion[20];
        callId_ = getFirmware(danteIdentifier_, i);
        waitReply(callId_, danteReply_, "getFirmware");
        snprintf(firmwareVersion, sizeof(firmwareVersion)-1, "%d.%d.%d",
                 danteReply_[0], danteReply_[1], danteReply_[2]);
        setStringParam (i, ADFirmwareVersion, firmwareVersion);
        setIntegerParam(i, DanteForceRead,                  0);
        setDoubleParam (i, mcaPresetCounts,               0.0);
        setDoubleParam (i, mcaElapsedCounts,              0.0);
        setDoubleParam (i, mcaPresetRealTime,             0.0);
        setDoubleParam (i, mcaPresetRealTime,             0.0);
        setIntegerParam(i, DanteCurrentPixel,             0);
        setIntegerParam(i, mcaNumChannels,                2048);
        setDoubleParam (i, DanteFastFilterThreshold,      0.1);
        setDoubleParam (i, DanteEnergyFilterThreshold,    0.0);
        setDoubleParam (i, DanteBaselineThreshold,        0.0);
        setDoubleParam (i, DanteMaxRiseTime,              0.25);
        setDoubleParam (i, DanteGain,                     1.0);
        setDoubleParam (i, DantePeakingTime,              1.0);
        setDoubleParam (i, DanteMaxPeakingTime,           0.0);
        setDoubleParam (i, DanteFlatTop,                  0.05);
        setDoubleParam (i, DanteFastPeakingTime,          0.03);
        setDoubleParam (i, DanteFastFlatTop,              0.01);
        setDoubleParam (i, DanteResetRecoveryTime,        6.0);
        setDoubleParam (i, DanteZeroPeakFreq,             1000.);
        setIntegerParam(i, DanteBaselineSamples,          64);
        setIntegerParam(i, DanteInvertedInput,            0);
        setDoubleParam (i, DanteTimeConstant,             0.0);
        setIntegerParam(i, DanteBaseOffset,               0);
        setIntegerParam(i, DanteResetThreshold,           300);
    }

    /* Read the MCA and DXP parameters once */
    dataAcquiring();
    this->getAcquisitionStatistics(ALL_BOARDS);

    // Enable array callbacks by default
    setIntegerParam(NDArrayCallbacks, 1);

}

asynStatus Dante::waitReply(uint32_t callId, char *reply, const char *caller) {
    struct danteMessage message;
    int numRecv;
    static const char *functionName = "waitReply";

    numRecv = msgQ_->receive(&message, sizeof(message), MESSAGE_TIMEOUT);
    if (numRecv != sizeof(message)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error receiving message in %f seconds, caller=%s numRecv=%d\n",
            driverName, functionName, MESSAGE_TIMEOUT, caller, numRecv);
        return asynError;
    }
    if (message.call_id != callId) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s incorrect callId expected=%d actual=%d\n",
            driverName, functionName, callId, message.call_id);
        return asynError;
    }
    for (uint32_t i=0; i<message.length; i++) {
        reply[i]=message.data[i];
    }
    return asynSuccess;
}

void Dante::danteCallback(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data) {
    struct danteMessage message;
    static const char *functionName = "danteCallback";

    message.type = type;
    message.call_id = call_id;
    message.length = length;
    for (uint32_t i=0; i<length; i++) {
        message.data[i] = data[i];
    }
    if (msgQ_->send(&message, sizeof(message)) != 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error sending message\n", driverName, functionName);
    }
}


/* virtual methods to override from asynNDArrayDriver */
asynStatus Dante::writeInt32( asynUser *pasynUser, epicsInt32 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int acquiring, mode;
    const char* functionName = "writeInt32";

    getBoard(pasynUser, &addr);
    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    if (function == mcaStartAcquire)
    {
        status = this->startAcquiring();
    }
    else if (function == mcaStopAcquire)
    {
        callId_ = stop(danteIdentifier_);
        waitReply(callId_, danteReply_, "stop");
        /* Wait for the acquisition task to realize the run has stopped and do the callbacks */
        while (1) {
            getIntegerParam(addr, mcaAcquiring, &acquiring);
            if (!acquiring) break;
            this->unlock();
            epicsThreadSleep(epicsThreadSleepQuantum());
            this->lock();
        }
    }
    else if (function == mcaReadStatus)
    {
        getIntegerParam(DanteCollectMode, &mode);
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s mcaReadStatus [%d] mode=%d\n",
            driverName, functionName, function, mode);
        /* We let the polling task set the acquiring flag, so that we can be sure that
         * the statistics and data have been read when needed.  DON'T READ ACQUIRE STATUS HERE */
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (mode == DanteModeMCA) {
            /* If we are acquiring then read the statistics, else we use the cached values */
            if (acquiring) status = this->getAcquisitionStatistics(addr);
        }
    }
    else if (
        (function == DanteBaselineSamples) ||
        (function == DanteInvertedInput) ||
        (function == DanteBaseOffset) ||
        (function == DanteResetThreshold))
    {
        this->setDanteConfiguration(addr);
    }
    else if (function == DanteAnalogOffset) {
        struct configuration_offset cfgOffset;
        cfgOffset.offset_val1 = value;
        cfgOffset.offset_val2 = value;
        callId_ = configure_offset(danteIdentifier_, addr, cfgOffset);
        waitReply(callId_, danteReply_, "configure_offset");
    }
    else if (function == DanteInputMode) {
        callId_ = configure_input(danteIdentifier_, addr, (InputMode)value);
        waitReply(callId_, danteReply_, "configure_input");
    }
    else if (function == DanteGatingMode) {
        // We force all boards to use the same gating mode as board 0
        GatingMode gatingMode = (GatingMode)value;
        for (const auto& board: activeBoards_) {
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                "%s::%s calling configure_gating, gatingMode=%d, board=%d\n",
                driverName, functionName, gatingMode, board);
            callId_ = configure_gating(danteIdentifier_, gatingMode, board);
            waitReply(callId_, danteReply_, "configure_gating");
        }
    }
    else if (function == DanteTraceLength) {
        // For length to be a multiple of 16K.
        uint16_t length = value / TRACE_LEN_INC;
        if (length < 1) length = 1;
        value = length * TRACE_LEN_INC;
        setIntegerParam(function, value);
        traceLength_ = value;
        /* Allocate a buffer for the trace data */
        if (traceBuffer_) free(traceBuffer_);
        traceBuffer_ = (uint16_t *)malloc(traceLength_ * sizeof(uint16_t));
        if (traceBufferInt32_) free(traceBufferInt32_);
        traceBufferInt32_ = (int32_t *)malloc(traceLength_ * sizeof(int32_t));
        /* Allocate a buffer for the trace time array */
        if (traceTimeBuffer_) free(traceTimeBuffer_);
        traceTimeBuffer_ = (epicsFloat64 *)malloc(traceLength_ * sizeof(epicsFloat64));
        newTraceTime_ = true;
    }
    else if (function == DanteReadTrace) {
        status = this->getTraces();
    }
    else if ((function == DanteEnableConfigure) && (value == 1)) {
        // enableConfigure has been set to 1, download configuration for all active boards
        for (const auto& board: activeBoards_) {
            setDanteConfiguration(board);
        }
    }
    else if (function == DanteEnableBoard) {
        activeBoards_.clear();
        for (int board=0; board<totalBoards_; board++) {
            int enable;
            getIntegerParam(board, DanteEnableBoard, &enable);
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s board=%d: getIntegerParam(DanteEnableBoard)=%d\n", driverName, functionName, board, enable);
            if (enable) {
                callId_ = disableBoard(danteIdentifier_, board, false);
                waitReply(callId_, danteReply_, "disableBoard");
                if (callId_ != 0) {
                    activeBoards_.push_back(board);
                    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s board=%d: Board enabled. In list.\n", driverName, functionName, board);
                }
                else {
                    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s board=%d: Board enable failed. Not in list.\n", driverName, functionName, board);
                }
            }
            else {
                callId_ = disableBoard(danteIdentifier_, board, true);
                waitReply(callId_, danteReply_, "disableBoard");
                if (callId_ != 0) {
                    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s board=%d: Board disabled.\n", driverName, functionName, board);
                }
                else {
                    asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s board=%d: Board disable failed.\n", driverName, functionName, board);
                }
            }
        }
    }
    else if (function == DanteKeepAlive) {
        // This just periodically reads the firmware version of the first board to keep the socket alive
        callId_ = getFirmware(danteIdentifier_, 0);
        waitReply(callId_, danteReply_, "getFirmware");
    }

    /* Call the callback */
    callParamCallbacks(addr);
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

asynStatus Dante::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    const char *functionName = "writeFloat64";

    getBoard(pasynUser, &addr);
    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setDoubleParam(addr, function, value);

    if (function == DanteMaxEnergy) {
        int numMcaChannels;
        getIntegerParam(addr, mcaNumChannels, &numMcaChannels);
        if (numMcaChannels <= 0.)
            numMcaChannels = 2048;
        /* Create a waveform which contain the value (in keV) of all the energy bins in the spectrum */
        double binEnergyEV = value / numMcaChannels;
        for(int bin=0; bin<numMcaChannels; bin++)
        {
            *(spectrumXAxisBuffer_ + bin) = (bin + 1) * binEnergyEV;
        }
        doCallbacksFloat64Array(spectrumXAxisBuffer_, numMcaChannels, DanteSpectrumXAxis, addr);
        // Must call setDanteConfiguration() because the energy parameters need to be recalculated and reloaded
        this->setDanteConfiguration(addr);
    }

    if ((function == DanteMaxEnergy) ||
        (function == DanteFastFilterThreshold) ||
        (function == DanteEnergyFilterThreshold) ||
        (function == DanteBaselineThreshold) ||
        (function == DanteMaxRiseTime) ||
        (function == DanteGain) ||
        (function == DantePeakingTime) ||
        (function == DanteMaxPeakingTime) ||
        (function == DanteFlatTop) ||
        (function == DanteFastPeakingTime) ||
        (function == DanteFastFlatTop) ||
        (function == DanteResetRecoveryTime) ||
        (function == DanteZeroPeakFreq) ||
        (function == DanteTimeConstant))
    {
        this->setDanteConfiguration(addr);
    }

    if (function == DanteTraceTime) {
        // Convert from microseconds to decimation of 16 ns.
        uint16_t decRatio = (uint16_t)(value/MIN_TRACE_TIME);
        if (decRatio < 1) decRatio = 1;
        if (decRatio > 32) decRatio = 32;
        value = decRatio * MIN_TRACE_TIME;
        setDoubleParam(function, value);
        newTraceTime_ = true;
    }
    /* Call the callback */
    callParamCallbacks(addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

asynStatus Dante::readInt32Array(asynUser *pasynUser, epicsInt32 *value, size_t nElements, size_t *nIn)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int board;
    int nBins, acquiring,mode;
    int i;
    const char *functionName = "readInt32Array";

    board = this->getBoard(pasynUser, &addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s::%s addr=%d board=%d function=%d\n",
        driverName, functionName, addr, board, function);
    if (function == mcaData)
    {
        if (board == ALL_BOARDS)
        {
            // if the MCA ALL board is being read - force reading of all individual
            // boards using the DanteForceRead command.
            for (const auto& ch: activeBoards_) {
                setIntegerParam(ch, DanteForceRead, 1);
                callParamCallbacks(ch);
                setIntegerParam(ch, DanteForceRead, 0);
                callParamCallbacks(ch);
            }
            goto done;
        }
        getIntegerParam(board, mcaNumChannels, &nBins);
        if (nBins > (int)nElements) nBins = (int)nElements;
        getIntegerParam(board, mcaAcquiring, &acquiring);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
            driverName, functionName, board, nBins, acquiring);
        *nIn = nBins;
        getIntegerParam(DanteCollectMode, &mode);

        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s mode=%d acquiring=%d\n",
            driverName, functionName, mode, acquiring);
        if (acquiring)
        {
            if (mode == DanteModeMCA)
            {
                /* While acquiring we'll force reading the data from the HW */
                this->getMcaData(addr);
            } else if (mode == DanteModeMCAMapping)
            {
                /*  Nothing needed here, the last data read from the mapping buffer has already been
                 *  copied to the buffer pointed to by pMcaRaw_. */
            }
        }
        for (i=0; i<nBins; i++) {
            value[i] = (epicsInt32)(pMcaRaw_[addr][i]);
        }
    }
    else {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s::%s Function not implemented: [%d]\n",
                driverName, functionName, function);
            status = asynError;
    }
    done:

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return(status);
}

int Dante::getBoard(asynUser *pasynUser, int *addr)
{
    int board;
    pasynManager->getAddr(pasynUser, addr);

    board = *addr;
    if (*addr == totalBoards_) board = ALL_BOARDS;
    return board;
}

asynStatus Dante::setDanteConfiguration(int addr)
{
    double maxEnergy;
    double dValue;
    int iValue;
    int enableConfigure;
    double mcaBinWidth;
    double usecToFastSample = 1e-6/8e-9;
    double usecToSlowSample = 1e-6/32e-9;
    int numChannels;
    static const char *functionName = "setDanteParam";

    getIntegerParam(DanteEnableConfigure, &enableConfigure);
    // If enableConfigure is 0 then we don't do anything
    if (!enableConfigure) return asynSuccess;
    getDoubleParam(addr, DanteMaxEnergy, &maxEnergy);
    getIntegerParam(addr, mcaNumChannels, &numChannels);
    mcaBinWidth = maxEnergy/numChannels;

    configuration *pConfig = &configurations_[addr];

    getDoubleParam(addr, DanteFastFilterThreshold, &dValue);
    pConfig->fast_filter_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteFastFilterThresholdRBV, pConfig->fast_filter_thr * mcaBinWidth);

    getDoubleParam(addr, DanteEnergyFilterThreshold, &dValue);
    pConfig->energy_filter_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteEnergyFilterThresholdRBV, pConfig->energy_filter_thr * mcaBinWidth);

    getDoubleParam(addr, DanteBaselineThreshold, &dValue);
    pConfig->energy_baseline_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteBaselineThresholdRBV, pConfig->energy_baseline_thr * mcaBinWidth);

    getDoubleParam(addr, DanteMaxRiseTime, &dValue);
    pConfig->max_risetime = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteMaxRiseTimeRBV, pConfig->max_risetime / usecToFastSample);

    getDoubleParam(addr, DanteGain, &dValue);
    pConfig->gain = dValue;

    getDoubleParam(addr, DantePeakingTime, &dValue);
    pConfig->peaking_time = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DantePeakingTimeRBV, pConfig->peaking_time / usecToSlowSample);

    getDoubleParam(addr, DanteMaxPeakingTime, &dValue);
    pConfig->max_peaking_time = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DanteMaxPeakingTimeRBV, pConfig->max_peaking_time / usecToSlowSample);

    getDoubleParam(addr, DanteFlatTop, &dValue);
    pConfig->flat_top = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DanteFlatTopRBV, pConfig->flat_top / usecToSlowSample);

    getDoubleParam(addr, DanteFastPeakingTime, &dValue);
    pConfig->edge_peaking_time = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteFastPeakingTimeRBV, pConfig->edge_peaking_time / usecToFastSample);

    getDoubleParam(addr, DanteFastFlatTop, &dValue);
    pConfig->edge_flat_top = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteFastFlatTopRBV, pConfig->edge_flat_top / usecToFastSample);

    getDoubleParam(addr, DanteResetRecoveryTime, &dValue);
    pConfig->reset_recovery_time = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteResetRecoveryTimeRBV, pConfig->reset_recovery_time / usecToFastSample);

    getDoubleParam(addr, DanteZeroPeakFreq, &dValue);
    pConfig->zero_peak_freq = dValue/1000.;

    getIntegerParam(addr, DanteBaselineSamples, &iValue);
    pConfig->baseline_samples = iValue;

    getIntegerParam(addr, DanteInvertedInput, &iValue);
    pConfig->inverted_input = iValue ? true : false;

    getDoubleParam(addr, DanteTimeConstant, &dValue);
    pConfig->time_constant = dValue;

    getIntegerParam(addr, DanteBaseOffset, &iValue);
    pConfig->base_offset = iValue;

    pConfig->overflow_recovery = 0;

    getIntegerParam(addr, DanteResetThreshold, &iValue);
    pConfig->reset_threshold = iValue;

    pConfig->tail_coefficient = 0.;

    callId_ = configure(danteIdentifier_, addr, *pConfig);
    if (callId_ < 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error calling configure = %d\n",
            driverName, functionName, callId_);
            return asynError;
    }
    waitReply(callId_, danteReply_, "configure");
    callParamCallbacks(addr);
    return asynSuccess;
}

bool Dante::waveformAcquiring()
{
    bool acquiring=false;
    int boardAcquiring;
    static const char *functionName = "waveformAcquiring";

    /* Note: we use the internal parameter DanteAcquiring rather than mcaAcquiring here
     * because we need to do callbacks in acquisitionTask() on all other parameters before
     * we do callbacks on mcaAcquiring, and callParamCallbacks does not allow control over the order. */
    for (const auto& board: activeBoards_) {
        callId_ = isRunning_system(danteIdentifier_, board);
        waitReply(callId_, danteReply_, "isRunning_system");
        boardAcquiring = danteReply_[0];
        setIntegerParam(board, DanteAcquiring, boardAcquiring);
        if (boardAcquiring) acquiring = true;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s board=%d: boardAcquiring=%d\n",
            driverName, functionName, board, boardAcquiring);
    }
    setIntegerParam(totalBoards_, DanteAcquiring, acquiring?1:0);
    return acquiring;
}

bool Dante::dataAcquiring()
{
    bool acquiring=false;
    bool lastDataReceived;
    int boardAcquiring;
    static const char *functionName = "dataAcquiring";

    /* Note: we use the internal parameter DanteAcquiring rather than mcaAcquiring here
     * because we need to do callbacks in acquisitionTask() on all other parameters before
     * we do callbacks on mcaAcquiring, and callParamCallbacks does not allow control over the order. */
    for (const auto& board: activeBoards_) {
        isLastDataReceived(danteIdentifier_, board, lastDataReceived);
        boardAcquiring = lastDataReceived ? 0 : 1;
        setIntegerParam(board, DanteAcquiring, boardAcquiring);
        if (boardAcquiring) acquiring = true;
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s board=%d: boardAcquiring=%d\n",
            driverName, functionName, board, boardAcquiring);
    }
    setIntegerParam(totalBoards_, DanteAcquiring, acquiring ? 1 : 0);
    return acquiring;
}

asynStatus Dante::getAcquisitionStatistics(int addr)
{
    double dvalue, realTime=0, liveTime=0, icr=0, ocr=0, lastTimeStamp=0;
    int events=0, triggers=0, fastDT=0, filt1DT=0, zeroCounts=0, baselinesValue=0;
    int pupValue=0, pupF1Value=0, pupNotF1Value=0, resetCounterValue=0;
    int ivalue;
    int board=addr;
    int erased;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == totalBoards_) board = ALL_BOARDS;
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s addr=%d board=%d\n",
        driverName, functionName, addr, board);
    if (board == ALL_BOARDS) { /* All boards */
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s start ALL_BOARDS\n",
            driverName, functionName);
        addr = totalBoards_;
        for (const auto& i: activeBoards_) {
            /* Call ourselves recursively but with a specific board */
            this->getAcquisitionStatistics(i);
            getDoubleParam(i, mcaElapsedRealTime, &realTime);
            realTime = std::max(realTime, dvalue);
            getDoubleParam(i, mcaElapsedLiveTime, &dvalue);
            liveTime = std::max(liveTime, dvalue);
            getDoubleParam(i, DanteInputCountRate, &dvalue);
            icr = std::max(icr, dvalue);
            getDoubleParam(i, DanteOutputCountRate, &dvalue);
            ocr = std::max(ocr, dvalue);
            getIntegerParam(i, DanteTriggers, &ivalue);
            triggers = std::max(triggers, ivalue);
            getIntegerParam(i, DanteEvents, &ivalue);
            events = std::max(events, ivalue);
            getIntegerParam(i, DanteFastDT, &ivalue);
            fastDT = std::max(fastDT, ivalue);
            getIntegerParam(i, DanteFilt1DT, &ivalue);
            filt1DT = std::max(filt1DT, ivalue);
            getIntegerParam(i, DanteZeroCounts, &ivalue);
            zeroCounts = std::max(zeroCounts, ivalue);
            getIntegerParam(i, DanteBaselinesValue, &ivalue);
            baselinesValue = std::max(baselinesValue, ivalue);
            getIntegerParam(i, DantePupValue, &ivalue);
            pupValue = std::max(pupValue, ivalue);
            getIntegerParam(i, DantePupF1Value, &ivalue);
            pupF1Value = std::max(pupF1Value, ivalue);
            getIntegerParam(i, DantePupNotF1Value, &ivalue);
            pupNotF1Value = std::max(pupNotF1Value, ivalue);
            getIntegerParam(i, DanteResetCounterValue, &ivalue);
            resetCounterValue = std::max(resetCounterValue, ivalue);
        }
        setDoubleParam(addr, mcaElapsedRealTime,    realTime);
        setDoubleParam(addr, mcaElapsedLiveTime,    liveTime);
        setDoubleParam(addr, DanteInputCountRate,   icr);
        setDoubleParam(addr, DanteOutputCountRate,  ocr);
        setDoubleParam(addr, DanteLastTimeStamp,    lastTimeStamp);
        setIntegerParam(addr, DanteTriggers,        triggers);
        setIntegerParam(addr, DanteEvents,          events);
        setIntegerParam(addr, DanteFastDT,          fastDT);
        setIntegerParam(addr, DanteFilt1DT,         filt1DT);
        setIntegerParam(addr, DanteZeroCounts,      zeroCounts);
        setIntegerParam(addr, DanteBaselinesValue,  baselinesValue);
        setIntegerParam(addr, DantePupValue,        pupValue);
        setIntegerParam(addr, DantePupF1Value,      pupF1Value);
        setIntegerParam(addr, DantePupNotF1Value,   pupNotF1Value);
        setIntegerParam(addr, DanteResetCounterValue, resetCounterValue);
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s end ALL_BOARDS\n",
            driverName, functionName);
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s start board %d\n",
            driverName, functionName, addr);
        getIntegerParam(addr, DanteErased, &erased);
        if (erased) {
            setDoubleParam(addr, mcaElapsedRealTime,    0);
            setDoubleParam(addr, mcaElapsedLiveTime,    0);
            setDoubleParam(addr, DanteInputCountRate,   0);
            setDoubleParam(addr, DanteOutputCountRate,  0);
            setDoubleParam(addr, DanteLastTimeStamp,    0);
            setIntegerParam(addr, DanteTriggers,        0);
            setIntegerParam(addr, DanteEvents,          0);
            setIntegerParam(addr, DanteFastDT,          0);
            setIntegerParam(addr, DanteFilt1DT,         0);
            setIntegerParam(addr, DanteZeroCounts,      0);
            setIntegerParam(addr, DanteBaselinesValue,  0);
            setIntegerParam(addr, DantePupValue,        0);
            setIntegerParam(addr, DantePupF1Value,      0);
            setIntegerParam(addr, DantePupNotF1Value,   0);
            setIntegerParam(addr, DanteResetCounterValue, 0);

        } else {
            statistics *pStats = &statistics_[addr];
            // real_time and live_time are returned in microseconds, though this is not documented
            setDoubleParam(addr,  mcaElapsedRealTime,     pStats->real_time/1e6);
            setDoubleParam(addr,  mcaElapsedLiveTime,     pStats->live_time/1e6);
            setDoubleParam(addr,  DanteInputCountRate,    pStats->ICR);
            setDoubleParam(addr,  DanteOutputCountRate,   pStats->OCR);
            setDoubleParam(addr,  DanteLastTimeStamp,     (double)pStats->last_timestamp);
            setIntegerParam(addr, DanteTriggers,          pStats->detected);
            setIntegerParam(addr, DanteEvents,            pStats->measured);
            setIntegerParam(addr, DanteFastDT,            pStats->edge_dt);
            setIntegerParam(addr, DanteFilt1DT,           pStats->filt1_dt);
            setIntegerParam(addr, DanteZeroCounts,        pStats->zerocounts);
            setIntegerParam(addr, DanteBaselinesValue,    pStats->baselines_value);
            setIntegerParam(addr, DantePupValue,          pStats->pup_value);
            setIntegerParam(addr, DantePupF1Value,        pStats->pup_f1_value);
            setIntegerParam(addr, DantePupNotF1Value,     pStats->pup_notf1_value);
            setIntegerParam(addr, DanteResetCounterValue, pStats->reset_counter_value);

            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                "%s::%s  board %d \n"
                "           real time=%f\n"
                "            livetime=%f\n"
                "    input count rate=%f\n"
                "   output count rate=%f\n"
                "     last time stamp=%f\n"
                "            triggers=%d\n"
                "              events=%d\n"
                "             fast dt=%d\n"
                "            filt1 dt=%d\n"
                "         zero counts=%d\n"
                "     baselines value=%d\n"
                "           pup value=%d\n"
                "        pup f1 value=%d\n"
                "     pup notf1 value=%d\n"
                " reset_counter_value=%d\n",
                driverName, functionName, addr,
                (double)pStats->real_time,
                (double)pStats->live_time,
                pStats->ICR,
                pStats->OCR,
                (double)pStats->last_timestamp,
                pStats->detected,
                pStats->measured,
                pStats->edge_dt,
                pStats->filt1_dt,
                pStats->zerocounts,
                pStats->baselines_value,
                pStats->pup_value,
                pStats->pup_f1_value,
                pStats->pup_notf1_value,
                pStats->reset_counter_value
            );
        }
    }
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return(asynSuccess);
}

asynStatus Dante::getMcaData(int addr)
{
    asynStatus status = asynSuccess;
    int arrayCallbacks;
    int nChannels;
    int board=addr;
    //NDArray *pArray;
    NDDataType_t dataType;
    epicsTimeStamp now;
    const char* functionName = "getMcaData";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == totalBoards_) board = ALL_BOARDS;

    getIntegerParam(mcaNumChannels, &nChannels);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getIntegerParam(NDDataType, (int *)&dataType);

    epicsTimeGetCurrent(&now);

    if (board == ALL_BOARDS) {  /* All boards */
        for (const auto& i: activeBoards_) {
            /* Call ourselves recursively but with a specific board */
            this->getMcaData(i);
        }
    } else {
        /* Read the MCA spectrum */
        uint32_t id, spectraSize = nChannels;
        getData(danteIdentifier_, addr, pMcaRaw_[addr], id, statistics_[addr], spectraSize);
        asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, (const char *)pMcaRaw_[addr], nChannels*sizeof(pMcaRaw_[0][0]),
            "%s::%s Got MCA spectrum board:%d ptr:%p\n",
            driverName, functionName, board, pMcaRaw_[addr]);

// In the future we may want to do array callbacks with the MCA data.  For now we are not doing this.
//        if (arrayCallbacks)
//       {
//            /* Allocate a buffer for callback */
//            pArray = this->pNDArrayPool->alloc(1, &nChannels, dataType, 0, NULL);
//            pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
//            pArray->uniqueId = spectrumCounter;
//            /* TODO: Need to copy the data here */
//            doCallbacksGenericPointer(pArray, NDArrayData, addr);
//            pArray->release();
//       }
    }
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}


/* Get trace data */
asynStatus Dante::getTraces()
{
    int iValue;
    uint16_t mode=0;
    uint32_t i;
    double traceTime;
    const char *functionName = "getTraces";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: enter\n",
        driverName, functionName);

    getDoubleParam(DanteTraceTime, &traceTime);
    // Convert from microseconds to decimation of 16 ns.
    uint16_t decRatio = (uint16_t)(traceTime/MIN_TRACE_TIME);
    if (decRatio < 1) decRatio = 1;
    if (decRatio > 32) decRatio = 32;
    setDoubleParam(DanteTraceTime, decRatio * MIN_TRACE_TIME);
    uint32_t triggerMask = 0;
    getIntegerParam(DanteTraceTriggerInstant, &iValue);
    if (iValue) triggerMask |= 1;
    getIntegerParam(DanteTraceTriggerRising, &iValue);
    if (iValue) triggerMask |= 2;
    getIntegerParam(DanteTraceTriggerFalling, &iValue);
    if (iValue) triggerMask |= 4;
    getIntegerParam(DanteTraceTriggerLevel, &iValue);
    uint32_t triggerLevel = iValue;
    double triggerWaitTime;
    getDoubleParam(DanteTraceTriggerWait, &triggerWaitTime);
    getIntegerParam(DanteTraceLength, &iValue);
    uint16_t length = iValue / TRACE_LEN_INC;
    if (length < 1) length = 1;
    setIntegerParam(DanteTraceLength, length * TRACE_LEN_INC);
    callParamCallbacks();
    callId_ = start_waveform(danteIdentifier_, mode, decRatio, triggerMask, triggerLevel, triggerWaitTime, length);
    waitReply(callId_, danteReply_, "start_waveform");
    while(1) {
        if (waveformAcquiring()) {
            epicsThreadSleep(0.001);
        } else {
            break;
        }
    }
    for (const auto& board: activeBoards_) {
        if (!getWaveData(danteIdentifier_, board, traceBuffer_, traceLength_)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s error calling getWaveData\n", driverName, functionName);
            return asynError;
        }
        // There is a bug in their firmware, the last waveform entry is always 0.
        // This messes up auto-scaling displays.  Replace it with the next to last value for now
        traceBuffer_[traceLength_-1] = traceBuffer_[traceLength_-2];
        for (i=0; i<traceLength_; i++) {
            traceBufferInt32_[i] = traceBuffer_[i];
        }
        doCallbacksInt32Array(traceBufferInt32_, traceLength_, DanteTraceData, board);
    }
    if (newTraceTime_) {
        double traceTime;
        getDoubleParam(0, DanteTraceTime, &traceTime);
        newTraceTime_ = false;
        for (i=0; i<traceLength_; i++) {
            traceTimeBuffer_[i] = i * traceTime;
        }
        doCallbacksFloat64Array(traceTimeBuffer_, traceLength_, DanteTraceTimeArray, 0);
    }
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return asynSuccess;
}

asynStatus Dante::startAcquiring()
{
    asynStatus status = asynSuccess;
    int acquiring, erased;
    const char *functionName = "startAcquiring";

    getIntegerParam(mcaAcquiring, &acquiring);
    getIntegerParam(DanteErased, &erased);

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s acquiring=%d, erased=%d\n",
        driverName, functionName, acquiring, erased);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    double presetReal;
    int numChannels;
    int collectMode;
    int mappingPoints;
    uint32_t msTime;

    getDoubleParam(mcaPresetRealTime, &presetReal);
    getIntegerParam(mcaNumChannels, &numChannels);
    getIntegerParam(DanteCollectMode, &collectMode);
    getIntegerParam(DanteMappingPoints, &mappingPoints);
    switch (collectMode){
      case DanteModeMCA:
        callId_ = start(danteIdentifier_, presetReal, numChannels);
        waitReply(callId_, danteReply_, "start");
        break;
      case DanteModeMCAMapping:
        setIntegerParam(DanteCurrentPixel, 0);
        msTime = (uint32_t)(presetReal * 1000);
asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s calling start_map(), msTime=%u, mappingPoints=%d, numChannels=%d\n",
driverName, functionName, msTime, mappingPoints, numChannels);
        callId_ = start_map(danteIdentifier_, msTime, (uint32_t)mappingPoints, numChannels);
        waitReply(callId_, danteReply_, "start_map");
        break;
      case DanteModeList:
        setIntegerParam(DanteCurrentPixel, 0);
        callId_ = start_list(danteIdentifier_, presetReal);
        waitReply(callId_, danteReply_, "start_list");
        break;
    }

    setIntegerParam(DanteErased, 0); /* reset the erased flag */
    setIntegerParam(mcaAcquiring, 1); /* Set the acquiring flag */

    callParamCallbacks();

    // signal cmdStartEvent to start the polling thread
    cmdStartEvent_->signal();
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

/** Thread used to poll the hardware for status and data.
 *
 */
void Dante::acquisitionTask()
{
    int paramStatus;
    int mode;
    int acquiring = 0;
    epicsFloat64 pollTime, sleepTime, dtmp;
    epicsTimeStamp now, start;
    const char* functionName = "acquisitionTask";

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
        "%s:%s acquisition task started!\n",
        driverName, functionName);

    lock();

    while (polling_) /* ... round and round and round we go until the IOC is shut down */
    {

        getIntegerParam(totalBoards_, DanteAcquiring, &acquiring);
        if (!acquiring)
        {
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                "%s:%s Waiting for acquisition to start!\n",
                driverName, functionName);
            cmdStartEvent_->wait();
            lock();
            getIntegerParam(DanteCollectMode, &mode);
            asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
                "%s::%s [%s]: started! (mode=%d)\n",
                driverName, functionName, this->portName, mode);
        }
        epicsTimeGetCurrent(&start);

        /* In this loop we only read the acquisition status to minimise overhead.
         * If a transition from acquiring to done is detected then we read the statistics
         * and the data. */
        acquiring = dataAcquiring();
        if (mode == DanteModeMCAMapping) {
            pollMCAMappingMode();
        }
        else if (mode == DanteModeList) {
            pollListMode();
        }
        if (!acquiring)
        {
            /* There must have just been a transition from acquiring to not acquiring */
           if (mode == DanteModeMCA) {
                /* In MCA mode we force a read of the data */
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s::%s Detected acquisition stop! Now reading statistics\n",
                    driverName, functionName);
                getMcaData(ALL_BOARDS);
                getAcquisitionStatistics(ALL_BOARDS);
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s::%s Detected acquisition stop! Now reading data\n",
                    driverName, functionName);
            }
            else if (mode == DanteModeList) {
                /* In List mode we need to call stop() */
                callId_ = stop(danteIdentifier_);
                waitReply(callId_, danteReply_, "stop");
            }
        }

        /* Do callbacks for all boards for everything except mcaAcquiring*/
        for (const auto& i: activeBoards_) callParamCallbacks(i);
        /* Copy internal acquiring flag to mcaAcquiring */
        for (const auto& i: activeBoards_) {
            getIntegerParam(i, DanteAcquiring, &acquiring);
            setIntegerParam(i, mcaAcquiring, acquiring);
            callParamCallbacks(i);
        }
        setIntegerParam(ADAcquire, acquiring);

        paramStatus |= getDoubleParam(DantePollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleepTime = pollTime - dtmp;
        if (sleepTime < 0) sleepTime = 0.001;
        this->unlock();
        epicsThreadSleep(sleepTime);
        this->lock();
    }
}

/** Check if there are any spectra to be read and read them */
asynStatus Dante::pollMCAMappingMode()
{
    int collectMode;
    asynStatus status = asynSuccess;
    uint32_t numAvailable=0;
    int numMCAChannels;
    int arrayCallbacks;
    const char* functionName = "pollMCAMappingMode";

    getIntegerParam(DanteCollectMode, &collectMode);
    getIntegerParam(mcaNumChannels, &numMCAChannels);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);

    // First see which board has fewest spectra available
    for (const auto& board: activeBoards_) {
        uint32_t itemp;
        if (!getAvailableData(danteIdentifier_, board, itemp)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s error calling getAvailableData\n", driverName, functionName);
            return asynError;
        }
asynPrint(pasynUserSelf, ASYN_TRACE_WARNING, "%s::%s::getAvailableData(): board=%d, numSpectra=%d\n", driverName, functionName, board, itemp);
        if (board == activeBoards_[0]) {
            numAvailable = itemp;
        } else if (itemp < numAvailable) {
            numAvailable = itemp;
        }
    }
    if (numAvailable == 0) return asynSuccess;

    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);
    uint32_t spectraSize = numMCAChannels;

    // Now read the same number of spectra from each board
    for (const auto& board: activeBoards_) {
        pMappingMCAData_   [board] = (uint16_t *)       malloc(numAvailable * numMCAChannels * sizeof(uint16_t));
        pMappingSpectrumId_[board] = (uint32_t *)       malloc(numAvailable * sizeof(uint32_t));
        pMappingStats_     [board] = (mappingStats *)   malloc(numAvailable * sizeof(mappingStats));
        pMappingAdvStats_  [board] = (mappingAdvStats *)malloc(numAvailable * sizeof(mappingAdvStats));
        if (!getAllData(danteIdentifier_, board, pMappingMCAData_[board], pMappingSpectrumId_[board],
                        (double *)pMappingStats_[board], (uint64_t*)pMappingAdvStats_[board], spectraSize, numAvailable)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s error calling getAllData\n", driverName, functionName);
            status = asynError;
            goto done;
        }
    }
    if (arrayCallbacks) {
        size_t dims[2];
        dims[0] = numMCAChannels;
        dims[1] = activeBoards_.size();
        NDArray *pArray;
        for (uint32_t pixel=0; pixel<numAvailable; pixel++) {
            pArray = this->pNDArrayPool->alloc(2, dims, NDUInt16, 0, NULL );
            epicsUInt16 *pOut = (epicsUInt16 *)pArray->pData;
            for (const auto& board: activeBoards_) {
                char tempString[20];
                uint16_t *pIn = pMappingMCAData_[board] + numMCAChannels * pixel;
                memcpy(pOut, pIn, numMCAChannels * sizeof(epicsUInt16));
                pOut += numMCAChannels;
                mappingStats *pStats = pMappingStats_[board] + pixel;
                double realTime = pStats->real_time/1.e6;
                sprintf(tempString, "RealTime_%d", board);
                pArray->pAttributeList->add(tempString, "Real time",         NDAttrFloat64, &realTime);
                double liveTime = pStats->live_time/1.e6;
                sprintf(tempString, "LiveTime_%d", board);
                pArray->pAttributeList->add(tempString, "Live time",         NDAttrFloat64, &liveTime);
                double ICR = pStats->ICR;
                sprintf(tempString, "ICR_%d", board);
                pArray->pAttributeList->add(tempString, "Input count rate",  NDAttrFloat64, &ICR);
                double OCR = pStats->OCR;
                sprintf(tempString, "OCR_%d", board);
                pArray->pAttributeList->add(tempString, "Output count rate", NDAttrFloat64, &OCR);
            }
            pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
            updateTimeStamp(&pArray->epicsTS);
            pArray->uniqueId = uniqueId_;
            uniqueId_++;
            int arrayCounter;
            getIntegerParam(NDArrayCounter, &arrayCounter);
            arrayCounter++;
            setIntegerParam(NDArrayCounter, arrayCounter);
            getAttributes(pArray->pAttributeList);
            doCallbacksGenericPointer(pArray, NDArrayData, 0);
            pArray->release();
        }
    }

    // Copy the spectral data for the first pixel in this buffer to the mcaRaw buffers.
    // This provides an update of the spectra and statistics while mapping is in progress
    // if the user sets the MCA spectra to periodically read.
    for (const auto& board: activeBoards_) {
        uint16_t *pIn = pMappingMCAData_[board];
        uint64_t *pOut = pMcaRaw_[board];
        for (int chan=0; chan<numMCAChannels; chan++) {
            pOut[chan] = pIn[chan];
        }
        mappingStats *pStats = pMappingStats_[board];
        mappingAdvStats *pAdvStats = pMappingAdvStats_[board];
        setDoubleParam(board, mcaElapsedRealTime,   pStats->real_time/1.e6);
        setDoubleParam(board, mcaElapsedLiveTime,   pStats->live_time/1.e6);
        setDoubleParam(board, DanteInputCountRate,  pStats->ICR);
        setDoubleParam(board, DanteOutputCountRate, pStats->OCR);
        setDoubleParam(board, DanteLastTimeStamp,   (double)pAdvStats->last_timestamp);
        setIntegerParam(board, DanteEvents,         (int)pAdvStats->detected);
        setIntegerParam(board, DanteTriggers,       (int)pAdvStats->measured);
        setIntegerParam(board, DanteFastDT,         (int)pAdvStats->edge_dt);
        setIntegerParam(board, DanteFilt1DT,        (int)pAdvStats->filt1_dt);
        setIntegerParam(board, DanteZeroCounts,     (int)pAdvStats->zerocounts);
        setIntegerParam(board, DanteBaselinesValue, (int)pAdvStats->baselines_value);
        setIntegerParam(board, DantePupValue,       (int)pAdvStats->pup_value);
        setIntegerParam(board, DantePupF1Value,     (int)pAdvStats->pup_f1_value);
        setIntegerParam(board, DantePupNotF1Value,  (int)pAdvStats->pup_notf1_value);
        setIntegerParam(board, DanteResetCounterValue, (int)pAdvStats->reset_counter_value);
        callParamCallbacks(board);
    }
    done:
    int currentPixel;
    getIntegerParam(DanteCurrentPixel, &currentPixel);
    currentPixel += numAvailable;
    setIntegerParam(DanteCurrentPixel, currentPixel);
    for (const auto& board: activeBoards_) {
        free(pMappingMCAData_[board]);
        free(pMappingSpectrumId_[board]);
        free(pMappingStats_[board]);
        free(pMappingAdvStats_[board]);
    }
    return status;
}

/** Check if there are enough events to be read and read them */
asynStatus Dante::pollListMode()
{
    int collectMode;
    asynStatus status = asynSuccess;
    uint32_t maxAvailable = 0;
    uint64_t totalEvents = 0;
    int listBufferSize;
    int arrayCallbacks;
    int acquiring;
    int numMCAChannels;
    int currentPixel;
    uint32_t spectrumId;
    epicsTimeStamp now;
    const char* functionName = "pollListMode";

    getIntegerParam(DanteCollectMode, &collectMode);
    getIntegerParam(DanteAcquiring, &acquiring);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getIntegerParam(DanteListBufferSize, &listBufferSize);
    getIntegerParam(mcaNumChannels, &numMCAChannels);
    getIntegerParam(DanteCurrentPixel, &currentPixel);

    // First see which board has most events available
    for (const auto& board: activeBoards_) {
        uint32_t itemp;
        if (!getAvailableData(danteIdentifier_, board, itemp)) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s error calling getAvailableData\n", driverName, functionName);
            return asynError;
        }
        itemp = std::min((int)itemp, listBufferSize);
        totalEvents += itemp;
        numEventsAvailable_[board] = itemp;
        if (board == activeBoards_[0]) {
            maxAvailable = itemp;
        } else if (itemp > maxAvailable) {
            maxAvailable = itemp;
        }
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            "%s::%s board=%d numEventsAvailable=%d\n",
            driverName, functionName, board, numEventsAvailable_[board]);
    }

    // If we are acquiring and have not yet filled a buffer return.
    if (((int)maxAvailable < listBufferSize)  && acquiring) {
        return asynSuccess;
    }

    epicsTimeGetCurrent(&now);

    // Now read the events from each board
    for (const auto& board: activeBoards_) {
        pListData_[board] = (uint64_t *) calloc(listBufferSize, sizeof(uint64_t));
        if (numEventsAvailable_[board] > 0) {
            if (!getData(danteIdentifier_, board, pListData_[board], spectrumId, statistics_[board], numEventsAvailable_[board])) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s error calling getData\n", driverName, functionName);
                status = asynError;
                goto done;
            }
        }
    }

    if (arrayCallbacks) {
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s doing array callbacks\n", driverName, functionName);
        size_t dims[2];
        dims[0] = listBufferSize;
        dims[1] = activeBoards_.size();
        NDArray *pArray;
        pArray = this->pNDArrayPool->alloc(2, dims, NDUInt64, 0, NULL );
        epicsUInt64 *pOut = (epicsUInt64 *)pArray->pData;
        for (const auto& board: activeBoards_) {
            char tempString[20];
            uint64_t *pIn = pListData_[board];
            memcpy(pOut, pIn, dims[0]*sizeof(epicsUInt64));
            statistics *pStats = &statistics_[board];
            double realTime = pStats->real_time/1.e6;
            sprintf(tempString, "RealTime_%d", board);
            pArray->pAttributeList->add(tempString, "Real time",         NDAttrFloat64, &realTime);
            double liveTime = pStats->live_time/1.e6;
            sprintf(tempString, "LiveTime_%d", board);
            pArray->pAttributeList->add(tempString, "Live time",         NDAttrFloat64, &liveTime);
            double ICR = pStats->ICR;
            sprintf(tempString, "ICR_%d", board);
            pArray->pAttributeList->add(tempString, "Input count rate",  NDAttrFloat64, &ICR);
            double OCR = pStats->OCR;
            sprintf(tempString, "OCR_%d", board);
            pArray->pAttributeList->add(tempString, "Output count rate", NDAttrFloat64, &OCR);
        }
        pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
        updateTimeStamp(&pArray->epicsTS);
        pArray->uniqueId = uniqueId_;
        uniqueId_++;
        int arrayCounter;
        getIntegerParam(NDArrayCounter, &arrayCounter);
        arrayCounter++;
        setIntegerParam(NDArrayCounter, arrayCounter);
        getAttributes(pArray->pAttributeList);
        doCallbacksGenericPointer(pArray, NDArrayData, 0);
        pArray->release();
    }

    // Copy the spectral data for the first pixel in this buffer to the mcaRaw buffers.
    // This provides an update of the spectra and statistics while mapping is in progress
    // if the user sets the MCA spectra to periodically read.
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER, "%s::%s copying spectrum data\n", driverName, functionName);
    for (const auto& board: activeBoards_) {
        uint64_t *pIn = pListData_[board];
        uint64_t *pOut = pMcaRaw_[board];
        memset(pOut, 0, numMCAChannels*sizeof(*pOut));
        for (int chan=0; ((chan<numMCAChannels) && (chan<(int)numEventsAvailable_[board])); chan++) {
            pOut[chan] = pIn[chan] & 0xffff;
        }
        statistics *pStats = &statistics_[board];
        setDoubleParam(board, mcaElapsedRealTime,      pStats->real_time/1.e6);
        setDoubleParam(board, mcaElapsedLiveTime,      pStats->live_time/1.e6);
        setDoubleParam(board, DanteInputCountRate,     pStats->ICR);
        setDoubleParam(board, DanteOutputCountRate,    pStats->OCR);
        setDoubleParam(board, DanteLastTimeStamp,      (double)pStats->last_timestamp);
        setIntegerParam(board, DanteEvents,            pStats->detected);
        setIntegerParam(board, DanteTriggers,          pStats->measured);
        setIntegerParam(board, DanteFastDT,            pStats->edge_dt);
        setIntegerParam(board, DanteFilt1DT,           pStats->filt1_dt);
        setIntegerParam(board, DanteZeroCounts,        pStats->zerocounts);
        setIntegerParam(board, DanteBaselinesValue,    pStats->baselines_value);
        setIntegerParam(board, DantePupValue,          pStats->pup_value);
        setIntegerParam(board, DantePupF1Value,        pStats->pup_f1_value);
        setIntegerParam(board, DantePupNotF1Value,     pStats->pup_notf1_value);
        setIntegerParam(board, DanteResetCounterValue, pStats->reset_counter_value);
        callParamCallbacks(board);
    }

    currentPixel += (int)totalEvents;
    setIntegerParam(DanteCurrentPixel, currentPixel);
    done:
    for (const auto& board: activeBoards_) {
        free(pListData_[board]);
    }
    return status;
}


void Dante::report(FILE *fp, int details)
{
    if (details > 0) {
        for (const auto& i: activeBoards_) {
            fprintf(fp, "Configuration %d:\n", i);
            configuration *pConfig = &configurations_[i];
            fprintf(fp, "      fast_filter_thr: %d\n", pConfig->fast_filter_thr);
            fprintf(fp, "    energy_filter_thr: %d\n", pConfig->energy_filter_thr);
            fprintf(fp, "  energy_baseline_thr: %d\n", pConfig->energy_baseline_thr);
            fprintf(fp, "         max_risetime: %f\n", pConfig->max_risetime);
            fprintf(fp, "                 gain: %f\n", pConfig->gain);
            fprintf(fp, "         peaking_time: %d\n", pConfig->peaking_time);
            fprintf(fp, "     max_peaking_time: %d\n", pConfig->max_peaking_time);
            fprintf(fp, "             flat_top: %d\n", pConfig->flat_top);
            fprintf(fp, "    fast_peaking_time: %d\n", pConfig->edge_peaking_time);
            fprintf(fp, "        fast_flat_top: %d\n", pConfig->edge_flat_top);
            fprintf(fp, "  reset_recovery_time: %d\n", pConfig->reset_recovery_time);
            fprintf(fp, "       zero_peak_freq: %f\n", pConfig->zero_peak_freq);
            fprintf(fp, "     baseline_samples: %d\n", pConfig->baseline_samples);
            fprintf(fp, "       inverted_input: %d\n", pConfig->inverted_input ? 1 : 0);
            fprintf(fp, "        time_constant: %f\n", pConfig->time_constant);
            fprintf(fp, "          base_offset: %d\n", pConfig->base_offset);
            fprintf(fp, "      reset_threshold: %d\n", pConfig->reset_threshold);
            fprintf(fp, "Statistics %d:\n", i);
            statistics *pStats = &statistics_[i];
            fprintf(fp, "            real_time: %f\n", (double)pStats->real_time);
            fprintf(fp, "            live_time: %f\n", (double)pStats->live_time);
            fprintf(fp, "                  ICR: %f\n", pStats->ICR);
            fprintf(fp, "                  OCR: %f\n", pStats->OCR);
            fprintf(fp, "       last_timestamp: %f\n", (double)pStats->last_timestamp);
            fprintf(fp, "             detected: %d\n", pStats->detected);
            fprintf(fp, "             measured: %d\n", pStats->measured);
            fprintf(fp, "              edge_dt: %d\n", pStats->edge_dt);
            fprintf(fp, "             filt1_dt: %d\n", pStats->filt1_dt);
            fprintf(fp, "           zerocounts: %d\n", pStats->zerocounts);
            fprintf(fp, "      baselines_value: %d\n", pStats->baselines_value);
            fprintf(fp, "            pup_value: %d\n", pStats->pup_value);
            fprintf(fp, "         pup_f1_value: %d\n", pStats->pup_f1_value);
            fprintf(fp, "      pup_notf1_value: %d\n", pStats->pup_notf1_value);
            fprintf(fp, "  reset_counter_value: %d\n", pStats->reset_counter_value);
        }
    }
    asynNDArrayDriver::report(fp, details);
}


void Dante::shutdown()
{
    double pollTime;
    static const char *functionName = "shutdown";

    getDoubleParam(DantePollTime, &pollTime);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s shutting down in %f seconds\n", driverName, functionName, 2*pollTime);
    polling_ = 0;
    epicsThreadSleep(2*pollTime);

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s calling clear_chain\n", driverName, functionName);
    clear_chain(danteIdentifier_);
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s calling CloseLibrary\n", driverName, functionName);
    if (CloseLibrary()) {
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, "%s::%s called CloseLibrary successfully\n", driverName, functionName);
        return;
    }
    else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, "%s::%s error calling CloseLibrary()\n", driverName, functionName);
        return;
    }
}


static const iocshArg DanteConfigArg0 = {"Asyn port name", iocshArgString};
static const iocshArg DanteConfigArg1 = {"IP address", iocshArgString};
static const iocshArg DanteConfigArg2 = {"Number of boards", iocshArgInt};
static const iocshArg DanteConfigArg3 = {"Maximum amount of memory (bytes)", iocshArgInt};
static const iocshArg * const DanteConfigArgs[] =  {&DanteConfigArg0,
                                                    &DanteConfigArg1,
                                                    &DanteConfigArg2,
                                                    &DanteConfigArg3};
static const iocshFuncDef configDante = {"DanteConfig", 4, DanteConfigArgs};
static void configDanteCallFunc(const iocshArgBuf *args)
{
    DanteConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival);
}

static void mcaDanteRegister(void)
{
    iocshRegister(&configDante, configDanteCallFunc);
}

extern "C" {
epicsExportRegistrar(mcaDanteRegister);
}
