pro read_dante_list_data, filename, energy, time, netcdf=netcdf
   if (keyword_set(netcdf)) then begin
     data = read_nd_netcdf(filename)
   endif else begin
     data = read_nd_hdf5(filename)
   endelse
   energy = uint(data and 'ffff'x)
   time = double(ishft((data and '3ffffffffffc0000'x), -18))*8e-9
end
