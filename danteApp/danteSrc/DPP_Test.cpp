// Test Library.cpp : Defines the entry point for the console application.
//

#include "inttypes.h"
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
		//std::cout << "Answer received. Type: " << type << ". Call id: " << call_id << ". Length: " << length << ". Data: ";
		for (uint32_t i = 0; i < length; i++)
		{
			//std::cout << data[i] << " ";
			answer_data[i] = data[i];
		}
		//std::cout << ".\n";
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

	bool check_func_result(char* func_to_test_str, uint32_t call_id, int16_t board = -1)
	{
		std::cout << "Test " << func_to_test_str << "\n";
		
		if (call_id > 0)
		{
			if (wait_answer(call_id))
			{
				if(board == -1)
					std::cout << "Test " << func_to_test_str << ": Ok.\n";
				else
					std::cout << "Test " << func_to_test_str << " on board " << board << ": Ok.\n";

				std::cout << "\n";
				return true;
			}
			else
			{
				uint16_t err_code;
				getLastError(err_code);
				if(board == -1)
					std::cout << "Test " << func_to_test_str << ": Failed. Error code is : " << err_code << ".\n";
				else
					std::cout << "Test " << func_to_test_str << " on board " << board << ": Failed. Error code is : " << err_code << ".\n";

				std::cout << "\n";
				return false;
			}
		}
		else
		{
			uint16_t err_code;
			getLastError(err_code);
			if (board == -1)
				std::cout << "Test " << func_to_test_str << ": Failed. Error code is : " << err_code << ".\n";
			else
				std::cout << "Test " << func_to_test_str << " on board " << board << ": Failed. Error code is: " << err_code << ".\n";

			std::cout << "\n";
			return false;
		}
	}

	bool check_func_result(char* func_to_test_str, bool func_to_test_res, int16_t board = -1)
	{
		std::cout << "Test " << func_to_test_str << "\n";

		if (func_to_test_res)
		{
			if (board == -1)
				std::cout << "Test " << func_to_test_str << ": Ok.\n";
			else
				std::cout << "Test " << func_to_test_str << " on board " << board << ": Ok.\n";

			std::cout << "\n";
			return true;
		}
		else
		{
			uint16_t err_code;
			getLastError(err_code);
			if (board == -1)
				std::cout << "Test " << func_to_test_str << ": Failed. Error code is : " << err_code << ".\n";
			else
				std::cout << "Test " << func_to_test_str << " on board " << board << ": Failed. Error code is : " << err_code << ".\n";

			std::cout << "\n";
			return false;
		}
	}

int32_t main(int argc, char* argv[])
{
#ifdef POLLING_LIB
	printf("\n\nPOLLING library\n\n");
#else
	printf("\n\nCALLBACK library\n\n");
#endif
	bool overall_result = true;
	bool result = true;
	uint16_t err_code = error_code::DLL_NO_ERROR;	
	
	char ip[] = "164.54.160.186";
	char mask[] = "255.255.255.0";
	char gw[] = "164.54.160.1";

	char identifier[16];

	// Test getLastError() Start.
	overall_result = overall_result && check_func_result("getLastError()", getLastError(err_code));
	// Test getLastError() End.	

	// Test resetLastError() Start.
	overall_result = overall_result && check_func_result("resetLastError()", resetLastError());
	getLastError(err_code);
	if (err_code != error_code::DLL_NO_ERROR)
	{
		std::cout << "WARNING:: checked error code after reset is: " << err_code << ". Test resetLastError(): Fail.\n\n";
		overall_result = overall_result && false;
	}
	// Test resetLastError() End.

#ifndef POLLINGLIB
	// Test register_callback() Start.
	overall_result = overall_result && check_func_result("register_callback()", register_callback(callback_func));
	// Test register_callback() End.
#endif

	// Test initLibrary() Start.
	overall_result = overall_result && check_func_result("InitLibrary()", InitLibrary());
	// Test initLibrary() End.

	// Test libVersion() Start.
	uint32_t version_size = 20;
	char version[20];
	result = check_func_result("libVersion()", libVersion(version, version_size));
	if (result)
		std::cout << "Version is: " << version << "\n\n";
	overall_result = overall_result && result;
	// Test libVersion() End.

	// Test flush_local_eth_conn() Start.
	//overall_result = overall_result && check_func_result("flush_local_eth_conn()", flush_local_eth_conn(ip));
	//std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait for system to correctly close all connections.
	// Test flush_local_eth_conn() End.

	// Test add_to_query() Start.
	overall_result = overall_result && check_func_result("add_to_query()", add_to_query(ip));
	// Test add_to_query() End.

	// Test remove_from_query() Start.
	overall_result = overall_result && check_func_result("remove_from_query()", remove_from_query(ip));
	// Test remove_from_query() End.

	// If the device is connected via Ethernet, add it's IP to the query.
	if (add_to_query(ip))
	{
		std::cout << "Added IP: " << std::string(ip) << " to the query.\n\n";
		
	}
	else
	{
		std::cout << "Error adding the IP to thew query.\n\n";
		overall_result = overall_result && false;;
	}	

	std::this_thread::sleep_for(std::chrono::seconds(4)); // Wait for the boards to be found by the DLL.

	// Test get_dev_number() Start.
	uint16_t dev_nb;
	result = check_func_result("get_dev_number()", get_dev_number(dev_nb));
	if (result)
		std::cout << "Number of recognized boards: " << dev_nb << "\n\n";
	overall_result = overall_result && result;
	// Test get_dev_number() End.

	// Test get_ids() Start.
	uint16_t nb = 0; 
	uint16_t id_size = 16;
	result = check_func_result("get_ids()", get_ids(identifier, nb, id_size));
	if (result)
	{
		std::cout << "Identifier of recognized board: " << identifier << "\n\n";
	}
	overall_result = overall_result && result;
	// Test get_ids() End.
	
	// Test get_boards_in_chain() Start.
	uint16_t chain = 1;
	result = check_func_result("get_boards_in_chain()", get_boards_in_chain(identifier, chain));
	if (result)
		std::cout << "Number of boards in the chain: " << chain << "\n\n";
	overall_result = overall_result && result;
	// Test get_boards_in_chain() End.

	uint32_t call_id;

	// Test getFirmware() Start.
	for (uint16_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("getFirmware()", getFirmware(identifier, i), i);
	}
	// Test getFirmware() End.

	// Test write_IP_configuration() Start.
	overall_result = overall_result && check_func_result("write_IP_configuration()", write_IP_configuration(identifier, ip, mask, gw));
	// Test write_IP_configuration() End.
	
	// Test configure() Start.
	configuration cfg;
	cfg.fast_filter_thr = 100;
	cfg.energy_filter_thr = 5;
	cfg.max_risetime = 5;
	cfg.baseline_samples = 0;
	cfg.edge_peaking_time = 4;
	cfg.max_peaking_time = 64;
	cfg.edge_flat_top = 1;
	cfg.flat_top = 7;
	cfg.gain = 1;
	cfg.peaking_time = 64;
	cfg.reset_recovery_time = 200;
	cfg.inverted_input = false;
	cfg.zero_peak_freq = 1;
	for (uint16_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("configure()", configure(identifier, i, cfg), i);
	}	
	// Test configure() End.

	// Test configure_gating() Start.
	for (uint16_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("configure_gating()", configure_gating(identifier, FreeRunning, i), i);
	}
	// Test configure_gating() End.
	
	// Test configure_offset() Start.
	configuration_offset cfg_offset;
	cfg_offset.offset_val1 = 128;
	cfg_offset.offset_val2 = 128;
	for (uint16_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("configure_offset()", configure_offset(identifier, i, cfg_offset), i);
	}
	// Test configure_offset() End.

	// Test isRunning_system() Start.
	std::cout << "Test isRunning_system().\n";
	bool running = false;
	for (uint16_t i = 0; i < chain; i++)
	//while (1)
	{
		//uint16_t i = 0;
		//isRunning_system(identifier, i);
		overall_result = overall_result && check_func_result("isRunning_system()", isRunning_system(identifier, i), i);
	}
	// Test isRunning_system() End.

	// Test clear_chain() Start.
	overall_result = overall_result && check_func_result("clear_chain()", clear_chain(identifier));
	// Test clear_chain() End. 

	// Test clear_board() Start.
	for (int32_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("clear_board()", clear_board(identifier, i));
	}

	double time;
	uint16_t bins;
	uint32_t spectra_size;

	// Test clear_board() End. 

	// Test Spectrum Acquisition - Timed - 4096 bins. Start.
	std::cout << "Test Spectrum Acquisition - Timed - 4096 bins.\n";
	time = 2; // seconds.
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
		std::cout << "Test Spectrum Acquisition - Timed - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Timed - 1024 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Timed - 1024 bins. End.

	// Test Spectrum Acquisition - Free running - 4096 bins. Start.
	std::cout << "Test Spectrum Acquisition - Free running - 4096 bins.\n";
	time = 0; // Free running.
	bins = 4096;
	result = true; running = true;
	call_id = start("SN01916|SN02712", time, bins);
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
		printf("Spectrum received. Zero peaks: %d\t- %d\n", values4k[95], values4k[93] + values4k[94] + values4k[95] + values4k[96] + values4k[97]);
		if (values4k[95] == 0)
		{
			// There should be some counts here due to the zero peak.
			result = false;
		}
	}

	for (int32_t i = 0; i < 1; i++)
	{
		if (!getData("SN02712", i, values4k, id, stats, spectra_size))
			result = false;
		printf("Spectrum received. Zero peaks: %d\t- %d\n", values4k[95], values4k[93] + values4k[94] + values4k[95] + values4k[96] + values4k[97]);
	}







	
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Free running - 4096 bins: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Free running - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Spectrum Acquisition - Free running - 4096 bins. End.

	// Test Map Acquisition - Timed - 4096 bins. Start.
	std::cout << "Test Map Acquisition - Timed - 4096 bins.\n";
	uint32_t sptime = 50; // milliseconds.
	uint32_t points = 50;
	bins = 4096;
	result = true; running = true;
	uint32_t data_number = 0;
	call_id = start_map("SN01916|SN02712", sptime, points, bins);
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
	call_id = stop("SN01916|SN02712");
  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
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
	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * 4096];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 22];

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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
			advstats_map = new uint64_t[data_number * 22];

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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
			advstats_map = new uint64_t[data_number * 22];

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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
			advstats_map = new uint64_t[data_number * 22];

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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Map Acquisition - Free running - 4096 bins: Failed.\n\n";
		overall_result = false;
	}
	// Test Map Acquisition - Free running - 4096 bins. End.
	
	// Test Waveform Acquisition. Start.
	std::cout << "Test Waveform Acquisition.\n";
	time = 1; // seconds.
	result = true;
	uint16_t dec_ratio = 1;
	unsigned int trig_mask = 1;
	unsigned int trig_level = 0;
	uint16_t length = 1;
	call_id = start_waveform("SN01916|SN02712",0,dec_ratio,trig_mask,trig_level,time,length);
	printf("Call id START WAVE: %d\n", call_id);
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
		printf("SN01916 board %d: %d-%d-%d-%d\n", i, wave[0], wave[1], wave[2], wave[3]);
		if (!(wave[0]>30000 && wave[0]<34000))
		{
			// With grounded input the signal should be at 32000 ADC bins more or less.
			result = false;
		}
	}

	for (int32_t i = 0; i < 1; i++)
	{
		if (!getWaveData("SN02712", i, wave, data_size))
			result = false;
		printf("SN02712 board %d: %d-%d-%d-%d\n", i, wave[0], wave[1], wave[2], wave[3]);
	}


	clear_chain(identifier);
	if (result)
		std::cout << "Test Waveform Acquisition: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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

	id;
	stats;

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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
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
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test List Wave Mode Acquisition - Free running: Failed.\n\n";
		overall_result = false;
	}
	// Test List Mode Acquisition - Timed. End.
	
	
	
	// Test CloseLibrary() Start.
	overall_result = overall_result && check_func_result("CloseLibrary()", CloseLibrary());
	// Test CloseLibrary() End.

	if (overall_result)
		std::cout << "All Tests Completed.\n\n";
	else
		std::cout << "At least one test failed.\n\n";
	
	return 0;
}