TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaDante.dbd
DBD += mcaDanteSupport.dbd

USR_CXXFLAGS_Linux += -std=c++11 -fmax-errors=20
LIBRARY_IOC_Linux += mcaDante
PROD_IOC_Linux    += mcaDanteApp

LIBRARY_IOC_WIN32   += mcaDante
PROD_IOC_WIN32      += mcaDanteApp

mcaDante_SRCS += dante.cpp

mcaDante_LIBS += asyn 
mcaDante_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaDanteApp_SRCS   += mcaDante_registerRecordDeviceDriver.cpp
mcaDanteApp_SRCS   += mcaDanteMain.cpp
mcaDanteApp_LIBS   += mca
mcaDanteApp_LIBS   += mcaDante
mcaDanteApp_LIBS   += XGL_DPP
mcaDanteApp_LIBS   += seq pv
mcaDanteApp_SYS_LIBS_Linux += python2.7
USR_LDFLAGS_Linux = -L/usr/lib/python2.7/config-x86_64-linux-gnu/


#=============================
PROD_IOC_Linux += DPP_Test
PROD_IOC_WIN32 += DPP_Test
DPP_Test_SRCS  += DPP_Test.cpp
DPP_Test_LIBS  += XGL_DPP
DPP_Test_SYS_LIBS_Linux += python2.7


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
