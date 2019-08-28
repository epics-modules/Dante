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

#define MAX_CHANNELS_PER_CARD      8
#define MAX_MCA_BINS            4096
#define ALL_CHANNELS              -1
#define MAX_MESSAGE_DATA          20
#define MSG_QUEUE_SIZE            50
#define MESSAGE_TIMEOUT           10.

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
    free(pDante);
}

static void acquisitionTaskC(void *drvPvt)
{
    Dante *pDante = (Dante *)drvPvt;
    pDante->acquisitionTask();
}

extern "C" int DanteConfig(const char *portName, const char *ipAddress, int nChannels,
                           int maxBuffers, size_t maxMemory)
{
    new Dante(portName, ipAddress, nChannels, maxBuffers, maxMemory);
    return 0;
}

/* Note: we use nChannels+1 for maxAddr because the last address is used for "all" channels" */
Dante::Dante(const char *portName, const char *ipAddress, int nChannels, int maxBuffers, size_t maxMemory)
    : asynNDArrayDriver(portName, nChannels + 1, maxBuffers, maxMemory,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask | asynDrvUserMask,
            asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynFloat64ArrayMask | asynGenericPointerMask | asynOctetMask,
            ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0)
{
    int status = asynSuccess;
    int i, ch;
    struct configuration config;
    struct statistics stats;
    const char *functionName = "Dante";

    nChannels_ = nChannels;
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
  	}	else {
	      printf("%s::%s library version=%s\n", driverName, functionName, libraryVersion);
  	}

    if (!add_to_query((char *)ipAddress)) {
	      printf("%s::%s error calling add_to_query\n", driverName, functionName);
	      return;
  	}	else {
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

    /* General parameters */
    createParam(DanteCollectModeString,            asynParamInt32,   &DanteCollectMode);
    createParam(DanteCurrentPixelString,           asynParamInt32,   &DanteCurrentPixel);
    createParam(DanteReadRateString,               asynParamFloat64, &DanteReadRate);
    createParam(DanteMaxEnergyString,              asynParamFloat64, &DanteMaxEnergy);
    createParam(DanteSpectrumXAxisString,          asynParamFloat64Array, &DanteSpectrumXAxis);

    /* Internal asyn driver parameters */
    createParam(DanteErasedString,                 asynParamInt32,   &DanteErased);
    createParam(DanteAcquiringString,              asynParamInt32,   &DanteAcquiring);
    createParam(DantePollTimeString,               asynParamFloat64, &DantePollTime);
    createParam(DanteForceReadString,              asynParamInt32,   &DanteForceRead);

    /* Diagnostic trace parameters */
    createParam(DanteTraceDataString,              asynParamInt32Array, &DanteTraceData);
    createParam(DanteTraceTimeArrayString,         asynParamFloat64Array, &DanteTraceTimeArray);

    /* Runtime statistics */
    createParam(DanteInputCountRateString,         asynParamFloat64, &DanteInputCountRate);
    createParam(DanteOutputCountRateString,        asynParamFloat64, &DanteOutputCountRate);
    createParam(DanteLastTimeStampString,          asynParamFloat64, &DanteLastTimeStamp);
    createParam(DanteTriggersString,               asynParamInt32,   &DanteTriggers);
    createParam(DanteEventsString,                 asynParamInt32,   &DanteEvents);
    createParam(DanteEdgeDTString,                 asynParamInt32,   &DanteEdgeDT);
    createParam(DanteFilt1DTString,                asynParamInt32,   &DanteFilt1DT);
    createParam(DanteZeroCountsString,             asynParamInt32,   &DanteZeroCounts);
    createParam(DanteBaselinesValueString,         asynParamInt32,   &DanteBaselinesValue);
    createParam(DantePupValueString,               asynParamInt32,   &DantePupValue);
    createParam(DantePupF1ValueString,             asynParamInt32,   &DantePupF1Value);
    createParam(DantePupNotF1ValueString,          asynParamInt32,   &DantePupNotF1Value);
    createParam(DanteResetCounterValueString,      asynParamInt32,   &DanteResetCounterValue);

    /* Configuration parameters */
    createParam(DanteFastFilterThresholdString,     asynParamFloat64, &DanteFastFilterThreshold);
    createParam(DanteEnergyFilterThresholdString,   asynParamFloat64, &DanteEnergyFilterThreshold);
    createParam(DanteEnergyBaselineThresholdString, asynParamFloat64, &DanteEnergyBaselineThreshold);
    createParam(DanteMaxRiseTimeString,             asynParamFloat64, &DanteMaxRiseTime);
    createParam(DanteGainString,                    asynParamFloat64, &DanteGain);
    createParam(DantePeakingTimeString,             asynParamFloat64, &DantePeakingTime);
    createParam(DanteMaxPeakingTimeString,          asynParamFloat64, &DanteMaxPeakingTime);
    createParam(DanteFlatTopString,                 asynParamFloat64, &DanteFlatTop);
    createParam(DanteEdgePeakingTimeString,         asynParamFloat64, &DanteEdgePeakingTime);
    createParam(DanteEdgeFlatTopString,             asynParamFloat64, &DanteEdgeFlatTop);
    createParam(DanteResetRecoveryTimeString,       asynParamFloat64, &DanteResetRecoveryTime);
    createParam(DanteZeroPeakFreqString,            asynParamFloat64, &DanteZeroPeakFreq);
    createParam(DanteBaselineSamplesString,         asynParamInt32,   &DanteBaselineSamples);
    createParam(DanteInvertedInputString,           asynParamInt32,   &DanteInvertedInput);
    createParam(DanteTimeConstantString,            asynParamFloat64, &DanteTimeConstant);
    createParam(DanteBaseOffsetString,              asynParamInt32,   &DanteBaseOffset);
    createParam(DanteOverflowRecoveryString,        asynParamFloat64, &DanteOverflowRecovery);
    createParam(DanteResetThresholdString,          asynParamInt32,   &DanteResetThreshold);
    createParam(DanteTailCoefficientString,         asynParamFloat64, &DanteTailCoefficient);

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
    for (i=0; i<=nChannels_; i++) {
        setIntegerParam(i, mcaAcquiring, 0);
    }

    /* Create the start and stop events that will be used to signal our
     * acquisitionTask when to start/stop polling the HW     */
    cmdStartEvent_ = new epicsEvent();
    cmdStopEvent_ = new epicsEvent();
    stoppedEvent_ = new epicsEvent();
    msgQ_ = new epicsMessageQueue(MSG_QUEUE_SIZE, MESSAGE_SIZE);

    /* Allocate a memory pointer for each of the channels */
    pMcaRaw_ = (unsigned long**) calloc(nChannels_, sizeof(unsigned long*));
    /* Allocate a memory area for each spectrum */
    for (ch=0; ch<nChannels_; ch++) {
        pMcaRaw_[ch] = (unsigned long*)calloc(MAX_MCA_BINS, sizeof(unsigned long));
    }
    
    // Allocate configuration and statistics buffers
    for (i=0; i<nChannels_; i++) {
        configurations_.push_back(config);
        statistics_.push_back(stats);
    }

    /* Allocate a buffer for the trace data */
    traceLength_ = 4096;
    traceBuffer_ = (epicsInt32 *)malloc(traceLength_ * sizeof(epicsInt32));

    /* Allocate a buffer for the trace time array */
    traceTimeBuffer_ = (epicsFloat64 *)malloc(traceLength_ * sizeof(epicsFloat64));
    
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

    /* Set default values for parameters that cannot be read from Handel */
    for (i=0; i<=nChannels_; i++) {
        setIntegerParam(i, DanteForceRead,                  0);
        setDoubleParam (i, mcaPresetCounts,               0.0);
        setDoubleParam (i, mcaElapsedCounts,              0.0);
        setDoubleParam (i, mcaPresetRealTime,             0.0);
        setDoubleParam (i, mcaPresetRealTime,             0.0);
        setIntegerParam(i, DanteCurrentPixel,             0);
        setIntegerParam(i, mcaNumChannels,                2048);
        setDoubleParam (i, DanteFastFilterThreshold,      0.1);
        setDoubleParam (i, DanteEnergyFilterThreshold,    0.0);
        setDoubleParam (i, DanteEnergyBaselineThreshold,  0.0);
        setDoubleParam (i, DanteMaxRiseTime,              0.25);
        setDoubleParam (i, DanteGain,                     1.0);
        setDoubleParam (i, DantePeakingTime,              1.0);
        setDoubleParam (i, DanteMaxPeakingTime,           0.0);
        setDoubleParam (i, DanteFlatTop,                  0.05);
        setDoubleParam (i, DanteEdgePeakingTime,          0.03);
        setDoubleParam (i, DanteEdgeFlatTop,              0.01);
        setDoubleParam (i, DanteResetRecoveryTime,        0.25);
        setDoubleParam (i, DanteZeroPeakFreq,             1000.);
        setIntegerParam(i, DanteBaselineSamples,          64);
        setIntegerParam(i, DanteInvertedInput,            0);
        setDoubleParam (i, DanteTimeConstant,             0.0);
        setIntegerParam(i, DanteBaseOffset,               0);
        setDoubleParam (i, DanteOverflowRecovery,         0.0);
        setIntegerParam(i, DanteResetThreshold,           0);
        setDoubleParam (i, DanteTailCoefficient,          0.0);
    }

    /* Read the MCA and DXP parameters once */
    this->getAcquisitionStatus(this->pasynUserSelf, ALL_CHANNELS);
    this->getAcquisitionStatistics(this->pasynUserSelf, ALL_CHANNELS);
    
    // Enable array callbacks by default
    setIntegerParam(NDArrayCallbacks, 1);

}

asynStatus Dante::waitReply(uint32_t callId, char *reply) {
    struct danteMessage message;
    int numRecv;
    static const char *functionName = "waitReply";

    numRecv = msgQ_->receive(&message, sizeof(message), MESSAGE_TIMEOUT);
    if (numRecv != sizeof(message)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error receiving message numRecv=%d\n", driverName, functionName, numRecv);
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
msgQ_->show(10);
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
    int channel;
    int addr, i;
    int acquiring, numChans, mode;
    const char* functionName = "writeInt32";

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: [%s]: function=%d value=%d addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

    /* Set the parameter and readback in the parameter library.  This may be overwritten later but that's OK */
    status = setIntegerParam(addr, function, value);

    if (function == DanteCollectMode)
    {
        status = this->configureCollectMode();
    } 
    else if (function == mcaErase) 
    {
        getIntegerParam(addr, mcaNumChannels, &numChans);
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (acquiring) {
            callId_ = stop(danteIdentifier_);
            waitReply(callId_, danteReply_);
            startAcquiring(pasynUser);
        } else {
            setIntegerParam(addr, DanteErased, 1);
            if (channel == ALL_CHANNELS) {
                for (i=0; i<nChannels_; i++) {
                    setIntegerParam(i, DanteErased, 1);
                    memset(this->pMcaRaw_[i], 0, numChans * sizeof(pMcaRaw_[0][0]));
                }
            } else {
                memset(this->pMcaRaw_[addr], 0, numChans * sizeof(pMcaRaw_[0][0]));
            }
            /* Need to call getAcquisitionStatistics to set elapsed values to 0 */
            this->getAcquisitionStatistics(pasynUser, addr);
        }
    } 
    else if (function == mcaStartAcquire) 
    {
        status = this->startAcquiring(pasynUser);
    } 
    else if (function == mcaStopAcquire) 
    {
        callId_ = stop(danteIdentifier_);
        waitReply(callId_, danteReply_);
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
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s mcaReadStatus [%d] mode=%d\n", 
            driverName, functionName, function, mode);
        /* We let the polling task set the acquiring flag, so that we can be sure that
         * the statistics and data have been read when needed.  DON'T READ ACQUIRE STATUS HERE */
        getIntegerParam(addr, mcaAcquiring, &acquiring);
        if (mode == DanteModeMCA) {
            /* If we are acquiring then read the statistics, else we use the cached values */
            if (acquiring) status = this->getAcquisitionStatistics(pasynUser, addr);
        }
    }
    else if (
        (function == DanteBaselineSamples) ||
        (function == DanteInvertedInput) ||
        (function == DanteBaseOffset) ||
        (function == DanteResetThreshold))
    {
        this->setDanteConfiguration(pasynUser, addr);
    }

    /* Call the callback */
    callParamCallbacks(addr, addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

asynStatus Dante::writeFloat64( asynUser *pasynUser, epicsFloat64 value)
{
    asynStatus status = asynSuccess;
    int function = pasynUser->reason;
    int addr;
    int channel;
    const char *functionName = "writeFloat64";

    channel = this->getChannel(pasynUser, &addr);
    asynPrint(pasynUser, ASYN_TRACE_FLOW,
        "%s:%s: [%s]: function=%d value=%f addr=%d channel=%d\n",
        driverName, functionName, this->portName, function, value, addr, channel);

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
    }

    if ((function == DanteMaxEnergy) ||
        (function == DanteFastFilterThreshold) ||
        (function == DanteEnergyFilterThreshold) ||
        (function == DanteEnergyBaselineThreshold) ||
        (function == DanteMaxRiseTime) ||
        (function == DanteGain) ||
        (function == DantePeakingTime) ||
        (function == DanteMaxPeakingTime) ||
        (function == DanteFlatTop) ||
        (function == DanteEdgePeakingTime) ||
        (function == DanteEdgeFlatTop) ||
        (function == DanteResetRecoveryTime) ||
        (function == DanteZeroPeakFreq) ||
        (function == DanteTimeConstant) ||
        (function == DanteOverflowRecovery) ||
        (function == DanteTailCoefficient))
    {
        this->setDanteConfiguration(pasynUser, addr);
    }
    /* Call the callback */
    callParamCallbacks(addr, addr);

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
    int channel;
    int nBins, acquiring,mode;
    int ch;
    int i;
    const char *functionName = "readInt32Array";

    channel = this->getChannel(pasynUser, &addr);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d function=%d\n",
        driverName, functionName, addr, channel, function);
    if (function == DanteTraceData) 
    {
        status = this->getTrace(pasynUser, channel, value, nElements, nIn);
    } 
    else if (function == mcaData) 
    {
        if (channel == ALL_CHANNELS)
        {
            // if the MCA ALL channel is being read - force reading of all individual
            // channels using the DanteForceRead command.
            for (ch=0; ch<nChannels_; ch++)
            {
                setIntegerParam(ch, DanteForceRead, 1);
                callParamCallbacks(ch, ch);
                setIntegerParam(ch, DanteForceRead, 0);
                callParamCallbacks(ch, ch);
            }
            goto done;
        }
        getIntegerParam(channel, mcaNumChannels, &nBins);
        if (nBins > (int)nElements) nBins = (int)nElements;
        getIntegerParam(channel, mcaAcquiring, &acquiring);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s getting mcaData. ch=%d mcaNumChannels=%d mcaAcquiring=%d\n",
            driverName, functionName, channel, nBins, acquiring);
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
                this->getMcaData(pasynUser, addr);
            } else if (mode == DanteModeSpectraMapping)
            {
                /*  Nothing needed here, the last data read from the mapping buffer has already been
                 *  copied to the buffer pointed to by pMcaRaw_. */
            }
        }
        for (i=0; i<nBins; i++) {
            value[i] = pMcaRaw_[addr][i];
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

int Dante::getChannel(asynUser *pasynUser, int *addr)
{
    int channel;
    pasynManager->getAddr(pasynUser, addr);

    channel = *addr;
    if (*addr == nChannels_) channel = ALL_CHANNELS;
    return channel;
}

asynStatus Dante::setDanteConfiguration(asynUser *pasynUser, int addr)
{
    double maxEnergy;
    double dValue;
    int ivalue;
    double mcaBinWidth;
    double usecToFastSample = 1e-6/8e-9;
    double usecToSlowSample = 1e-6/32e-9;
    int numChannels;
    static const char *functionName = "setDanteParam";

    // First do the float64 parameters
    getDoubleParam(addr, DanteMaxEnergy, &maxEnergy);
    getIntegerParam(addr, mcaNumChannels, &numChannels);
    mcaBinWidth = maxEnergy/numChannels;
    
    configuration *pConfig = &configurations_[addr];
    
    getDoubleParam(addr, DanteFastFilterThreshold, &dValue);
    pConfig->fast_filter_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteFastFilterThreshold, pConfig->fast_filter_thr * mcaBinWidth);
    
    getDoubleParam(addr, DanteEnergyFilterThreshold, &dValue);
    pConfig->energy_filter_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteEnergyFilterThreshold, pConfig->energy_filter_thr * mcaBinWidth);
   
    getDoubleParam(addr, DanteEnergyBaselineThreshold, &dValue);
    pConfig->energy_baseline_thr = uint32_t(round(dValue / mcaBinWidth));
    setDoubleParam(addr, DanteEnergyBaselineThreshold, pConfig->energy_baseline_thr * mcaBinWidth);

    getDoubleParam(addr, DanteMaxRiseTime, &dValue);
    pConfig->max_risetime = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteMaxRiseTime, pConfig->max_risetime / usecToFastSample);

    getDoubleParam(addr, DanteGain, &dValue);
    pConfig->gain = dValue;

    getDoubleParam(addr, DantePeakingTime, &dValue);
    pConfig->peaking_time = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DantePeakingTime, pConfig->peaking_time / usecToSlowSample);

    getDoubleParam(addr, DanteMaxPeakingTime, &dValue);
    pConfig->max_peaking_time = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DanteMaxPeakingTime, pConfig->max_peaking_time / usecToSlowSample);

    getDoubleParam(addr, DanteFlatTop, &dValue);
    pConfig->flat_top = uint32_t(round(dValue * usecToSlowSample));
    setDoubleParam(addr, DanteFlatTop, pConfig->flat_top / usecToSlowSample);

    getDoubleParam(addr, DanteEdgePeakingTime, &dValue);
    pConfig->edge_peaking_time = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteEdgePeakingTime, pConfig->edge_peaking_time / usecToFastSample);

    getDoubleParam(addr, DanteEdgeFlatTop, &dValue);
    pConfig->edge_flat_top = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteEdgeFlatTop, pConfig->edge_flat_top / usecToFastSample);

    getDoubleParam(addr, DanteResetRecoveryTime, &dValue);
    pConfig->reset_recovery_time = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteResetRecoveryTime, pConfig->reset_recovery_time / usecToFastSample);

    getDoubleParam(addr, DanteZeroPeakFreq, &dValue);
    pConfig->zero_peak_freq = dValue/1000.;

    getIntegerParam(addr, DanteBaselineSamples, &iValue);
    pConfig->baseline_samples = ivalue;

    getIntegerParam(addr, DanteInvertedInput, &iValue);
    pConfig->inverted_input = ivalue;

    getDoubleParam(addr, DanteTimeConstant, &dValue);
    // NEED TO CHECK UNITS HERE, not documented in DLL_SPP_Callback.h
    pConfig->time_constant = dValue;

    getIntegerParam(addr, DanteBaseOffset, &iValue);
    pConfig->base_offset = ivalue;

    getDoubleParam(addr, DanteOverflowRecovery, &dValue);
    pConfig->overflow_recovery = uint32_t(round(dValue * usecToFastSample));
    setDoubleParam(addr, DanteOverflowRecovery, pConfig->overflow_recovery / usecToFastSample);

    getIntegerParam(addr, DanteResetThreshold, &iValue);
    pConfig->reset_threshold = ivalue;

    getDoubleParam(addr, DanteTailCoefficient, &dValue);
    // NEED TO CHECK UNITS HERE, not documented in DLL_SPP_Callback.h
    pConfig->tail_coefficient = dValue;

    callId_ = configure(danteIdentifier_, addr, *pConfig);
    if (callId_ < 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
            "%s:%s: error calling configure = %d\n",
            driverName, functionName, callId_);
            return asynError;
    }
    waitReply(callId_, danteReply_);
//    if (runActive) xiaStartRun(channel, 1);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return asynSuccess;
}


asynStatus Dante::configureCollectMode()
{
    asynStatus status = asynSuccess;
    static const char *functionName = "configureCollectMode";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: returning status=%d\n",
        driverName, functionName, status);
    return status;
}

asynStatus Dante::getAcquisitionStatus(asynUser *pasynUser, int addr)
{
    int acquiring=0;
    int ivalue;
    int channel=addr;
    asynStatus status=asynSuccess;
    int i;
    //static const char *functionName = "getAcquisitionStatus";
    
    /* Note: we use the internal parameter DanteAcquiring rather than mcaAcquiring here
     * because we need to do callbacks in acquisitionTask() on all other parameters before
     * we do callbacks on mcaAcquiring, and callParamCallbacks does not allow control over the order. */

    if (addr == nChannels_) channel = ALL_CHANNELS;
    else if (addr == ALL_CHANNELS) addr = nChannels_;
    if (channel == ALL_CHANNELS) { /* All channels */
        for (i=0; i<nChannels_; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatus(pasynUser, i);
            getIntegerParam(i, DanteAcquiring, &ivalue);
            acquiring = std::max(acquiring, ivalue);
        }
        setIntegerParam(addr, DanteAcquiring, acquiring);
    } else {
        /* Get the run time status from the Dante library */
        callId_ = isRunning_system(danteIdentifier_, addr);
        waitReply(callId_, danteReply_);
        setIntegerParam(addr, DanteAcquiring, danteReply_[0]);
    }
    //asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
    //    "%s::%s addr=%d channel=%d: acquiring=%d\n",
    //    driverName, functionName, addr, channel, acquiring);
    return(status);
}

asynStatus Dante::getAcquisitionStatistics(asynUser *pasynUser, int addr)
{
    double dvalue, realTime=0, liveTime=0, icr=0, ocr=0, lastTimeStamp=0;
    int events=0, triggers=0, edgeDT=0, filt1DT=0, zeroCounts=0, baselinesValue=0;
    int pupValue=0, pupF1Value=0, pupNotF1Value=0, resetCounterValue=0;
    int ivalue;
    int channel=addr;
    int erased;
    int i;
    const char *functionName = "getAcquisitionStatistics";

    if (addr == nChannels_) channel = ALL_CHANNELS;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s::%s addr=%d channel=%d\n", 
        driverName, functionName, addr, channel);
    if (channel == ALL_CHANNELS) { /* All channels */
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s start ALL_CHANNELS\n", 
            driverName, functionName);
        addr = nChannels_;
        for (i=0; i<nChannels_; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getAcquisitionStatistics(pasynUser, i);
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
            getIntegerParam(i, DanteEdgeDT, &ivalue);
            edgeDT = std::max(edgeDT, ivalue);
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
        setIntegerParam(addr, DanteEdgeDT,          edgeDT);
        setIntegerParam(addr, DanteFilt1DT,         filt1DT);
        setIntegerParam(addr, DanteZeroCounts,      zeroCounts);
        setIntegerParam(addr, DanteBaselinesValue,  baselinesValue);
        setIntegerParam(addr, DantePupValue,        pupValue);
        setIntegerParam(addr, DantePupF1Value,      pupF1Value);
        setIntegerParam(addr, DantePupNotF1Value,   pupNotF1Value);
        setIntegerParam(addr, DanteResetCounterValue, resetCounterValue);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
            "%s::%s end ALL_CHANNELS\n", 
            driverName, functionName);
    } else {
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s start channel %d\n", 
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
            setIntegerParam(addr, DanteEdgeDT,          0);
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
            setDoubleParam(addr, mcaElapsedRealTime,    pStats->real_time/1e6);
            setDoubleParam(addr, mcaElapsedLiveTime,    pStats->live_time/1e6);
            setDoubleParam(addr, DanteInputCountRate,   pStats->ICR);
            setDoubleParam(addr, DanteOutputCountRate,  pStats->OCR);
            setDoubleParam(addr, DanteLastTimeStamp,    pStats->last_timestamp);
            setIntegerParam(addr, DanteTriggers,        pStats->detected);
            setIntegerParam(addr, DanteEvents,          pStats->measured);
            setIntegerParam(addr, DanteEdgeDT,          pStats->edge_dt);
            setIntegerParam(addr, DanteFilt1DT,         pStats->filt1_dt);
            setIntegerParam(addr, DanteZeroCounts,      pStats->zerocounts);
            setIntegerParam(addr, DanteBaselinesValue,  pStats->baselines_value);
            setIntegerParam(addr, DantePupValue,        pStats->pup_value);
            setIntegerParam(addr, DantePupF1Value,      pStats->pup_f1_value);
            setIntegerParam(addr, DantePupNotF1Value,   pStats->pup_notf1_value);
            setIntegerParam(addr, DanteResetCounterValue, pStats->reset_counter_value);
  
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                "%s::%s  channel %d \n"
                "           real time=%f\n" 
                "            livetime=%f\n" 
                "    input count rate=%f\n" 
                "   output count rate=%f\n"
                "     last time stamp=%f\n" 
                "            triggers=%d\n" 
                "              events=%d\n" 
                "             edge dt=%d\n" 
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
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return(asynSuccess);
}

asynStatus Dante::getMcaData(asynUser *pasynUser, int addr)
{
    asynStatus status = asynSuccess;
    int arrayCallbacks;
    int nChannels;
    int channel=addr;
    int i;
    //NDArray *pArray;
    NDDataType_t dataType;
    epicsTimeStamp now;
    const char* functionName = "getMcaData";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == nChannels_) channel = ALL_CHANNELS;

    getIntegerParam(mcaNumChannels, &nChannels);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    getIntegerParam(NDDataType, (int *)&dataType);

    epicsTimeGetCurrent(&now);

    if (channel == ALL_CHANNELS) {  /* All channels */
        for (i=0; i<nChannels_; i++) {
            /* Call ourselves recursively but with a specific channel */
            this->getMcaData(pasynUser, i);
        }
    } else {
        /* Read the MCA spectrum */
        uint32_t id, spectraSize = nChannels;
        getData(danteIdentifier_, addr, pMcaRaw_[addr], id, statistics_[addr], spectraSize);
        asynPrintIO(pasynUser, ASYN_TRACEIO_DRIVER, (const char *)pMcaRaw_[addr], nChannels*sizeof(pMcaRaw_[0][0]),
            "%s::%s Got MCA spectrum channel:%d ptr:%p\n",
            driverName, functionName, channel, pMcaRaw_[addr]);

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
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
    return status;
}

/** Reads the mapping data for all of the modules in the system */
asynStatus Dante::getMappingData()
{
    asynStatus status = asynSuccess;
/*
    int xiastatus;
    int arrayCallbacks;
    NDDataType_t dataType;
    int buf = 0, channel, i, k, l;
    NDArray *pArray=NULL;
    epicsUInt16 *pOut=0;
    epicsUInt16 *pStats;
    int mappingMode, pixelOffset, dataOffset, events, triggers, nChans;
    double realTime, triggerLiveTime, energyLiveTime, icr, ocr;
    size_t dims[2];
    int bufferCounter, arraySize;
    epicsTimeStamp now, after;
    double mBytesRead;
    double readoutTime, readoutBurstRate, MBbufSize;
    const char* functionName = "getMappingData";

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s: enter\n",
        driverName, functionName);

    getIntegerParam(NDDataType, (int *)&dataType);
    getIntegerParam(DanteBufferCounter, &bufferCounter);
    bufferCounter++;
    setIntegerParam(DanteBufferCounter, bufferCounter);
    getIntegerParam(NDArraySize, &arraySize);
    getDoubleParam(DanteMBytesRead, &mBytesRead);
    getIntegerParam(NDArrayCallbacks, &arrayCallbacks);
    MBbufSize = (double)((arraySize)*sizeof(epicsUInt16)) / (double)MEGABYTE;

    // First read and reset the buffers, do this as quickly as possible
    for (channel=0; channel<nChannels_; channel+=this->channelsPerCard)
    {
        buf = this->currentBuf[channel];

        // The buffer is full so read it out
        epicsTimeGetCurrent(&now);
        xiastatus = xiaGetRunData(channel, DanteBufferString[buf], this->pMapTemp);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "GetRunData mapping");
        epicsTimeGetCurrent(&after);
        readoutTime = epicsTimeDiffInSeconds(&after, &now);
        readoutBurstRate = MBbufSize / readoutTime;
        mBytesRead += MBbufSize;
        setDoubleParam(DanteMBytesRead, mBytesRead);
        setDoubleParam(DanteReadRate, readoutBurstRate);
        // Notify system that we read out the buffer
        xiastatus = xiaBoardOperation(channel, "buffer_done", DanteBufferCharString[buf]);
        status = xia_checkError(this->pasynUserSelf, xiastatus, "buffer_done");
        if (buf == 0) this->currentBuf[channel] = 1;
        else this->currentBuf[channel] = 0;
        callParamCallbacks();
        // xiaGetRunData requires an unsigned long buffer, but the data are actually only 16 bits.
        // Convert to 16-bit data here
        for (i=0; i<arraySize; i++) {
            pMapRaw[i] = (epicsUInt16) pMapTemp[i];
        }
    
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s Got data! size=%.3fMB (%d) dt=%.3fs speed=%.3fMB/s\n",
            driverName, functionName, MBbufSize, arraySize, readoutTime, readoutBurstRate);
        asynPrint(this->pasynUserSelf, ASYN_TRACEIO_DRIVER, 
            "%s::%s channel=%d, tag0=0x%x, tag1=0x%x, headerSize=%d, mappingMode=%d, runNumber=%d, bufferNumber=%d, bufferID=%d, numPixels=%d, firstPixel=%d\n",
            driverName, functionName, channel, pMapRaw[0], pMapRaw[1], pMapRaw[2], pMapRaw[3], pMapRaw[4], pMapRaw[5], pMapRaw[7], pMapRaw[8], pMapRaw[9]);
    
        // If this is MCA mapping mode then copy the spectral data for the first pixel
        // in this buffer to the mcaRaw buffers.
        // This provides an update of the spectra and statistics while mapping is in progress
        // if the user sets the MCA spectra to periodically read.
        mappingMode = pMapRaw[3];
        if (mappingMode == DanteModeSpectraMapping) {
            pixelOffset = 256;
            dataOffset = pixelOffset + 256;
            for (i=0; i<this->channelsPerCard; i++) {
                k = channel + i;
                nChans = pMapRaw[pixelOffset + 8 + i];
                for (l=0; l<nChans; l++) {
                    pMcaRaw_[k][l] = pMapRaw[dataOffset + l];
                }
                dataOffset += nChans;
                pStats = &pMapRaw[pixelOffset + 32 + i*8];
                realTime        = (pStats[0] + (pStats[1]<<16)) * MAPPING_CLOCK_PERIOD;
                triggerLiveTime = (pStats[2] + (pStats[3]<<16)) * MAPPING_CLOCK_PERIOD;
                triggers        =  pStats[4] + (pStats[5]<<16);
                events          =  pStats[6] + (pStats[7]<<16);
                if (triggers > 0.) 
                    energyLiveTime = (triggerLiveTime * events) / triggers;
                else
                    energyLiveTime = triggerLiveTime;
                if (triggerLiveTime > 0.)
                    icr = triggers / triggerLiveTime;
                else
                    icr = 0.;
                if (realTime > 0.)
                    ocr = events / realTime;
                else
                    ocr = 0.;
                setDoubleParam(k, mcaElapsedRealTime, realTime);
                setDoubleParam(k, mcaElapsedLiveTime, energyLiveTime);
                setDoubleParam(k, DanteTriggerLiveTime, triggerLiveTime);
                setIntegerParam(k,DanteEvents, events);
                setIntegerParam(k, DanteTriggers, triggers);
                setDoubleParam(k, DanteInputCountRate, icr);
                setDoubleParam(k, DanteOutputCountRate, ocr);
                callParamCallbacks(k, k);
            }
        }
            
        if (arrayCallbacks)
        {
            // Now rearrange the data and do the callbacks
            // If this is the first module then allocate an NDArray for the callback
            if (channel == 0) {
               dims[0] = arraySize;
               dims[1] = this->nCards;
               pArray = this->pNDArrayPool->alloc(2, dims, dataType, 0, NULL );
               pOut = (epicsUInt16 *)pArray->pData;
            }
            
            memcpy(pOut, pMapRaw, arraySize * sizeof(epicsUInt16));
            pOut += arraySize;
        }
    }
    if (arrayCallbacks) 
    {
        pArray->timeStamp = now.secPastEpoch + now.nsec / 1.e9;
        pArray->uniqueId = bufferCounter;
        doCallbacksGenericPointer(pArray, NDArrayData, 0);
        pArray->release();
    }
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s::%s Done reading! Ch=%d bufchar=%s\n",
        driverName, functionName, channel, DanteBufferCharString[buf]);
*/
    return status;
}

/* Get trace data */
asynStatus Dante::getTrace(asynUser* pasynUser, int addr,
                           epicsInt32* data, size_t maxLen, size_t *actualLen)
{
    asynStatus status = asynSuccess;
/*
    int xiastatus, channel=addr;
    int i, j;
    int newTraceTime;
    double info[2];
    double traceTime;
    int traceMode;
    const char *functionName = "getTrace";

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: enter addr=%d\n",
        driverName, functionName, addr);
    if (addr == nChannels_) channel = ALL_CHANNELS;
    if (channel == ALL_CHANNELS) {  // All channels
        for (i=0; i<nChannels_; i++) {
            // Call ourselves recursively but with a specific channel
            this->getTrace(pasynUser, i, data, maxLen, actualLen);
        }
    } else {
        getDoubleParam(channel, DanteTraceTime, &traceTime);
        getIntegerParam(channel, DanteNewTraceTime, &newTraceTime);
        getIntegerParam(channel, DanteTraceMode, &traceMode);
        info[0] = 0.;
        // Convert from us to ns
        info[1] = traceTime * 1000.;

        xiastatus = xiaDoSpecialRun(channel, (char *)DanteTraceCommands[traceMode], info);
        status = this->xia_checkError(pasynUser, xiastatus, DanteTraceCommands[traceMode]);
        // Don't return error, read it out or we get stuck permanently with module busy
        // if (status == asynError) return asynError;

        *actualLen = this->traceLength;
        if (maxLen < *actualLen) *actualLen = maxLen;

        xiastatus = xiaGetSpecialRunData(channel, "adc_trace", this->traceBuffer);
        status = this->xia_checkError( pasynUser, xiastatus, "adc_trace" );
        if (status == asynError) return status;

        memcpy(data, this->traceBuffer, *actualLen * sizeof(epicsInt32));
        
        if (newTraceTime) {
            setIntegerParam(channel, DanteNewTraceTime, 0);  // Clear flag
            for (j=0; j<this->traceLength; j++) this->traceTimeBuffer[j] = j*traceTime;
            doCallbacksFloat64Array(this->traceTimeBuffer, this->traceLength, DanteTraceTimeArray, channel);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "%s:%s: exit\n",
        driverName, functionName);
*/
    return status;
}

asynStatus Dante::startAcquiring(asynUser *pasynUser)
{
    asynStatus status = asynSuccess;
    int channel, addr, i;
    int acquiring, erased;
    const char *functionName = "startAcquiring";

    channel = this->getChannel(pasynUser, &addr);
    getIntegerParam(addr, mcaAcquiring, &acquiring);
    getIntegerParam(addr, DanteErased, &erased);

    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
        "%s::%s ch=%d acquiring=%d, erased=%d\n",
        driverName, functionName, channel, acquiring, erased);
    /* if already acquiring we just ignore and return */
    if (acquiring) return status;

    double presetReal;
    int numChannels;
    getDoubleParam(mcaPresetRealTime, &presetReal);
    getIntegerParam(mcaNumChannels, &numChannels);
    callId_ = start(danteIdentifier_, presetReal, numChannels);
    waitReply(callId_, danteReply_);

    setIntegerParam(addr, DanteErased, 0); /* reset the erased flag */
    setIntegerParam(addr, mcaAcquiring, 1); /* Set the acquiring flag */

    if (channel == ALL_CHANNELS) {
        for (i=0; i<nChannels_; i++) {
            setIntegerParam(i, mcaAcquiring, 1);
            setIntegerParam(i, DanteErased, 0);
            callParamCallbacks(i);
        }
    }

    callParamCallbacks(addr);

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
    asynUser *pasynUser = this->pasynUserSelf;
    int paramStatus;
    int i;
    int mode;
    int acquiring = 0;
    epicsFloat64 pollTime, sleeptime, dtmp;
    epicsTimeStamp now, start;
    const char* functionName = "acquisitionTask";

    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s:%s acquisition task started!\n",
        driverName, functionName);

    lock();

    while (polling_) /* ... round and round and round we go until the IOC is shut down */
    {

        getIntegerParam(nChannels_, mcaAcquiring, &acquiring);

        if (!acquiring)
        {
            /* Release the lock while we wait for an event that says acquire has started, then lock again */
            unlock();
            /* Wait for someone to signal the cmdStartEvent */
            asynPrint(pasynUser, ASYN_TRACE_FLOW, 
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
        getAcquisitionStatus(this->pasynUserSelf, ALL_CHANNELS);
        getIntegerParam(nChannels_, DanteAcquiring, &acquiring);
        if (!acquiring)
        {
            /* There must have just been a transition from acquiring to not acquiring */

            if (mode == DanteModeMCA) {
                /* In MCA mode we force a read of the data */
                asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                    "%s::%s Detected acquisition stop! Now reading statistics\n",
                    driverName, functionName);
                getMcaData(this->pasynUserSelf, ALL_CHANNELS);
                getAcquisitionStatistics(this->pasynUserSelf, ALL_CHANNELS);
                asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
                    "%s::%s Detected acquisition stop! Now reading data\n",
                    driverName, functionName);
            }
            else {
                /* In mapping modes need to make an extra call to pollMappingMode because there could be
                 * 2 mapping mode buffers that still need to be read out. 
                 * This call will read out the first one, and just below this !acquiring block
                 * there is a second call to pollMapping mode which is
                 * done on every main loop in mapping modes. */
                 this->pollMappingMode();
            }
        } 
        if (mode != DanteModeMCA)
        {
            this->pollMappingMode();
        }

        /* Do callbacks for all channels for everything except mcaAcquiring*/
        for (i=0; i<=nChannels_; i++) callParamCallbacks(i);
        /* Copy internal acquiring flag to mcaAcquiring */
        for (i=0; i<=nChannels_; i++) {
            getIntegerParam(i, DanteAcquiring, &acquiring);
            setIntegerParam(i, mcaAcquiring, acquiring);
            callParamCallbacks(i);
        }
        
        paramStatus |= getDoubleParam(DantePollTime, &pollTime);
        epicsTimeGetCurrent(&now);
        dtmp = epicsTimeDiffInSeconds(&now, &start);
        sleeptime = pollTime - dtmp;
        if (sleeptime > 0.0)
        {
            //asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            //    "%s::%s Sleeping for %f seconds\n",
            //    driverName, functionName, sleeptime);
            this->unlock();
            epicsThreadSleep(sleeptime);
            this->lock();
        }
    }
}

/** Check if the current mapping buffer is full in which case it reads out the data */
asynStatus Dante::pollMappingMode()
{
    asynStatus status = asynSuccess;
/*
    asynUser *pasynUser = this->pasynUserSelf;
    int xiastatus;
    int ignored;
    int ch, buf=0, allFull=1, anyFull=0;
    unsigned short isFull;
    unsigned long currentPixel = 0;
    const char* functionName = "pollMappingMode";
    DanteCollectMode_t mappingMode;
    
    getIntegerParam(DanteCollectMode, (int *)&mappingMode);

    for (ch=0; ch<nChannels_; ch+=this->channelsPerCard)
    {
        buf = this->currentBuf[ch];

        if (mappingMode == DanteModeListMapping) {
            CALLHANDEL( xiaGetRunData(ch, DanteListBufferLenString[buf], &currentPixel), "DanteListBufferLenString[buf]")
        }
        else {
            CALLHANDEL( xiaGetRunData(ch, "current_pixel", &currentPixel) , "current_pixel" )
        }
        setIntegerParam(ch, DanteCurrentPixel, (int)currentPixel);
        callParamCallbacks(ch);
        CALLHANDEL( xiaGetRunData(ch, DanteBufferFullString[buf], &isFull), "DanteBufferFullString[buf]" )
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
            "%s::%s %s isfull=%d\n",
            driverName, functionName, DanteBufferFullString[buf], isFull);
        if (!isFull) allFull = 0;
        if (isFull)  anyFull = 1;
    }

    // In list mapping mode if any buffer is full then switch buffers on the non-full ones.
    // Note: this is prone to error because they could have already switched!
    if (anyFull && (mappingMode == DanteModeListMapping)) {
        for (ch=0; ch<nChannels_; ch+=this->channelsPerCard) {
            CALLHANDEL( xiaGetRunData(ch, DanteBufferFullString[buf], &isFull), "DanteBufferFullString[buf]" )
            if (isFull) continue;
            CALLHANDEL( xiaBoardOperation(ch, "buffer_switch", &ignored), "buffer_switch" )
        }
        // Now wait until all modules report they are full
        do {
            allFull = 1;
            for (ch=0; ch<nChannels_; ch+=this->channelsPerCard) {
                CALLHANDEL( xiaGetRunData(ch, DanteBufferFullString[buf], &isFull), "DanteBufferFullString[buf]" )
                if (!isFull) allFull = 0;
            }
        } while (allFull != 1);
    }
    // If all of the modules have full buffers then read them all out
    if (allFull)
    {
        status = this->getMappingData();
    }
*/
    return status;
}


void Dante::report(FILE *fp, int details)
{
    if (details > 0) {
        for (int i=0; i<nChannels_; i++) {
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
            fprintf(fp, "    edge_peaking_time: %d\n", pConfig->edge_peaking_time);
            fprintf(fp, "        edge_flat_top: %d\n", pConfig->edge_flat_top);
            fprintf(fp, "  reset_recovery_time: %d\n", pConfig->reset_recovery_time);
            fprintf(fp, "       zero_peak_freq: %f\n", pConfig->zero_peak_freq);
            fprintf(fp, "     baseline_samples: %d\n", pConfig->baseline_samples);
            fprintf(fp, "       inverted_input: %d\n", pConfig->inverted_input ? 1 : 0);
            fprintf(fp, "        time_constant: %f\n", pConfig->time_constant);
            fprintf(fp, "          base_offset: %d\n", pConfig->base_offset);
            fprintf(fp, "    overflow_recovery: %d\n", pConfig->overflow_recovery);
            fprintf(fp, "      reset_threshold: %d\n", pConfig->reset_threshold);
            fprintf(fp, "     tail_coefficient: %f\n", pConfig->tail_coefficient);
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
    
    getDoubleParam(DantePollTime, &pollTime);
    asynPrint(this->pasynUserSelf, ASYN_TRACE_FLOW, 
        "%s: shutting down in %f seconds\n", driverName, 2*pollTime);
    polling_ = 0;
    epicsThreadSleep(2*pollTime);
    clear_chain(danteIdentifier_);
    if (CloseLibrary()) {
        printf("%s shut down successfully.\n",
                driverName);
        return;
    }
    else {
        printf("Error calling CloseLibrary()\n");
        return;
    }
}


static const iocshArg DanteConfigArg0 = {"Asyn port name", iocshArgString};
static const iocshArg DanteConfigArg1 = {"IP address", iocshArgString};
static const iocshArg DanteConfigArg2 = {"Number of channels", iocshArgInt};
static const iocshArg DanteConfigArg3 = {"Maximum number of buffers", iocshArgInt};
static const iocshArg DanteConfigArg4 = {"Maximum amount of memory (bytes)", iocshArgInt};
static const iocshArg * const DanteConfigArgs[] =  {&DanteConfigArg0,
                                                    &DanteConfigArg1,
                                                    &DanteConfigArg2,
                                                    &DanteConfigArg3,
                                                    &DanteConfigArg4};
static const iocshFuncDef configDante = {"DanteConfig", 5, DanteConfigArgs};
static void configDanteCallFunc(const iocshArgBuf *args)
{
    DanteConfig(args[0].sval, args[1].sval, args[2].ival, args[3].ival, args[4].ival);
}

static void mcaDanteRegister(void)
{
    iocshRegister(&configDante, configDanteCallFunc);
}

extern "C" {
epicsExportRegistrar(mcaDanteRegister);
}
