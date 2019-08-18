// Test Library.cpp : Defines the entry point for the console application.
//

#include <inttypes.h>
#ifdef POLLINGLIB
	#include "DLL_DPP_Polling.h"
#else
	#include "DLL_DPP_Callback.h"
#endif
#include <assert.h>
#include <ctime>
#include <vector>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <thread>
#include <map>
#include <future>
#include <stdio.h>

#ifdef WIN32
//#include "stdafx.h"
	#include <direct.h>
char* GetCurrentWorkingDir(void) {
	char buff[FILENAME_MAX];
	_getcwd(buff, FILENAME_MAX);
	return buff;
}
#endif



uint32_t answer_data[10];
#ifdef POLLINGLIB
	#define WAIT_ANS_RETRIES 100
	uint32_t answer_full[10];
	bool wait_answer(uint32_t id)
	{
		uint32_t retries = 0, result = 0, length = 0;
		while (retries < WAIT_ANS_RETRIES)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			result = GetAnswersDataLength(length);
			if (result && length >= 4)
			{
				// Answer received.
				result = GetAnswersData(length, answer_full);
				uint32_t call_id = answer_full[0];
				uint32_t type = answer_full[1];
				uint32_t data_length = answer_full[2];
				for (int32_t i = 0; i < data_length; i++)
					answer_data[i] = answer_full[i + 3];
				if (result)
				{
					if (call_id != id)
					{
						// Wrong call id.
						return false;
					}
					else if (type == 0)
					{
						// Error answer.
						return false;
					}
					else if (type == 2)
					{
						// Answer of a write command.
						return answer_data[0] == 1;
					}
					else if ((type == 1) && (length == data_length + 3))
					{
						// Answer of a read command. Let the user read the data from the global var answer_data[].
						return true;
					}
				}
				else
					return false;
			}
			else
				result = false;
			retries++;
		}
		return false;
	}
#else
	#define MS_WAIT_ANS 6000 // Milliseconds to wait the answer. 
	typedef std::map<uint32_t, std::promise<bool>> promise_map_t;
	promise_map_t promise_map;
	void callback_func(uint16_t type, uint32_t call_id, uint32_t length, uint32_t* data)
	{
		std::cout << "Answer received. Type: " << type << ". Call id: " << call_id << ". Length: " << length << ". Data: ";
		for (uint32_t i = 0; i < length; i++)
		{
			std::cout << data[i] << " ";
			answer_data[i] = data[i];
		}
		std::cout << ".\n";
		promise_map_t::iterator promise_it(promise_map.find(call_id));
		if (promise_it != promise_map.end())
		{
			if (type == 0)
				promise_it->second.set_value(false);
			else if ((type == 2 && data[0] == 1) || (type == 1))
				promise_it->second.set_value(true);
			else
				promise_it->second.set_value(false);
		}
	}
	bool wait_answer(uint32_t id)
	{
		promise_map.insert(std::make_pair(id, std::promise<bool>()));
		std::future<bool> current_future(promise_map.at(id).get_future());
		switch (current_future.wait_for(std::chrono::milliseconds(MS_WAIT_ANS)))
		{
		case std::future_status::deferred:
			// This should not happen.
			promise_map.erase(id);
			return false;
		case std::future_status::timeout:
			// No answer received, remove promise.
			promise_map.erase(id);
			return false;
		case std::future_status::ready:
			// Data ready, read it.
			break;
		}
		bool answer = current_future.get();
		promise_map.erase(id);
		return answer;
	}
#endif

int main(int argc, char* argv[])
{
	bool overall_result = true;

	// Test getLastError() Start.
	std::cout << "Test getLastError().\n";
	uint16_t error_code;
	if (getLastError(error_code))
	{
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test getLastError(): Ok.\n\n";
	}
	else
	{
		std::cout << "Test getLastError(): Failed.\n\n";
		overall_result = false;
	}
	// Test getLastError() End.

	// Test resetLastError() Start.
	std::cout << "Test resetLastError().\n";
	if (resetLastError())
	{
		getLastError(error_code);
		if (error_code == DLL_NO_ERROR)
		{
			std::cout << "Error code is: " << error_code << ".\n";
			std::cout << "Test resetLastError(): Ok.\n\n";
		}
		else
		{
			std::cout << "Test resetLastError(): Failed.\n\n";
			overall_result = false;
		}
	}
	else
	{
		std::cout << "Test resetLastError(): Failed.\n\n";
		overall_result = false;
	}
	// Test resetLastError() End.

#ifndef POLLINGLIB
	// Test register_callback() Start.
	std::cout << "Test getLastError().\n";
	if (register_callback(callback_func))
	{
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test register_callback(): Ok.\n\n";
	}
	else
	{
		getLastError(error_code);
		std::cout << "Test register_callback(): Failed.\n\n";
		overall_result = false;
	}
	// Test register_callback() End.
#endif

	// Test initLibrary() Start.
	std::cout << "Test initLibrary().\n";
	if (InitLibrary()) 
		std::cout << "Test initLibrary(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test initLibrary(): Failed.\n\n";
		overall_result = false;
	}
	// Test initLibrary() End.

	// Test libVersion() Start.
	std::cout << "Test libVersion().\n";
	bool result;
	uint32_t version_size = 20;
	char version[20];
	result = libVersion(version, version_size);	
	if (result)
	{
		std::cout << "Version is: " << version << std::endl;
		std::cout << "Test libVersion(): Ok.\n\n";
	}
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test libVersion(): Failed.\n\n";
		overall_result = false;
	}
	// Test libVersion() End.

	// Test add_to_query() Start.
	std::cout << "Test add_to_query().\n";
	char ip[] = "164.54.160.232";	
	result = add_to_query(ip);
	if (result)
		std::cout << "Test add_to_query(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test add_to_query(): Failed.\n\n";
		overall_result = false;
	}
	// Test add_to_query() End.

	// Test remove_from_query() Start.
	std::cout << "Test remove_from_query().\n";
	result = remove_from_query(ip);
	if (result)
		std::cout << "Test remove_from_query(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test remove_from_query(): Failed.\n\n";
		overall_result = false;
	}
	// Test remove_from_query() End.

	// If the device is connected via Ethernet, add it's IP to the query.
	if (add_to_query(ip))
	{
		std::cout << "Added IP: " << std::string(ip) << " to the query.\n\n";
		std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait for the boards to be found by the DLL.
	}
	else
	{
		std::cout << "Error adding the IP to thew query.\n\n";
		overall_result = false;
	}	

	// Test get_dev_number() Start.
	std::cout << "Test get_dev_number().\n";
	uint16_t dev_nb;
	result = get_dev_number(dev_nb);
	if (result)
	{
		std::cout << "Number of recognized boards: " << dev_nb << ".\n";
		std::cout << "Test get_dev_number(): Ok.\n\n";
	}
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test get_dev_number(): Failed.\n\n";
		overall_result = false;
	}
	// Test get_dev_number() End.

	// Test get_ids() Start.
	std::cout << "Test get_ids().\n";
	char identifier[16]; uint16_t nb = 0; uint16_t id_size = 16;
	result = get_ids(identifier, nb, id_size);
	if (result)
	{
		std::cout << "Identifier of recognized board: " << identifier << ".\n";
		std::cout << "Test get_ids(): Ok.\n\n";
	}
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test get_ids(): Failed.\n\n";
		overall_result = false;
	}
	// Test get_ids() End.
	
	// Test get_boards_in_chain() Start.
	std::cout << "Test get_boards_in_chain().\n";
	uint16_t chain = 1;
	result = get_boards_in_chain(identifier, chain);
	if (result)
	{
		std::cout << "Number of boards in the chain: " << chain << ".\n";
		std::cout << "Test get_boards_in_chain(): Ok.\n\n";
	}
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test get_boards_in_chain(): Failed.\n\n";
		overall_result = false;
	}
	// Test get_boards_in_chain() End.

	uint32_t call_id;

	// Eventually wait a little bit for daisy chain synchronization and ask again for connected systems.
	std::this_thread::sleep_for(std::chrono::seconds(1));
	nb = 0; id_size = 16;
	result = get_ids(identifier, nb, id_size);
	if (result)
	{
		std::cout << "Identifier of recognized board: " << identifier << ".\n";
		result = get_boards_in_chain(identifier, chain);
		if (result)
			std::cout << "Number of boards in the chain: " << chain << ".\n";
		else
			std::cout << "Error when asking boards in chain.\n\n";
	}
	else
		std::cout << "No board recognized.\n\n";	

	// Test getFirmware() Start.
	std::cout << "Test getFirmware().\n";
	result = true;
	for (int32_t i = 0; i < chain; i++)
	{
		call_id = getFirmware(identifier, i);
		result = result && (call_id > 0);
		if (call_id > 0)
			result = result && wait_answer(call_id);
		if (result)
			std::cout << "Firmware version of board " << i << " is: "
					  << answer_data[0] << "."
					  << answer_data[1] << "."
					  << answer_data[2] << ".\n";
	}
	if (result)
		std::cout << "Test getFirmware(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test getFirmware(): Failed.\n\n";
		overall_result = false;
	}
	// Test getFirmware() End.

	// Test write_IP_configuration() Start.
	std::cout << "Test write_IP_configuration().\n";
	char mask[] = "255.255.255.0";
	char gw[] = "10.96.0.1";
	call_id = write_IP_configuration(identifier, ip, mask, gw);
	if (call_id > 0)
		result = wait_answer(call_id);
	if (result && call_id > 0)
		std::cout << "Test write_IP_configuration(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test write_IP_configuration(): Failed.\n\n";
		overall_result = false;
	}
	// Test write_IP_configuration() End.
	
	// Test configure() Start.
	std::cout << "Test configure().\n";
	configuration cfg;
	cfg.fast_filter_thr = 50;
	cfg.energy_filter_thr = 0;
	cfg.max_risetime = 0;
	cfg.baseline_samples = 0;
	cfg.edge_peaking_time = 1;
	cfg.max_peaking_time = 64;
	cfg.edge_flat_top = 1;
	cfg.flat_top = 7;
	cfg.gain = 1;
	cfg.peaking_time = 64;
	cfg.reset_recovery_time = 200;
	cfg.inverted_input = false;
	cfg.zero_peak_freq = 1;
	for (int32_t i = 0; i < chain; i++)
	{
		call_id = configure(identifier, i, cfg);
		result = result && (call_id > 0);
		if (call_id > 0)
			result = result && wait_answer(call_id);
	}
	if (result)
		std::cout << "Test configure(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test configure(): Failed.\n\n";
		overall_result = false;
	}		
	// Test configure() End.

	// Test configure_offset() Start.
	std::cout << "Test configure_offset().\n";
	configuration_offset cfg_offset;
	cfg_offset.offset_val1 = 128;
	cfg_offset.offset_val2 = 128;
	for (int32_t i = 0; i < chain; i++)
	{
		call_id = configure_offset(identifier, i, cfg_offset);
		result = result && (call_id > 0);
		if (call_id > 0)
			result = result && wait_answer(call_id);
	}
	if (result)
		std::cout << "Test configure_offset(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test configure_offset(): Failed.\n\n";
		overall_result = false;
	}
	// Test configure_offset() End.

	// Test configure_baseline() Start.
	std::cout << "Test configure_baseline().\n";
	uint16_t base_vector[] = { 0,0,16,32,64,128,256,512,1,0,0,0,0,0,0,0 };
	for (int32_t i = 0; i < chain; i++)
	{
		result = result && configure_baseline(identifier, i, base_vector);
	}
	if (result)
		std::cout << "Test configure_baseline(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test configure_baseline(): Failed.\n\n";
		overall_result = false;
	}
	// Test configure_baseline() End.

	// Test configure_timestamp_delay() Start.
	std::cout << "Test configure_timestamp_delay().\n";
	int32_t timestamp_delay = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		call_id = configure_timestamp_delay(identifier, i, timestamp_delay);
		result = result && (call_id > 0);
		if (call_id > 0)
			result = result && wait_answer(call_id);
	}
	if (result)
		std::cout << "Test configure_timestamp_delay(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test configure_timestamp_delay(): Failed.\n\n";
		overall_result = false;
	}
	// Test configure_timestamp_delay() End.

	// Test isRunning_system() Start.
	std::cout << "Test isRunning_system().\n";
	bool running = false;
	for (int32_t i = 0; i < chain; i++)
	{
		call_id = isRunning_system(identifier, i);
		result = result && (call_id > 0);
		if (call_id > 0)
			result = result && wait_answer(call_id);
		if (result)
			running = running || answer_data[0];
	}
	if (result && !running)
		std::cout << "Test isRunning_system(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test isRunning_system(): Failed.\n\n";
		overall_result = false;
	}
	// Test isRunning_system() End.

	// Test clear_chain() Start.
	std::cout << "Test clear_chain().\n";
	if (clear_chain(identifier))
		std::cout << "Test clear_chain(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test clear_chain(): Failed.\n\n";
		overall_result = false;
	}
	// Test clear_chain() End. 

	// Test clear_board() Start.
	std::cout << "Test clear_board().\n";
	for (int32_t i = 0; i < chain; i++)
		result = result && clear_board(identifier, i);
	if (result)
		std::cout << "Test clear_board(): Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test clear_board(): Failed.\n\n";
		overall_result = false;
	}
	// Test clear_board() End. 

	// Test Spectrum Acquisition - Timed - 4096 bins. Start.
	std::cout << "Test Spectrum Acquisition - Timed - 4096 bins.\n";
	double time = 2; // seconds.
	uint16_t bins = 4096;
	result = true; running = true;
	call_id = start(identifier, time, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint64_t values4k[4096];
	uint32_t id;
	statistics stats;
	uint32_t spectra_size = 4096;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getData(identifier, i, values4k, id, stats, spectra_size))
			result = false;
		if (values4k[95] == 0)
		{
			// There should be some counts here due to the zero peak.
			result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Timed - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Timed - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Timed - 4096 bins. End.

	// Test Spectrum Acquisition - Timed - 2048 bins. Start.
	std::cout << "Test Spectrum Acquisition - Timed - 2048 bins.\n";
	time = 2; // seconds.
	bins = 2048;
	result = true; running = true;
	call_id = start(identifier, time, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint64_t values2k[2048];
	spectra_size = 2048;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getData(identifier, i, values2k, id, stats, spectra_size))
			result = false;
		if (values2k[47] == 0)
		{
			// There should be some counts here due to the zero peak.
			result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Timed - 2048 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Timed - 2048 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Timed - 2048 bins. End.

	// Test Spectrum Acquisition - Timed - 1024 bins. Start.
	std::cout << "Test Spectrum Acquisition - Timed - 1024 bins.\n";
	time = 2; // seconds.
	bins = 1024;
	result = true; running = true;
	call_id = start(identifier, time, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint64_t values1k[1024];
	spectra_size = 1024;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getData(identifier, i, values1k, id, stats, spectra_size))
			result = false;
		if (values1k[23] == 0)
		{
			// There should be some counts here due to the zero peak.
			result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Timed - 1024 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Timed - 1024 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Timed - 1024 bins. End.

	// Test Spectrum Acquisition - Free running - 4096 bins. Start.
	std::cout << "Test Spectrum Acquisition - Free running - 4096 bins.\n";
	time = 0; // Free running.
	bins = 4096;
	result = true; running = true;
	call_id = start(identifier, time, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				call_id = stop(identifier);
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;	
	spectra_size = 4096;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getData(identifier, i, values4k, id, stats, spectra_size))
			result = false;
		if (values4k[95] == 0)
		{
			// There should be some counts here due to the zero peak.
			result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Free running - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Free running - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Free running - 4096 bins. End.

	// Test Map Acquisition - Timed - 4096 bins. Start.
	std::cout << "Test Map Acquisition - Timed - 4096 bins.\n";
	uint32_t sptime = 100; // milliseconds.
	uint32_t points = 20;
	bins = 4096;
	result = true; running = true;
	call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint16_t* values_map;
	uint32_t* id_map;
	double* stats_map;
	uint64_t* advstats_map;
	spectra_size = 4096;
	uint32_t data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * 4096];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 18];

			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;
			if (values_map[95] == 0 || values_map[95 + 4096] == 0) // bin 96 of first two spectra.
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Timed - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Map Acquisition - Timed - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Map Acquisition - Timed - 4096 bins. End.

	// Test Map Acquisition - Timed - 2048 bins. Start.
	std::cout << "Test Map Acquisition - Timed - 2048 bins.\n";
	sptime = 100; // milliseconds.
	points = 20;
	bins = 2048;
	result = true; running = true;
	call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	spectra_size = 2048;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * 2048];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 18];

			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;
			if (values_map[47] == 0 || values_map[47 + 2048] == 0) // bin 96 of first two spectra.
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Timed - 2048 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Map Acquisition - Timed - 2048 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Map Acquisition - Timed - 2048 bins. End.

	// Test Map Acquisition - Timed - 1024 bins. Start.
	std::cout << "Test Map Acquisition - Timed - 1024 bins.\n";
	sptime = 100; // milliseconds.
	points = 20;
	bins = 1024;
	result = true; running = true;
	call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	spectra_size = 1024;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * 1024];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 18];

			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;
			if (values_map[23] == 0 || values_map[23 + 1024] == 0) // bin 96 of first two spectra.
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Timed - 1024 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Map Acquisition - Timed - 1024 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Map Acquisition - Timed - 1024 bins. End.

	// Test Map Acquisition - Free running - 4096 bins. Start.
	std::cout << "Test Map Acquisition - Free running - 4096 bins.\n";
	sptime = 100; // milliseconds.
	points = 0; // Free running.
	bins = 4096;
	result = true; running = true;
	call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				call_id = stop(identifier);
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	spectra_size = 4096;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * 4096];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 18];

			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;
			if (values_map[95] == 0 || values_map[95 + 4096] == 0) // bin 96 of first two spectra.
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Free running - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Map Acquisition - Free running - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Map Acquisition - Free running - 4096 bins. End.

	// Test Waveform Acquisition. Start.
	std::cout << "Test Waveform Acquisition.\n";
	time = 1; // seconds.
	result = true; running = true;
	uint16_t dec_ratio = 1;
	unsigned int trig_mask = 1;
	unsigned int trig_level = 0;
	uint16_t length = 1;
	call_id = start_waveform(identifier,0,dec_ratio,trig_mask,trig_level,time,length);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint16_t wave[16384];
	unsigned int data_size = 16384;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getWaveData(identifier, i, wave, data_size))
			result = false;
		if (!(wave[0]>30000 && wave[0]<34000))
		{
			// With grounded input the signal should be at 32000 ADC bins more or less.
			result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Waveform Acquisition: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test Waveform Acquisition: Failed.\n\n";
		overall_result = false;
	}
	// Test Waveform Acquisition. End.

	// Test List Mode Acquisition - Timed. Start.
	std::cout << "Test List Mode Acquisition - Timed.\n";
	time = 2; // seconds.
	result = true; running = true;
	call_id = start_list(identifier,time);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint64_t* list_data;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			list_data = new uint64_t[data_number];

			if (!getData(identifier, i, list_data, id, stats, data_number))
				result = false;
			if (!(((list_data[0] & 0x000000000000FFFF) < 98 * 4) && ((list_data[0] & 0x000000000000FFFF) > 94 * 4)))
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] list_data;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test List Mode Acquisition - Timed: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test List Mode Acquisition - Timed: Failed.\n\n";
		overall_result = false;
	}
	// Test List Mode Acquisition - Timed. End.

	// Test List Mode Acquisition - Free running. Start.
	std::cout << "Test List Mode Acquisition - Free running.\n";
	time = 0; // Free running.
	result = true; running = true;
	call_id = start_list(identifier, time);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				call_id = stop(identifier);
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			list_data = new uint64_t[data_number];

			if (!getData(identifier, i, list_data, id, stats, data_number))
				result = false;
			if (!(((list_data[0] & 0x000000000000FFFF) < 98 * 4) && ((list_data[0] & 0x000000000000FFFF) > 94 * 4)))
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] list_data;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test List Mode Acquisition - Free running: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test List Mode Acquisition - Free running: Failed.\n\n";
		overall_result = false;
	}
	// Test List Mode Acquisition - Timed. End.

	// Test List Wave Mode Acquisition - Timed. Start.
	std::cout << "Test List Wave Mode Acquisition - Timed.\n";
	time = 2; // seconds.
	dec_ratio = 1;
	uint16_t list_wave_length = 400;
	uint16_t list_wave_offset = 0;
	result = true; running = true;
	call_id = start_listwave(identifier, time, dec_ratio, list_wave_length, list_wave_offset);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
	}
	else
		result = false;
	uint64_t* listw_values;
	uint64_t* listw_wave_values = 0;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			listw_values = new uint64_t[data_number*(length + 1)];

			if (!getListWaveData(identifier, i, listw_values, listw_wave_values, id, stats, data_number))
				result = false;
			if (!(((listw_values[0] & 0x000000000000FFFF) < 98 * 4) && ((listw_values[0] & 0x000000000000FFFF) > 94 * 4)) && (((listw_values[1] & 0x000000000000FFFF) < 36000) && ((listw_values[1] & 0x000000000000FFFF) > 30000)))
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] listw_values;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test List Wave Mode Acquisition - Timed: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test List Wave Mode Acquisition - Timed: Failed.\n\n";
		overall_result = false;
	}
	// Test List Mode Acquisition - Timed. End.

	// Test List Wave Mode Acquisition - Free running. Start.
	std::cout << "Test List Wave Mode Acquisition - Free running.\n";
	time = 0; // Free running.
	dec_ratio = 1;
	list_wave_length = 400;
	list_wave_offset = 0;
	result = true; running = true;
	call_id = start_listwave(identifier, time, dec_ratio, list_wave_length, list_wave_offset);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				call_id = isRunning_system(identifier, 0);
				if (wait_answer(call_id))
				{
					std::cout << "Acquisition in progress.\n";
					running = answer_data[0];
				}
				else
					result = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				call_id = stop(identifier);
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	data_number = 0;
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			listw_values = new uint64_t[data_number*(length + 1)];

			if (!getListWaveData(identifier, i, listw_values, listw_wave_values, id, stats, data_number))
				result = false;
			if (!(((listw_values[0] & 0x000000000000FFFF) < 98 * 4) && ((listw_values[0] & 0x000000000000FFFF) > 94 * 4)) && (((listw_values[1] & 0x000000000000FFFF) < 36000) && ((listw_values[1] & 0x000000000000FFFF) > 30000)))
			{
				// There should be some counts here due to the zero peak.
				result = false;
			}
			delete[] listw_values;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test List Wave Mode Acquisition - Free running: Ok.\n\n";
	else
	{
		getLastError(error_code);
		std::cout << "Error code is: " << error_code << ".\n";
		std::cout << "Test List Wave Mode Acquisition - Free running: Failed.\n\n";
		overall_result = false;
	}
	// Test List Mode Acquisition - Timed. End.

	
	
	// Test CloseLibrary() Start.
	std::cout << "Test CloseLibrary().\n";
	if (CloseLibrary())
		std::cout << "Test CloseLibrary(): Ok.\n\n";
	else
	{
		std::cout << "Test CloseLibrary(): Failed.\n\n";
		overall_result = false;
	}
	// Test CloseLibrary() End.

	if (overall_result)
	{
		std::cout << "All Tests Completed.\n\n";
		std::this_thread::sleep_for(std::chrono::seconds(50));
		return 1;
	}
	else
	{
		std::cout << "At least one test failed.\n\n";
		std::this_thread::sleep_for(std::chrono::seconds(50));
		return 0;
	}
}