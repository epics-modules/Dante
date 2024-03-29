#include <time.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <thread>
#ifdef _WIN32
  #include <Windows.h>
#endif
#include "DLL_DPP_Callback.h"

static const char *driverName = "Dante";
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
    int64_t last_timestamp;
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

#ifdef _WIN32
    static FILETIME tstart, tnow;
    static void startClock() {
        GetSystemTimePreciseAsFileTime(&tstart);
    }
    static double elapsed_time() {
        GetSystemTimePreciseAsFileTime(&tnow);
        long long elapsed = (tnow.dwLowDateTime   + (((long long)tnow.dwHighDateTime)<<32)) -
                            (tstart.dwLowDateTime + (((long long)tstart.dwHighDateTime)<<32));
        return elapsed * 100e-9;
    }

#else
    static struct timespec tstart, tnow;
    static void startClock() {
        clock_gettime(CLOCK_MONOTONIC, &tstart);
    }
    static double elapsed_time() {
        clock_gettime(CLOCK_MONOTONIC, &tnow);
        return (((double)tnow.tv_sec + 1e-9*tnow.tv_nsec) -
                ((double)tstart.tv_sec + 1e-9*tstart.tv_nsec));
    }
#endif

static void mySleep(double seconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds((int)(seconds*1000)));
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
    uint32_t currentPixel[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t numAvailable[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t acqTimeMs;
    uint32_t pollTimeMs;
    uint32_t mappingPoints;
    uint32_t gatingMode;
    int board;
    int numMCAChannels;
    double maxEnergy = 25;
    double mcaBinWidth;
    double usecToFastSample = 1e-6/8e-9;
    double usecToSlowSample = 1e-6/32e-9;
    struct configuration config;
    configuration *pConfig = &config;
    char danteIdentifier[16];
    char danteReply[MAX_MESSAGE_DATA];
    char ipAddress[100];
    const char *functionName = "Dante";

    if (argc != 7) {
        printf("Usage: test_mapping ipAddress mcaChannels acquireTime pollTimeMs mappingPoints gatingMode\n");
        exit(-1);
    }
    strcpy(ipAddress,     argv[1]);
    numMCAChannels = atoi(argv[2]);
    acqTimeMs   =    atoi(argv[3]);
    pollTimeMs =     atoi(argv[4]);
    mappingPoints =  atoi(argv[5]);
    gatingMode =     atoi(argv[6]);
    printf("IP address=%s, mcaChannels=%d, acquire time (ms)=%d, poll time (ms)=%d, mappingPoints=%d, gatingMode=%d\n",
           ipAddress, numMCAChannels, acqTimeMs, pollTimeMs, mappingPoints, gatingMode);

    if (!InitLibrary()) {
        printf("%s::%s error calling InitLibrary\n", driverName, functionName);
        return -1;
    }

    char libraryVersion[20];
    uint32_t libSize = sizeof(libraryVersion);
    if (!libVersion(libraryVersion, libSize)) {
        printf("%s::%s error calling libVersion\n", driverName, functionName);
        return -1;
    } else {
        printf("%s::%s library version=%s\n", driverName, functionName, libraryVersion);
    }

    if (!add_to_query((char *)ipAddress)) {
        printf("%s::%s error calling add_to_query\n", driverName, functionName);
        return -1;
    } else {
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

    uint16_t numBoards = 1;
    if (!get_boards_in_chain(danteIdentifier, numBoards)) {
        printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
        return -1;
    } else {
        printf("%s::%s boards in chain=%d\n", driverName, functionName, numBoards);
    }

    // Wait a little bit for daisy chain synchronization and ask again for connected systems.
    mySleep(1.0);
    if (!get_ids(danteIdentifier, deviceId, idSize)) {
        printf("%s::%s error calling get_ids\n", driverName, functionName);
        return -1;
    } else {
        printf("%s::%s danteIdentifier=%s\n", driverName, functionName, danteIdentifier);
    }

    if (!get_boards_in_chain(danteIdentifier, numBoards)) {
        printf("%s::%s error calling get_boards_in_chain\n", driverName, functionName);
        return -1;
    } else {
        printf("%s::%s boards in chain=%d\n", driverName, functionName, numBoards);
    }

    if (!register_callback(danteCallback)) {
       printf("%s::%s error calling register_callback\n", driverName, functionName);
       return -1;
    } else {
        printf("%s::%s register callback OK\n", driverName, functionName);
    }

    // It is necessary to disable the autoScanSlaves() when configuring, in order to prevent interlock problems. Keep disabled also for acquisitions.
    autoScanSlaves(false);
    mySleep(0.050);  // wait 50ms

    mcaBinWidth = maxEnergy/numMCAChannels;

    pConfig->fast_filter_thr = uint32_t(round(2.0 / mcaBinWidth));
    pConfig->energy_filter_thr = uint32_t(round(0.0 / mcaBinWidth));
    pConfig->energy_baseline_thr = uint32_t(round(0.0 / mcaBinWidth));
    pConfig->max_risetime = uint32_t(round(0.20 * usecToFastSample));
    pConfig->gain = 7.0;
    pConfig->peaking_time = uint32_t(round(2.0 * usecToSlowSample));
    //pConfig->max_peaking_time = uint32_t(round(2.0 * usecToSlowSample));
    // Must be 0 for LE firmware
    pConfig->max_peaking_time = uint32_t(round(0 * usecToSlowSample));
    pConfig->flat_top = uint32_t(round(0.10 * usecToSlowSample));
    pConfig->edge_peaking_time = uint32_t(round(0.2 * usecToFastSample));
    pConfig->edge_flat_top = uint32_t(round(0.01 * usecToFastSample));
    pConfig->reset_recovery_time = uint32_t(round(4.0 * usecToFastSample));
    pConfig->zero_peak_freq = 1.;
    pConfig->baseline_samples = 64;
    pConfig->inverted_input = 0;
    pConfig->time_constant = 0;
    pConfig->base_offset = 0;
    pConfig->overflow_recovery = uint32_t(round(0.0 * usecToFastSample));
    pConfig->reset_threshold = 300;
    pConfig->tail_coefficient = 0.0;

    for (board=0; board<numBoards; board++) {
        printf("Calling configure for board %d\n", board);
        callId = configure(danteIdentifier, board, *pConfig);
        if (callId < 0) {
            printf("%s:%s: error calling configure = %d\n",
                driverName, functionName, callId);
                return -1;
        }
        waitReply(callId, danteReply);
        printf("configure complete for board %d\n", board);
    }

    struct configuration_offset cfgOffset;
    cfgOffset.offset_val1 = 128;
    cfgOffset.offset_val2 = 128;
    for (board=0; board<numBoards; board++) {
        printf("Calling configure_offset for board %d\n", board);
        callId = configure_offset(danteIdentifier, board, cfgOffset);
        waitReply(callId, danteReply);
        printf("configure_offset complete for board %d\n", board);
    }

    for (board=0; board<numBoards; board++) {
        printf("Calling configure_input for board %d\n", board);
        callId = configure_input(danteIdentifier, board, DC_HighImp);
        waitReply(callId, danteReply);
        printf("configure_input complete for board %d\n", board);
    }

    for (board=0; board<numBoards; board++) {
        printf("Calling configure_gating for board %d mode=%d\n", board, gatingMode);
        callId = configure_gating(danteIdentifier, (GatingMode)gatingMode, board);
        waitReply(callId, danteReply);
        printf("configure_gating complete for board %d\n", board);
    }

    printf("Calling start_map, acqTimeMs=%u, mappingPoints=%u, numMCAChannels=%u\n", acqTimeMs, mappingPoints, numMCAChannels);
    startClock();
    callId = start_map(danteIdentifier, acqTimeMs, mappingPoints, numMCAChannels);
    waitReply(callId, danteReply);
    printf("start_map complete\n");

    bool anyBoardBusy = true;
    while (anyBoardBusy) {
        mySleep(pollTimeMs/1000.);
        anyBoardBusy = false;
        uint32_t minAvailable;
        for (board=0; board<numBoards; board++) {
            bool lastDataReceived;
            isLastDataReceived(danteIdentifier, board, lastDataReceived);
            if (lastDataReceived) {
                printf("%7.4f Acquisition complete on board %d\n", elapsed_time(), board);
            } else {
                anyBoardBusy = true;
            }
            if (!getAvailableData(danteIdentifier, board, numAvailable[board])) {
                printf("%s::%s error calling getAvailableData\n", driverName, functionName);
            }
            printf("%7.4f getAvailableData on board %d numAvailable=%d\n", elapsed_time(), board, numAvailable[board]);
            if (board == 0) {
                minAvailable = numAvailable[board];
            } else if (numAvailable[board] < minAvailable) {
                minAvailable = numAvailable[board];
            }
        }
        if (minAvailable == 0) continue;
        printf("%7.4f Minimum number of spectra available=%d\n", elapsed_time(), minAvailable);
        uint32_t spectraSize = numMCAChannels;
        for (board=0; board<numBoards; board++) {
            uint16_t *pMappingMCAData                = (uint16_t *) malloc(minAvailable * numMCAChannels * sizeof(uint16_t));
            uint32_t *pSpectraId                     = (uint32_t *) malloc(minAvailable * sizeof(uint32_t));
            struct mappingStats *pMappingStats       = (mappingStats *) malloc(minAvailable * sizeof(struct mappingStats));
            struct mappingAdvStats *pMappingAdvStats = (mappingAdvStats *)malloc(minAvailable * sizeof(struct mappingAdvStats));

            if (!getAllData(danteIdentifier, board, pMappingMCAData, pSpectraId,
                            (double *)pMappingStats, (uint64_t*)pMappingAdvStats, spectraSize, minAvailable)) {
                printf("%s::%s error calling getAllData\n", driverName, functionName);
            }
            currentPixel[board] += minAvailable;
            printf("%7.4f getAllData board=%d, read %d spectra OK, pSpectraId[0]=%u, currentPixel=%d\n",
                elapsed_time(), board, minAvailable, pSpectraId[0], currentPixel[board]);
            free(pMappingMCAData);
            free(pSpectraId);
            free(pMappingStats);
            free(pMappingAdvStats);
        }
    }
    for (int board=0; board<numBoards; board++) {
        getAvailableData(danteIdentifier, board, numAvailable[board]);
        printf("%7.4f getAvailableData on board %d numAvailable=%d\n", elapsed_time(), board, numAvailable[board]);
    }
    printf("%7.4f Spectra collected on each board\n", elapsed_time());
    for (int board=0; board<numBoards; board++) {
        printf("  Board %d, number collected=%d\n", board, currentPixel[board] + numAvailable[board]);
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
