/*
 * FreeRTOS V202107.00
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/**
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 * 
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_spi_flash.h"

#include "FreeRTOS_CLI.h"

#include "tinyusb.h"
#include "cmd_wifi.h"

#define cliNEW_LINE    "\r\n"

char* null_password = "";

#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
static BaseType_t prvTaskStatusCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif

#if CFG_TUD_NET
static BaseType_t prvStationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvScanCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvAPCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvSetWiFiModeCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvSmartConfigCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
#endif /* CFG_TUD_NET */

static BaseType_t prvRamCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvRestartCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

static BaseType_t prvGetVersionCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
/* Structure that defines the "task-stats" command line command. */
static const CLI_Command_Definition_t xTaskStatus =
{
	"task-status", /* The command string to type. */
	"task-status: Displays the state of each task\r\n",
	prvTaskStatusCommand, /* The function to run. */
	0 /* No parameters are expected. */
};
#endif

#if CFG_TUD_NET
/* Structure that defines the "sta" command line command. */
static const CLI_Command_Definition_t xStationCommand =
{
	"sta", /* The command string to type. */
	"sta -s <ssid> [-p <password>]: join specified soft-AP\r\nsta -d: disconnect specified soft-AP\r\n",
	prvStationCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

/* Structure that defines the "scan" command line command. */
static const CLI_Command_Definition_t xScanCommand =
{
	"scan", /* The command string to type. */
	"scan [<ssid>]: <ssid>  SSID of AP want to be scanned\r\n",
	prvScanCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

/* Structure that defines the "ap" command line command. */
static const CLI_Command_Definition_t xAPCommand =
{
	"ap", /* The command string to type. */
	"ap <ssid> [<password>]: configure ssid and password\r\n",
	prvAPCommand, /* The function to run. */
	-1 /* The user can enter any number of commands. */
};

/* Structure that defines the "mode" command line command. */
static const CLI_Command_Definition_t xSetWiFiModeCommand =
{
	"mode", /* The command string to type. */
	"mode <mode>: <sta> station mode; <ap> ap mode\r\n",
	prvSetWiFiModeCommand, /* The function to run. */
	1 /* one parameters are expected. */
};

/* Structure that defines the "smartconfig" command line command. */
static const CLI_Command_Definition_t xSmartConfigCommand =
{
	"smartconfig", /* The command string to type. */
	"smartconfig [op]: op:1, start smartconfig; op:0, stop smartconfig\r\n",
	prvSmartConfigCommand, /* The function to run. */
	1 /* No parameters are expected. */
};
#endif /* CFG_TUD_NET */

/* Structure that defines the "ram" command line command. */
static const CLI_Command_Definition_t xRamCommand =
{
	"ram", /* The command string to type. */
	"ram: Get the current size of free heap memory and minimum size of free heap memory\r\n",
	prvRamCommand, /* The function to run. */
	0 /* No parameters are expected. */
};

/* Structure that defines the "restart" command line command. */
static const CLI_Command_Definition_t xRestartCommand =
{
	"restart", /* The command string to type. */
	"restart: Software reset of the chip\r\n",
	prvRestartCommand, /* The function to run. */
	0 /* No parameters are expected. */
};

/* Structure that defines the "version" command line command. */
static const CLI_Command_Definition_t xGetVersionCommand =
{
	"version", /* The command string to type. */
	"version: Get version of chip and SDK\r\n",
	prvGetVersionCommand, /* The function to run. */
	0 /* No parameters are expected. */
};

/*-----------------------------------------------------------*/

void vRegisterCLICommands( void )
{
    FreeRTOS_CLICreatMux();

	/* Register all the command line commands defined immediately above. */
#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
	FreeRTOS_CLIRegisterCommand( &xTaskStatus );
#endif
#if CFG_TUD_NET
	FreeRTOS_CLIRegisterCommand( &xAPCommand );
	FreeRTOS_CLIRegisterCommand( &xStationCommand );
	FreeRTOS_CLIRegisterCommand( &xSetWiFiModeCommand );
	FreeRTOS_CLIRegisterCommand( &xSmartConfigCommand );
	FreeRTOS_CLIRegisterCommand( &xScanCommand );
#endif /* CFG_TUD_NET */
	FreeRTOS_CLIRegisterCommand( &xRamCommand );
	FreeRTOS_CLIRegisterCommand( &xRestartCommand );
	FreeRTOS_CLIRegisterCommand( &xGetVersionCommand );
}
/*-----------------------------------------------------------*/

#if CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS
static BaseType_t prvTaskStatusCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *const pcHeader = "Task          State  Priority  Stack	#\r\n************************************************\r\n";

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	char *data = (char *)malloc(512);
	/* Generate a table of task stats. */
	strcpy( data, pcHeader );
	vTaskList( data + strlen( pcHeader ) );
	printf("%s", data);
	free(data);

	/* There is no more data to return after this single string, so return pdFALSE. */
	return pdFALSE;
}
#endif
/*-----------------------------------------------------------*/

#if CFG_TUD_NET
static BaseType_t prvStationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	char *pc1, *pc2, *pc3, *pc4, *pc5, *pc6;
	BaseType_t xLength1, xLength2, xLength3, xLength4, xLength5, xLength6;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	/* Obtain the sixth parameter */
	pc6 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									6,                  /* Return the Sixth parameter. */
									&xLength6           /* Store the parameter string length. */
								);

	/* Obtain the fifth parameter */
	pc5 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									5,                  /* Return the fifth parameter. */
									&xLength5           /* Store the parameter string length. */
								);

	/* Obtain the fourth parameter */
	pc4 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									4,                  /* Return the fourth parameter. */
									&xLength4           /* Store the parameter string length. */
								);

	/* Obtain the third parameter. */
	pc3 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									3,                  /* Return the third parameter. */
									&xLength3           /* Store the parameter string length. */
								);

	/* Obtain the second parameter */
	pc2 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									2,                  /* Return the second parameter. */
									&xLength2           /* Store the parameter string length. */
								);

	/* Obtain the first parameter. */
	pc1 = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									1,                  /* Return the first parameter. */
									&xLength1           /* Store the parameter string length. */
								);

	if (pc1 == NULL) {
		wifi_cmd_query();
		return pdFALSE;
	} else {
		/* Sanity check something was returned. */
		configASSERT( pc1 );
		/* Terminate the string. */
		pc1[ xLength1 ] = 0x00;

		if(strncmp(pc1, "-d", strlen("-d")) == 0) {
			if (wif_cmd_disconnect_wifi() == ESP_OK) {
				sprintf(pcWriteBuffer, "OK\r\n");
			} else {
				sprintf(pcWriteBuffer, "FAIL\r\n");
			}
			return pdFALSE;
		} else if(strncmp(pc1, "-s", strlen("-s")) != 0) {
			sprintf(pcWriteBuffer, "Invalid parameter\r\n");
			return pdFALSE;
		}
	}

	if (pc2 == NULL) {
		sprintf(pcWriteBuffer, "Invalid parameter\r\n");
		return pdFALSE;
	} else {
		if (pc3 == NULL) {
			wifi_cmd_sta_join(pc2, null_password);
			printf("the ssid is %s.\r\n", pc2);
			printf("the ssid len is %d.\r\n", xLength2);
			return pdFALSE;
		}
	}

	if (pc4 == NULL) {
		wifi_cmd_sta_join(pc2, null_password);
		printf("the ssid is %s.\r\n", pc2);
		printf("the ssid len is %d.\r\n", (xLength2 + xLength3 + 1));
		return pdFALSE;
	}

	if (pc5 == NULL) {
		/* Terminate the string. */
		pc2[ xLength2 ] = 0x00;

		if(strncmp(pc3, "-p", strlen("-p")) != 0) {
			sprintf(pcWriteBuffer, "Invalid parameter\r\n");
			return pdFALSE;
		}
		/* Terminate the string. */
		pc3[ xLength3 ] = 0x00;
		wifi_cmd_sta_join(pc2, pc4);
		printf("the ssid is %s, the password is %s.\r\n", pc2, pc4);
		printf("the ssid len is %d, the password len is %d.\r\n", xLength2, xLength4);
		return pdFALSE;
	}

	if (pc6 == NULL) {
		if(strncmp(pc3, "-p", strlen("-p")) == 0) {    
			if(strncmp(pc4, "-p", strlen("-p")) != 0) {
				/* Terminate the string. */
				pc2[ xLength2 ] = 0x00;
				pc3[ xLength3 ] = 0x00;
				wifi_cmd_sta_join(pc2, pc4);
				printf("the ssid is %s, the password is %s.\r\n", pc2, pc4);
				printf("the ssid len is %d, the password len is %d.\r\n", xLength2, (xLength4 + xLength5 + 1));
				return pdFALSE;
			}
		}
		/* Terminate the string. */
		pc3[ xLength3 ] = 0x00;
		pc4[ xLength4 ] = 0x00;
		wifi_cmd_sta_join(pc2, pc5);
		printf("the ssid is %s, the password is %s.\r\n", pc2, pc5);
		printf("the ssid len is %d, the password len is %d.\r\n", (xLength2 + xLength3 + 1), xLength5);
	} else {
		/* Terminate the string. */
		pc3[ xLength3 ] = 0x00;
		pc4[ xLength4 ] = 0x00;
		wifi_cmd_sta_join(pc2, pc5);
		printf("the ssid is %s, the password is %s.\r\n", pc2, pc5);
		printf("the ssid len is %d, the password len is %d.\r\n", (xLength2 + xLength3 + 1), (xLength5 + xLength6 + 1));
	}

	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvScanCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcSSID;
	BaseType_t SSIDLength;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	/* Obtain the SSID of AP . */
	pcSSID = FreeRTOS_CLIGetParameter
					(
						pcCommandString,    /* The command string itself. */
						1,                  /* Return the first parameter. */
						&SSIDLength         /* Store the parameter string length. */
					);

	if (pcSSID == NULL) {
		/* TODO */ 
		wifi_cmd_sta_scan(NULL);
	} else {
		/* TODO */
		wifi_cmd_sta_scan(pcSSID);
	}

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvAPCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	char *pcSSID, *pcPassWord;
	BaseType_t xSSIDLength, xPassWordLength;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	/* Obtain the ssid of AP. */
	pcPassWord = ( char * ) FreeRTOS_CLIGetParameter
									(
										pcCommandString,    /* The command string itself. */
										2,                  /* Return the second parameter. */
										&xPassWordLength    /* Store the parameter string length. */
									);

	/* Obtain the password of AP */
	pcSSID = ( char * ) FreeRTOS_CLIGetParameter
								(
									pcCommandString,    /* The command string itself. */
									1,                  /* Return the first parameter. */
									&xSSIDLength        /* Store the parameter string length. */
								);

	if (pcSSID == NULL) {
		wifi_cmd_query();
		return pdFALSE;
	}
	
	/* Sanity check something was returned. */
	configASSERT( pcSSID );

	/* Terminate the string. */
	pcSSID[ xSSIDLength ] = 0x00;

	if (pcPassWord == NULL) {
		/* TODO */ 
		wifi_cmd_ap_set(pcSSID, null_password);
		printf("the ssid is %s.\r\n", pcSSID);
		printf("the ssid len is %d.\r\n", xSSIDLength);
	} else {
		/* TODO */
		wifi_cmd_ap_set(pcSSID, pcPassWord);
		printf("the ssid is %s, the password is %s.\r\n", pcSSID, pcPassWord);
		printf("the ssid len is %d, the password len is %d.\r\n", xSSIDLength, xPassWordLength);
	}

	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvSetWiFiModeCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcWiFiMode;
	BaseType_t WiFiModeLength;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	/* Obtain the parameter string. */
	pcWiFiMode = FreeRTOS_CLIGetParameter
					(
						pcCommandString,    /* The command string itself. */
						1,                  /* Return the first parameter. */
						&WiFiModeLength     /* Store the parameter string length. */
					);

	/* Sanity check something was returned. */
	configASSERT( pcWiFiMode );

	if (wifi_cmd_set_mode(pcWiFiMode) == ESP_FAIL) {
		sprintf(pcWriteBuffer, "Invalid parameter\r\n");
	}

	/* There is no more data to return after this single string, so return
	pdFALSE. */
	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvSmartConfigCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	const char *pcSmartConfig;
	BaseType_t Length;

	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	/* Obtain the parameter string. */
	pcSmartConfig = FreeRTOS_CLIGetParameter
					(
						pcCommandString,    /* The command string itself. */
						1,                  /* Return the first parameter. */
						&Length             /* Store the parameter string length. */
					);

	/* Sanity check something was returned. */
	configASSERT( pcSmartConfig );

	/* There are only two valid parameter values. */
	if(strncmp(pcSmartConfig, "0", strlen("0")) == 0) {
		if (wifi_cmd_stop_smart_config() == ESP_OK) {
			sprintf(pcWriteBuffer, "OK\r\n");
		} else {
			sprintf(pcWriteBuffer, "FAIL\r\n");
		}
		// sprintf(pcWriteBuffer, "Stop SmartConfig\r\n");
	} else if (strncmp(pcSmartConfig, "1", strlen("1")) == 0) {
		if (wifi_cmd_start_smart_config() == ESP_FAIL) {
			sprintf(pcWriteBuffer, "SmartConfig Task has been created, Don't create repeatedly\r\n");
		}
	} else {
		sprintf(pcWriteBuffer, "Valid parameters are '0' and '1'\r\n");
	}

	return pdFALSE;
}
/*-----------------------------------------------------------*/
#endif /* CFG_TUD_NET */

static BaseType_t prvRamCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

	uint32_t heap_size = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
	uint32_t size = esp_get_free_heap_size();

	sprintf(pcWriteBuffer, "free heap size: %d, min heap size: %u\r\n", size, heap_size);

	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvRestartCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );
	
    esp_restart();

	return pdFALSE;
}
/*-----------------------------------------------------------*/

static BaseType_t prvGetVersionCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString )
{
	/* Remove compile time warnings about unused parameters, and check the
	write buffer is not NULL.  NOTE - for simplicity, this example assumes the
	write buffer length is adequate, so does not check for buffer overflows. */
	( void ) pcCommandString;
	( void ) xWriteBufferLen;
	configASSERT( pcWriteBuffer );
	memset( pcWriteBuffer, 0x00, xWriteBufferLen );

    esp_chip_info_t info;
    esp_chip_info(&info);
	sprintf(pcWriteBuffer, "IDF Version:%s\r\nChip info:\r\n\tcores:%d\r\n\tfeature:%s%s%s%s%d%s\r\n\trevision number:%d\r\n", 
							esp_get_idf_version(),
							info.cores,
							info.features & CHIP_FEATURE_WIFI_BGN ? "/802.11bgn" : "",
							info.features & CHIP_FEATURE_BLE ? "/BLE" : "",
							info.features & CHIP_FEATURE_BT ? "/BT" : "",
							info.features & CHIP_FEATURE_EMB_FLASH ? "/Embedded-Flash:" : "/External-Flash:",
							spi_flash_get_chip_size() / (1024 * 1024), " MB",
							info.revision);

	return pdFALSE;
}
/*-----------------------------------------------------------*/
