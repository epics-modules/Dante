List mode
---------
These are the records for list mode.  They are contained in dante.template.

.. cssclass:: table-bordered table-striped table-hover
.. list-table::
   :header-rows: 1
   :widths: auto

   * - EPICS record names
     - Record types
     - drvInfo string
     - Description
   * - CurrentPixel
     - longin
     - DanteCurrentPixel
     - In List mode this is the total number of x-ray events received so far.
   * - ListBufferSize, ListBufferSize_RBV
     - longout, longin
     - DanteListBufferSize
     - The number of x-ray events per buffer in list mode. 
       Once this number of events has been received the events read from the Dante
       stored in NDArrays, and callbacks are done to any registered plugins.

List mode events are 64-bit unsigned integers.

- Bits 0 to 15 are the x-ray energy, i.e. ADC value.
- Bits 16 to 17 are not used.
- Bits 18 to 61 are the timestamp in 8 ns units.
- Bits 62 and 63 are not used.

In list mode the x-ray events are copied into NDArrays.
The data type of the NDArrays is NDUInt64, and the NDArrayDimensions are [ListBufferSize, NumBoards].
For a 1-channel Dante NumBoards is 1.

The run-time statistics for ListBufferSize events are copied into NDAttributes attached to each
NDArray. The attribute names contain the board number, for example "RealTime_0".
Note that these statistics are cummulative for the entire acquisition, not just since the
last time the event buffer was read.
By making ListBufferSize smaller one obtains a more frequent sampling of these statistics.

These statistics also update the run-time statistics records described above, so there is feedback
while the list mode acquisition is in progress.

The first NumMCAChannels events are copied to the buffer for the MCA record for each board.
In this case the MCA record will not contain an x-ray spectrum, but rather will contain the x-ray
energy in ADC units on the vertical axis and the event number on the horizontal axis.

The NDArrays can be used by most of the standard areaDetector plugins.  For example, they can be streamed
to HDF5 or TIFF files.  List-mode data cannot be written to a netCDF file, because the netCDF classic format 
does not support 64-bit integer data types.

The following is an IDL procedure to read the List mode data from an HDF5 file into two arrays, "energy" and "time"::

  pro read_dante_list_data, filename, energy, time
     data = read_nd_hdf5(filename)
     energy = uint(data and 'ffff'x)
     time = double(ishft((data and '3ffffffffffc0000'x), -18))*8e-9
  end


read_nd_hdf5_ is a function that reads an HDF5 file written by the areaDetector NDFileHDF5 plugin::

  function read_nd_hdf5, file, range=range, dataset=dataset
    if (n_elements(dataset) eq 0) then dataset = '/entry/data/data'
    file_id = h5f_open(file)
    dataset_id = h5d_open(file_id, dataset)
    data = h5d_read(dataset_id)
    h5d_close, dataset_id
    h5f_close, file_id
    return, data
  end


The following is a plot of the energy events for the first 1 second of that data, using this IDL command::

  IDL> p = plot(time, energy, xrange=[0,1], yrange=[0,20000], linestyle='none', symbol='plus')

.. figure:: dante_idl_list_plot.png
    :align: center

