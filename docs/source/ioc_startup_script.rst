IOC startup script
------------------
The command to configure a Dante in the startup script is::

  DanteConfig(portName, ipAddress, totalBoards, maxMemory)

``portName`` is the name for the Dante port driver

``ipAddress`` is the IP address of the Dante 

``totalBoards`` is the total number of boards in the Dante system, including those that may be disabled.

``maxMemory`` is the maximum amount of memory the NDArrayPool is allowed to allocate.  0 means unlimited.


