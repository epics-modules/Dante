pro read_dante_list_data, filename, energy, time
   raw = read_nd_netcdf(filename)
   data = ulong64(raw, 0, n_elements(raw)/8)
   energy = uint(data and 'ffff'x)
   time = double(ishft((data and '3ffffffffffc0000'x), -18))*8e-9
end
