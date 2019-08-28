TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

DBD += mcaDanteSupport.dbd

USR_CXXFLAGS_Linux += -std=c++11 -fmax-errors=20
LIBRARY_IOC_Linux += mcaDante
LIBRARY_IOC_WIN32   += mcaDante

mcaDante_SRCS += dante.cpp
mcaDante_LIBS += asyn 
mcaDante_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_NAME = mcaDanteApp
PROD_IOC_Linux  += $(PROD_NAME)
PROD_IOC_WIN32  += $(PROD_NAME)
DBD += $(PROD_NAME).dbd
$(PROD_NAME)_DBD += mcaSupport.dbd
$(PROD_NAME)_DBD += mcaDanteSupport.dbd

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
$(PROD_NAME)_SRCS   += $(PROD_NAME)_registerRecordDeviceDriver.cpp
$(PROD_NAME)_SRCS   += $(PROD_NAME)Main.cpp
$(PROD_NAME)_LIBS   += mca
$(PROD_NAME)_LIBS   += mcaDante
$(PROD_NAME)_SYS_LIBS_Linux += XGL_DPP
$(PROD_NAME)_LIBS_WIN32     += XGL_DPP
$(PROD_NAME)_LIBS   += seq pv
$(PROD_NAME)_SYS_LIBS_Linux += python2.7
USR_LDFLAGS_Linux = -L/usr/lib/python2.7/config-x86_64-linux-gnu/


#=============================
PROD_IOC_Linux += DPP_Test
PROD_IOC_WIN32 += DPP_Test
DPP_Test_SRCS  += DPP_Test.cpp
DPP_Test_SYS_LIBS_Linux  += XGL_DPP
DPP_Test_LIBS_WIN32      += XGL_DPP
DPP_Test_SYS_LIBS_Linux += python2.7

include $(ADCORE)/ADApp/commonDriverMakefile

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
