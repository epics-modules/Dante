< envPaths

# Tell EPICS all about the record types, device-support modules, drivers,
# etc. in this build from dxpApp
dbLoadDatabase("$(DANTE)/dbd/mcaDanteApp.dbd")
mcaDanteApp_registerRecordDeviceDriver(pdbbase)

# The search path for database files
epicsEnvSet("EPICS_DB_INCLUDE_PATH", "$(ADCORE)/db")

# The detector prefix
epicsEnvSet("PREFIX", "Dante:")
epicsEnvSet("PORT", "DANTE1")

# DanteConfig(portName, ipAddress, numDetectors, maxBuffers, maxMemory)
DanteConfig("DANTE1",  164.54.160.232, 1, 0, 0)

dbLoadTemplate("dante.substitutions")

# Create a netCDF file saving plugin
NDFileNetCDFConfigure("NetCDF1", 100, 0, $(PORT), 0)
dbLoadRecords("$(ADCORE)/db/NDFileNetCDF.template","P=$(PREFIX),R=netCDF1:,PORT=NetCDF1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

# Create a TIFF file saving plugin
NDFileTIFFConfigure("TIFF1", 20, 0, "$(PORT)", 0)
dbLoadRecords("$(ADCORE)/db/NDFileTIFF.template",  "P=$(PREFIX),R=TIFF1:,PORT=TIFF1,ADDR=0,TIMEOUT=1,NDARRAY_PORT=$(PORT)")

### Scan-support software
# crate-resident scan.  This executes 1D, 2D, 3D, and 4D scans, and caches
# 1D data, but it doesn't store anything to disk.  (See 'saveData' below for that.)
dbLoadRecords("$(SSCAN)/sscanApp/Db/scan.db","P=$(PREFIX),MAXPTS1=2000,MAXPTS2=1000,MAXPTS3=10,MAXPTS4=10,MAXPTSH=2048")

# Load devIocStats
dbLoadRecords("$(DEVIOCSTATS)/db/iocAdminSoft.db", "IOC=$(PREFIX)")

# Setup for save_restore
< save_restore.cmd
save_restoreSet_status_prefix("$(PREFIX)")
dbLoadRecords("$(AUTOSAVE)/asApp/Db/save_restoreStatus.db", "P=$(PREFIX)")

iocInit

### Start up the autosave task and tell it what to do.
# Save settings every thirty seconds
create_monitor_set("auto_settings.req", 30, "P=$(PREFIX), R=dante1:, M=mca1")
