#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdlib.h>

#define PACKAGE_SIZE 16 // obviamente será maior, isso era pra testar só
#define ESP_HOST_PORT 50000
#define ESP_IP_ADDRESS "127.0.0.1" // alterar depois
#define PLANT_GOOD 1
#define PLANT_BADD 0
#define PLANT_NONE -1

enum package_type {
	PKG_BOTH_EMPTY,					// [PC, ESP]	no new data
	PKG_ESP_DETECTED_SOMETHING,		// [ESP]		ESP32 detected a new thing to take a screenshot of
	PKG_ESP_JPG_SEND,				// [ESP]		ESP32 is sending the photo it took
	PKG_PC_OPTION_FINAL,			// [PC]			PC tells ESP32 what to do with the detected thing (is it good or bad?)
	PKG_PC_REQUEST_WEIGHT,			// [PC]			Request read of current balance
	PKG_ESP_WEIGHT_INFO				// [ESP]		Return weight data
};

union pc_data_possibilities {
	char raw_data[PACKAGE_SIZE];
	// PC_OPTION_FINAL:
	struct {
		char good; // right now: 0 = bad, 1 = good
	} option_final;
	// PC_REQUEST_WEIGHT:
	// <just a request, no data>
};

union esp_data_possibilites {
	char raw_data[PACKAGE_SIZE];
	// ESP_DETECTED_SOMETHING:
	// <just a request, no data>
	// ESP_JPG_SEND:
	// <full raw data>
	// ESP_WEIGHT_INFO
	struct {
		float good_side_kg;
		float bad_side_kg;
		// add more if more options in the future
	} weight_info;
};

struct esp_package {
	union {
		char raw[PACKAGE_SIZE];
		union pc_data_possibilities pc;
		union esp_data_possibilites esp;
	} data;
	unsigned long len;
	int type;
	int left;
};

// struct to store the data, the data being stored, size of the data (if bigger than one package it will return the possible size stored and will correctly set "left"), what type? (enum-like)
int build_package_raw(struct esp_package*, const char*, unsigned long, const int);

// - - - - - - BOTH - - - - - - //

// struct to store the data
int build_both_empty(struct esp_package*);

// - - - - - - PC TO ESP32 FUNCTIONS - - - - - - //

// struct to store the data, good (1) or bad (0) ?
int build_pc_good_or_bad(struct esp_package*, const char);
// struct to store the data
int build_pc_request_weight(struct esp_package*);

// - - - - - - ESP32 TO PC FUNCTIONS - - - - - - //

// struct to store the data
int build_esp_has_something(struct esp_package*);
// struct to store the data, a copy of esp32 data that will be modified by this function as it goes, a pointer to a size_t with the maximum size (this will be decreased until 0). Function returns > 0 while size_t* is > 0 (while it has something to send)
int build_esp_each_jpg_data(struct esp_package*, char**, unsigned long*);
// struct to store the data, kilograms of bad, kilograms of good
int build_esp_weight_info(struct esp_package*, const float, const float);

#ifdef __cplusplus
}
#endif