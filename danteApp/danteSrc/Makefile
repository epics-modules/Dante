TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

DBD += mcaDanteSupport.dbd
PROD_NAME = mcaDanteApp

ifeq (x86_64, $(findstring x86_64, $(ARCH_CLASS)))
  USR_CXXFLAGS_Linux += -std=c++11
  LIBRARY_IOC_Linux += mcaDante
  PROD_IOC_Linux  += $(PROD_NAME)
  PROD_IOC_Linux += DPP_Test
  PROD_IOC_Linux += test_mapping
endif
ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
  LIBRARY_IOC_WIN32   += mcaDante
  PROD_IOC_WIN32  += $(PROD_NAME)
  PROD_IOC_WIN32 += DPP_Test
  PROD_IOC_WIN32 += test_mapping
endif

LIB_SRCS += dante.cpp
LIB_SRCS += danteMED.st

LIB_LIBS           += asyn 
LIB_LIBS           += ADBase
LIB_LIBS           += seq pv
LIB_LIBS           += $(EPICS_BASE_IOC_LIBS)
LIB_LIBS_WIN32     += XGL_DPP
LIB_SYS_LIBS_Linux += XGL_DPP

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


#=============================
DPP_Test_SRCS  += DPP_Test.cpp
DPP_Test_SYS_LIBS_Linux  += XGL_DPP
DPP_Test_LIBS_WIN32      += XGL_DPP

#=============================
test_mapping_SRCS  += test_mapping.cpp
test_mapping_SYS_LIBS_Linux  += XGL_DPP
test_mapping_LIBS_WIN32      += XGL_DPP

include $(ADCORE)/ADApp/commonDriverMakefile

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
