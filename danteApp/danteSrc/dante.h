#ifndef NDDXP_H
#define NDDXP_H

#ifdef __cplusplus
extern "C"
{
#endif

int danteConfig(const char *portName, const char *ipAddress, int nChannels, int maxBuffers, size_t maxMemory);

#ifdef __cplusplus
}
#endif

#endif
