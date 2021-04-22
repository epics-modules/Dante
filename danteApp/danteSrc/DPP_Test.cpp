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
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>

#ifdef WIN32
#include <direct.h>
char* GetCurrentWorkingDir(void) {
	char buff[FILENAME_MAX];
	_getcwd(buff, FILENAME_MAX);
	return buff;
}
#endif

uint32_t answer_data[10];
bool overall_result = true;

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

void Test_SingleSpectrum_Timed(char * identifier, uint16_t chain, uint32_t spectra_size)
{
	std::cout << "Test Spectrum Acquisition - Timed - " << spectra_size << " bins.\n";
	uint16_t err_code;
	double time = 2;
	uint16_t bins = (uint16_t)spectra_size;
	uint32_t result = true;
	bool running = true;
	bool LastDataReceived = false;
	uint32_t call_id = start(identifier, time, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{
							running = true; break;
						}
					}
					else
					{
						result = false; break;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	std::cout << "Launching stop acquisition...\n";
	call_id = stop(identifier);
	if (call_id > 0)
	{
		if (!wait_answer(call_id))
			result = false;
		else
			std::cout << "Acquisition Stopped.\n";
	}
	else
		result = false;
	uint64_t values4k[4096];
	uint64_t values2k[2048];
	uint64_t values1k[1024];
	uint32_t id;
	statistics stats;

	for (int32_t i = 0; i < chain; i++)
	{
		if (spectra_size == 4096)
		{
			if (!getData(identifier, i, values4k, id, stats, spectra_size))
				result = false;
		}
		else if (spectra_size == 2048)
		{
			if (!getData(identifier, i, values2k, id, stats, spectra_size))
				result = false;
		}
		else
		{
			if (!getData(identifier, i, values1k, id, stats, spectra_size))
				result = false;
		}
		if ((spectra_size == 4096) && (values4k[95] == 0) || (spectra_size == 2048) && (values4k[47] == 0) || (spectra_size == 1024) && (values4k[23] == 0))
		{
			// There should be some counts here due to the zero peak.
			std::cout << "Problem board " << i << ": Zero peak not detected.\n";
			//result = false;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Spectrum Acquisition - Timed - " << spectra_size << " bins: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Spectrum Acquisition - Timed - " << spectra_size << " bins: Failed.\n\n";
		overall_result = false;
	}
}

void Test_Map_Timed(char * identifier, uint16_t chain, uint32_t spectra_size, uint32_t sptime, uint32_t points)
{
	std::cout << "Test Map Acquisition - Timed - " << spectra_size << " bins.\n";
	uint16_t err_code;
	uint16_t bins = (uint16_t)spectra_size;
	uint32_t result = true;
	bool running = true;
	bool LastDataReceived = false;
	uint32_t data_number = 0;
	uint32_t call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{
							running = true; break;
						}
						std::cout << "Board " << i << " - LastDataReceived! \n";
					}
					else
					{
						result = false; break;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;

	//std::cout << "Launching stop acquisition...\n";
	//call_id = stop(identifier);
	////std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	//if (call_id > 0)
	//{
	//	if (!wait_answer(call_id))
	//		result = false;
	//	else
	//		std::cout << "Acquisition Stopped.\n";
	//}
	//else
	//	result = false;

	uint16_t* values_map;
	uint32_t* id_map;
	double* stats_map;
	uint64_t* advstats_map;
	uint16_t print_bin_index = spectra_size/42-1;

	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			
			values_map = new uint16_t[data_number * spectra_size];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 22];
			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;

			std::cout << "Board " << i << " - getAvailableData returned: " << data_number << " - Spectra: ";
			std::cout << "[" << print_bin_index << "][" << 1 << "] = "<< values_map[print_bin_index] << " - ";
			std::cout << "[" << print_bin_index << "][" << 2 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size] << " - ";
			std::cout << "[" << print_bin_index << "][" << 3 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *2] << " - ";
			std::cout << "[" << print_bin_index << "][" << 4 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size * 3] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number - 3 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 4)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number-2 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 3)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number-1 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 2)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number -1)] << "\n";
			
			//{
			//	// There should be some counts here due to the zero peak.
			//	std::cout << "Problem board " << i << ": Zero peak not detected.\n";
			//	//result = false;
			//}
			//system("PAUSE");
			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Timed - " << spectra_size << " bins: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Map Acquisition - Timed - " << spectra_size << " bins: Failed.\n\n";
		overall_result = false;
	}
}

void Test_Map_FreeRunning(char * identifier, uint16_t chain, uint32_t spectra_size, uint32_t sptime, uint32_t points)
{
	std::cout << "Test Map Acquisition - Free Running - " << spectra_size << " bins.\n";
	uint16_t err_code;
	uint16_t bins = (uint16_t)spectra_size;
	uint32_t result = true;
	bool running = true;
	bool LastDataReceived = false;
	bool stopped = false;
	uint32_t data_number = 0;
	uint32_t call_id = start_map(identifier, sptime, points, bins);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			do
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{ running = true; break; }
					}
					else
					{ result = false; break; }
				}
	
				std::this_thread::sleep_for(std::chrono::milliseconds(5000));
				if (!stopped)
					std::cout << "Launching stop acquisition...\n";
				call_id = stop(identifier);
				stopped = true;
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			} while (running && result);
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;

	uint16_t* values_map;
	uint32_t* id_map;
	double* stats_map;
	uint64_t* advstats_map;
	uint16_t print_bin_index = spectra_size / 42 - 1;

	for (int32_t i = 0; i < chain; i++)
	{
		if (!getAvailableData(identifier, i, data_number))
			result = false;
		else
		{
			values_map = new uint16_t[data_number * spectra_size];
			id_map = new uint32_t[data_number];
			stats_map = new double[data_number * 4];
			advstats_map = new uint64_t[data_number * 22];
	
			if (!getAllData(identifier, i, values_map, id_map, stats_map, advstats_map, spectra_size, data_number))
				result = false;
			
			std::cout << "Board " << i << " - getAvailableData returned: " << data_number << " - Spectra: ";
			std::cout << "[" << print_bin_index << "][" << 1 << "] = " << values_map[print_bin_index] << " - ";
			std::cout << "[" << print_bin_index << "][" << 2 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size] << " - ";
			std::cout << "[" << print_bin_index << "][" << 3 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size * 2] << " - ";
			std::cout << "[" << print_bin_index << "][" << 4 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size * 3] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number - 3 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 4)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number - 2 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 3)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number - 1 << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 2)] << " - ";
			std::cout << "[" << print_bin_index << "][" << data_number << "] = " << values_map[print_bin_index + (uint16_t)spectra_size *((uint16_t)data_number - 1)] << "\n";

			delete[] values_map;
			delete[] id_map;
			delete[] stats_map;
			delete[] advstats_map;
		}
	}
	clear_chain(identifier);
	if (result)
		std::cout << "Test Map Acquisition - Free Running - " << spectra_size << " bins: Ok.\n\n";
	else
	{
		getLastError(err_code);
		std::cout << "Error code is: " << err_code << ".\n";
		std::cout << "Test Map Acquisition - Free Running - " << spectra_size << " bins: Failed.\n\n";
		overall_result = false;
	}
}

void Test_Wave_Timed(char * identifier, uint16_t chain)
{
	
	std::cout << "Test Waveform Acquisition.\n";
	uint16_t err_code;
	double time = 1; // seconds.
	bool result = true;
	bool running = true;
	uint16_t dec_ratio = 1;
	unsigned int trig_mask = 1;
	unsigned int trig_level = 0;
	bool LastDataReceived = false;
	uint16_t length = 1;
	uint32_t call_id = start_waveform(identifier, 0, dec_ratio, trig_mask, trig_level, time, length);
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
		printf("%s board %d: %d-%d-%d-%d\n", identifier, i, wave[0], wave[1], wave[2], wave[3], wave[14383]);
		//if (!(wave[0] > 30000 && wave[0] < 34000))
		//{
		//	// With grounded input the signal should be at 32000 ADC bins more or less.
		//	std::cout << "Problem board " << i << ": Waveform acquisition out of range [30000 - 34000].\n";
		//	//result = false;
		//}
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
}

void Test_List_Timed(char * identifier, uint16_t chain)
{
	std::cout << "Test List Mode Acquisition - Timed.\n";
	uint16_t err_code;
	double time = 2; // seconds.
	bool result = true;
	bool running = true;
	bool LastDataReceived = false;
	uint32_t call_id = start_list(identifier, time);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{ running = true; break; }
					}
					else
					{ result = false; break; }
				}
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
	uint32_t data_number = 0;

	uint32_t id;
	statistics stats;

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
				std::cout << "Problem board " << i << ": List acquisition out of range of zero peak.\n";
				//result = false;
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
}

void Test_List_FreeRunning(char * identifier, uint16_t chain)
{
	std::cout << "Test List Mode Acquisition - Free running.\n";
	double time = 0; // Free running.
	uint32_t result = true;
	bool running = true;
	bool stopped = false;
	bool LastDataReceived = false;
	uint32_t call_id = start_list(identifier, time);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			do
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{ running = true; break; }
					}
					else
					{ result = false; break; }
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				if (!stopped)
					std::cout << "Launching stop acquisition...\n";
				call_id = stop(identifier);
				stopped = true;
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			} while (running && result);
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
			
	}
	else
		result = false;
	uint32_t data_number = 0;
	uint64_t *list_data;
	uint32_t id;
	statistics stats;
	uint16_t err_code;
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
				std::cout << "Problem board " << i << ": List acquisition out of range of zero peak.\n";
				//result = false;
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
}

void Test_ListWave_Timed(char * identifier, uint16_t chain)
{
	std::cout << "Test List Wave Mode Acquisition - Timed.\n";
	double time = 2; // seconds.
	uint16_t dec_ratio = 1;
	uint16_t list_wave_length = 400;
	uint16_t list_wave_offset = 0;
	bool result = true;
	bool running = true;
	bool LastDataReceived = false;
	uint32_t call_id = start_listwave(identifier, time, dec_ratio, list_wave_length, list_wave_offset);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			while (running && result)
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{
							running = true; break;
						}
					}
					else
					{
						result = false; break;
					}
				}
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
	uint32_t data_number = 0;
	uint16_t length = 1;
	uint32_t id;
	statistics stats;
	uint16_t err_code;
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
				std::cout << "Problem board " << i << ": ListWave acquisition out of range of zero peak.\n";
				//result = false;
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
}

void Test_ListWave_FreeRunning(char * identifier, uint16_t chain)
{
	std::cout << "Test List Wave Mode Acquisition - Free running.\n";
	double time = 0; // Free running.
	uint16_t dec_ratio = 1;
	uint16_t list_wave_length = 400;
	uint16_t list_wave_offset = 0;
	bool result = true;
	bool running = true;
	bool stopped = false;
	bool LastDataReceived = false;
	uint32_t call_id = start_listwave(identifier, time, dec_ratio, list_wave_length, list_wave_offset);
	if (call_id > 0)
	{
		if (wait_answer(call_id))
		{
			std::cout << "Acquisition started.\n";
			do
			{
				std::cout << "Acquisition in progress.\n";
				for (int32_t i = 0; i < chain; i++)
				{
					running = false;
					call_id = isLastDataReceived(identifier, i, LastDataReceived);
					if (call_id)
					{
						if (!LastDataReceived)
						{
							running = true; break;
						}
					}
					else
					{
						result = false; break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				if (!stopped)
					std::cout << "Launching stop acquisition...\n";
				call_id = stop(identifier);
				stopped = true;
				if (call_id > 0)
				{
					if (!wait_answer(call_id))
						result = false;
				}
				else
					result = false;
			} while (running && result);
			std::cout << "Acquisition finished.\n";
		}
		else
			result = false;
	}
	else
		result = false;
	uint32_t data_number = 0;
	uint64_t *listw_values;
	uint16_t length = 1;
	uint64_t* listw_wave_values = 0;
	uint32_t id;
	statistics stats;
	uint16_t err_code;
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
				std::cout << "Problem board " << i << ": ListWave acquisition out of range of zero peak.\n";
				//result = false;
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
}

int32_t main(int argc, char* argv[])
{
#ifdef POLLING_LIB
	printf("\n\nPOLLING library\n\n");
#else
	printf("\n\nCALLBACK library\n\n");
#endif
	bool result = true;
	bool stopped = false;
	uint16_t err_code = error_code::DLL_NO_ERROR;

	char ip[] = "10.96.0.111d";
	std::cout << "Insert IP Address of DANTE under test: ";
	std::cin >> ip;
	std::cout << "Inserted IP: " << ip << "\n\n";
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	char mask[] = "255.255.255.0";
	char gw[] = "10.96.0.1";

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
	//system("PAUSE");
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

	//// Test get_boards_in_chain() Start.
	uint16_t chain = 1;
	bool all_boards = 0;
	do
	{
		result = check_func_result("get_boards_in_chain()", get_boards_in_chain(identifier, chain));
		if (result)
			std::cout << "Number of boards in the chain: " << chain << "\n\n";
		overall_result = overall_result && result;
		if (chain == 8)
			all_boards = 1;
		std::this_thread::sleep_for(std::chrono::seconds(4)); // Wait for the boards to be found by the DLL.
	} while (all_boards == 0);
	// Test get_boards_in_chain() End.

	//system("PAUSE");
	////result = check_func_result("reset_daisy_chain()", reset_daisy_chain(identifier, 0));
	//result = reset_daisy_chain(identifier, 0);
	//if (result)
	//	std::cout << "Reset daisy chain\n\n";
	//overall_result = overall_result && result;
	//std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait for the boards to be found by the DLL.
	//system("PAUSE");

	uint32_t call_id;

	// Test getFirmware() Start.
	for (uint16_t i = 0; i < chain; i++)
	{
		overall_result = overall_result && check_func_result("getFirmware()", getFirmware(identifier, i), i);
	}
	// Test getFirmware() End.

	// Test write_IP_configuration() Start.
	//overall_result = overall_result && check_func_result("write_IP_configuration()", write_IP_configuration(identifier, ip, mask, gw));
	// Test write_IP_configuration() End.

	// Test configure() Start.
	autoScanSlaves(false);	// it is necessary to disable the autoScanSlaves() when configuring, in order to prevent interlock problems. Keep disabled also for acquisitions.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));	// wait 50ms

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
	//for (uint16_t i = 0; i < chain; i++)
	//{
	//	overall_result = overall_result && check_func_result("configure()", configure(identifier, i, cfg), i);
	//}
	overall_result = overall_result && check_func_result("configure()", configure(identifier, 255, cfg), 0);
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
		overall_result = overall_result && check_func_result("isRunning_system()", isRunning_system(identifier, i), i);
	// Test isRunning_system() End.

	// Test isLastDataReceived() Start.
	std::cout << "Test isLastDataReceived().\n";
	bool LastDataReceived = false;
	for (uint16_t i = 0; i < chain; i++)
		overall_result = overall_result && check_func_result("isLastDataReceived()", isLastDataReceived(identifier, i, LastDataReceived), i);
	// Test isLastDataReceived() End.

	// Test clear_chain() Start.
	overall_result = overall_result && check_func_result("clear_chain()", clear_chain(identifier));
	// Test clear_chain() End. 

	// Test clear_board() Start.
	for (uint16_t i = 0; i < chain; i++)
		overall_result = overall_result && check_func_result("clear_board()", clear_board(identifier, i));

	// Test clear_board() End. 

	// START ACQUISITION TESTS --------------------------------------------------------------------------------------------

	// Test Spectrum Acquisition - Timed - 4096 bins.
	//Test_SingleSpectrum_Timed(identifier, chain, 4096);

	// Test Spectrum Acquisition - Timed - 2048 bins.
	//Test_SingleSpectrum_Timed(identifier, chain, 2048);

	// Test Spectrum Acquisition - Timed - 1024 bins.
	//Test_SingleSpectrum_Timed(identifier, chain, 1024);
	
	// Test Map Acquisition - Timed - [1024 or 2048 or 4096] bins.
	uint32_t map_bins;
	uint32_t sptime;
	uint32_t points;
	std::cout << "Insert Map bins: ";
	std::cin >> map_bins;
	std::cout << "Insert Map spectra time: ";
	std::cin >> sptime;
	std::cout << "Insert Map points: ";
	std::cin >> points;
	Test_Map_Timed(identifier, chain, map_bins, sptime, points);

	// Test Map Acquisition - Free running - [1024 or 2048 or 4096] bins.
	Test_Map_FreeRunning(identifier, chain, map_bins, sptime, 0);

	// Test Waveform Acquisition - Timed.
	//Test_Wave_Timed(identifier, chain);

	//// Test List Mode Acquisition - Timed.
	//Test_List_Timed(identifier, chain);

	//// Test List Mode Acquisition - Free running.
	//Test_List_FreeRunning(identifier, chain);

	//// Test List Wave Mode Acquisition - Timed.
	//Test_ListWave_Timed(identifier, chain);

	//// Test List Wave Mode Acquisition - Free running.
	//Test_ListWave_FreeRunning(identifier, chain);

	// END ACQUISITION TESTS -----------------------------------------------------------------------------------------------

	// Test CloseLibrary() Start.
	overall_result = overall_result && check_func_result("CloseLibrary()", CloseLibrary());
	// Test CloseLibrary() End.

	if (overall_result)
		std::cout << "All Tests Completed.\n\n";
	else
		std::cout << "At least one test failed.\n\n";
	return 0;
}