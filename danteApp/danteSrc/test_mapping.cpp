#include <time.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#ifdef _WIN32
  #include <Windows.h>
#endif
#include "DLL_DPP_Callback.h"

static const char *driverName = "Dante";
static const char *ipAddress = "164.54.160.181";
#define MAX_MESSAGE_DATA          20

struct message {
    uint16_t type;
    uint32_t call_id;
    uint32_t length;
    uint32_t data[MAX_MESSAGE_DATA];
};

struct mappingStats {
      double real_time;
      double live_time;
      double ICR;
      double OCR;
};

static struct message message;

struct mappingAdvStats {
      uint64_t last_timestamp;
      uint64_t detected;
      uint64_t measured;
      uint64_t edge_dt;
      uint64_t filt1_dt;
      uint64_t zerocounts;
      uint64_t baselines_value;
      uint64_t pup_value;
      uint64_t pup_f1_value;
      uint64_t pup_notf1_value;
      uint64_t reset_counter_value;
      uint64_t spectra_in_FIFO;
    uint64_t x_position;
    uint64_t x_timestamp;
    uint64_t gateRising;
    uint64_t gateFalling;
    uint64_t gateHigh;
    uint64_t gateLow;
    uint64_t y_position;
    uint64_t y_timestamp;
    uint64_t z_position;
    uint64_t z_timestamp;
    uint64_t unused1;
    uint64_t unused2;
};

static bool callbackComplete;

static void mySleep(double seconds)
{
#ifdef _WIN32
    return Sleep(seconds * 1000);
#else
    time_t sec = (int)seconds;
    long nsec = (int)((seconds - sec) * 1e9);
    struct timespec ts;
    ts.tv_sec = sec;
    ts.tv_nsec = nsec;
    struct timespec tr;
    int result = nanosleep(&ts, &tr);
    if (result != 0) printf("Error return from nanosleep\n");
#endif
}

void danteCallback(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data) {
    message.type = type;
    message.call_id = call_id;
    message.length = length;
    for (uint32_t i=0; i<length; i++) {
        message.data[i] = data[i];
    }
    callbackComplete = true;
}

static int waitReply(uint32_t callId, char *reply) {
    static const char *functionName = "waitReply";
    callbackComplete = false;
    while (!callbackComplete) mySleep(0.001);
    if (message.call_id != callId) {
        printf("%s::%s incorrect callId expected=%d actual=%d\n",
            driverName, functionName, callId, message.call_id);
        return 1;
    }
    for (uint32_t i=0; i<message.length; i++) {
        reply[i]=message.data[i];
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int callId;
    uint32_t currentPixel = 0;
    uint32_t acqTimeMs;
    uint32_t pollTimeMs;
    uint32_t mappingPoints;
    int numMCAChannels = 2048;
    double maxEnergy = 25;
    double mcaBinWidth;
    double usecToFastSample = 1e-6/8e-9;
    double usecToSlowSample = 1e-6/32e-9;
    struct configuration config;
    configuration *pConfig = &config;
    char danteIdentifier[16];
    char danteReply[MAX_MESSAGE_DATA];
    const char *functionName = "Dante";

    acqTimeMs = atoi(argv[1]);
    pollTimeMs = atoi(argv[2]);
    mappingPoints = atoi(argv[3]);
    printf("Acquire time (ms)=%d, poll time (ms)=%d, mappingPoints=%d\n", acqTimeMs, pollTimeMs, mappingPoints);

    if (!InitLibrary()) {
          printf("%s::%s error calling InitLibrary\n", driverName, functionName);
          return -1;
    }

    char libraryVersion[20];
    uint32_t libSize = sizeof(libraryVersion);
      if (!libVersion(libraryVersion, libSize)) {
          printf("%s::%s error calling libVersion\n", driverName, functionName);
          return -1;
      }    else {
          printf("%s::%s library version=%s\n", driverName, functionName, libraryVersion);
      }

    if (!add_to_query((char *)ipAddress)) {
          printf("%s::%s error calling add_to_query\n", driverName, functionName);
          return -1;
      }    else {
          printf("%s::%s ipAddress added to query=%s\n", driverName, functionName, ipAddress);
      }

    // Wait 5 seconds for devices to be found
    mySleep(5.);

      uint16_t numDevices;
      if (!get_dev_number(numDevices)) {
          printf("%s::%s error calling get_dev_number\n", driverName, functionName);
          return -1;
      } else {
          printf("%s::%s number of devices=%d\n", driverName, functionName, numDevices);
      }

    uint16_t idSize = sizeof(danteIdentifier);
    uint16_t deviceId = 0;
      if (!get_ids(danteIdentifier, deviceId, idSize)) {
          printf("%s::%s error calling get_ids\n", driverName, functionName);
          return -1;
      } else {
          printf("%s::%s danteIdentifier=%s\n", driverName, functionName, danteIdentifier);
      }

      uint16_t chain = 1;
      if (!get_boards_in_chain(danteIdentifier, chain)) {
          printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
          return -1;
      } else {
          printf("%s::%s boards in chain=%d\n", driverName, functionName, chain);
      }

    // Wait a little bit for daisy chain synchronization and ask again for connected systems.
    mySleep(1.0);
      if (!get_ids(danteIdentifier, deviceId, idSize)) {
          printf("%s::%s error calling get_ids\n", driverName, functionName);
          return -1;
      } else {
          printf("%s::%s danteIdentifier=%s\n", driverName, functionName, danteIdentifier);
      }

      if (!get_boards_in_chain(danteIdentifier, chain)) {
          printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
          return -1;
      } else {
          printf("%s::%s boards in chain=%d\n", driverName, functionName, chain);
      }

      if (!register_callback(danteCallback)) {
         printf("%s::%s error calling register_callback\n", driverName, functionName);
         return -1;
      } else {
          printf("%s::%s register callback OK\n", driverName, functionName);
      }


    mcaBinWidth = maxEnergy/numMCAChannels;

    pConfig->fast_filter_thr = uint32_t(round(0.1 / mcaBinWidth));
    pConfig->energy_filter_thr = uint32_t(round(3.0 / mcaBinWidth));
    pConfig->energy_baseline_thr = uint32_t(round(3.0 / mcaBinWidth));
    pConfig->max_risetime = uint32_t(round(0.20 * usecToFastSample));
    pConfig->gain = 1.0;
    pConfig->peaking_time = uint32_t(round(2.0 * usecToSlowSample));
    pConfig->max_peaking_time = uint32_t(round(2.0 * usecToSlowSample));
    pConfig->flat_top = uint32_t(round(0.10 * usecToSlowSample));
    pConfig->edge_peaking_time = uint32_t(round(0.05 * usecToFastSample));
    pConfig->edge_flat_top = uint32_t(round(0.01 * usecToFastSample));
    pConfig->reset_recovery_time = uint32_t(round(4.0 * usecToFastSample));
    pConfig->zero_peak_freq = 1.;
    pConfig->baseline_samples = 64;
    pConfig->inverted_input = 0;
    pConfig->time_constant = 0;
    pConfig->base_offset = 0;
    pConfig->overflow_recovery = uint32_t(round(0.0 * usecToFastSample));
    pConfig->reset_threshold = 1000;
    pConfig->tail_coefficient = 0.0;

    printf("Calling configure\n");
    callId = configure(danteIdentifier, 0, *pConfig);
    if (callId < 0) {
        printf("%s:%s: error calling configure = %d\n",
            driverName, functionName, callId);
            return -1;
    }
    waitReply(callId, danteReply);
    printf("configure complete\n");

    printf("Calling start_map\n");
    callId = start_map(danteIdentifier, acqTimeMs, mappingPoints, numMCAChannels);
    waitReply(callId, danteReply);
    printf("start_map complete\n");

    while (currentPixel < mappingPoints-1) {
        mySleep(pollTimeMs/1000.);
        callId = isRunning_system(danteIdentifier, 0);
        waitReply(callId, danteReply);
        //bool acquiring = (danteReply[0]!=0) ? true : false;
        uint32_t numAvailable;
        if (!getAvailableData(danteIdentifier, 0, numAvailable)) {
            printf("%s::%s error calling getAvailableData\n", driverName, functionName);
        }
        if (numAvailable == 0) continue;

        uint32_t spectraSize = numMCAChannels;

        uint16_t *pMappingMCAData  = (uint16_t *) malloc(numAvailable * numMCAChannels * sizeof(uint16_t));
        uint32_t *pSpectraId = (uint32_t *) malloc(numAvailable * sizeof(uint32_t));
        struct mappingStats *pMappingStats = (mappingStats *) malloc(numAvailable * sizeof(struct mappingStats));
        struct mappingAdvStats *pMappingAdvStats = (mappingAdvStats *)malloc(numAvailable * sizeof(struct mappingAdvStats));

        if (!getAllData(danteIdentifier, 0, pMappingMCAData, pSpectraId,
                        (double *)pMappingStats, (uint64_t*)pMappingAdvStats, spectraSize, numAvailable)) {
            printf("%s::%s error calling getAllData\n", driverName, functionName);
        }

        currentPixel += numAvailable;
        printf("getAllData read %d spectra OK, pSpectraId[0]=%u, currentPixel=%d \n", numAvailable, pSpectraId[0], currentPixel);
        free(pMappingMCAData);
        free(pSpectraId);
        free(pMappingStats);
        free(pMappingAdvStats);
    }
    callId = stop(danteIdentifier);
    waitReply(callId, danteReply);
    clear_chain(danteIdentifier);
    printf("%s::%s calling CloseLibrary()\n", driverName, functionName);
    if (CloseLibrary()) {
        printf("%s::%s called CloseLibrary successfully\n", driverName, functionName);
        return -1;
    }
    printf("Closed library OK, exiting\n");
}
