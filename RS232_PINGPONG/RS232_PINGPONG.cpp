// RS232_PINGPONG.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <Windows.h>
#include <process.h>
#include "Read.h"

#define STATUS_CHECK_TIMEOUT 500	// ms 

using namespace std;

struct configStruct
{
	HANDLE portHandle;
};

struct configStruct Config();
void Read(void * args);
void Write(void * args);

int main()
{
	//initialize port configuration
	printf("MAIN: Configuring port... ");
	struct configStruct conf = Config();
	if (conf.portHandle != INVALID_HANDLE_VALUE)
		printf("Done.\n");
	else
	{
		printf("Error when creating HANDLE.\n");
		return 1;
	}

	printf("MAIN: Starting READ thread...");
	//start threads for tx and rx
	_beginthread(Read, 0, &conf.portHandle);
	printf("Done.\n");
	printf("MAIN: Starting READ thread...");
	_beginthread(Write, 0, &conf);
	printf("Done.\n");
	//wait until kill signal comes
	for (;;);
}


void Write(void * args)
{
	printf("WRITE: Started.\n");
	struct configStruct * conf = (struct configStruct *) args;

	// Amount of DWORDS in data[]
	const DWORD dwToWrite = 4;
	// Actual data to be transmitted
	DWORD data[dwToWrite] = { 0, 0xff, 0, 0 };
	// OVERLAPPED structure that subscribes to the reading events
	OVERLAPPED osWrite = { 0 };
	// WORDS written
	DWORD dwWritten;
	// Operation Result
	BOOL fRes;

	// Create this writes OVERLAPPED structure hEvent.
	for (;;)
	{
		printf("WRITE: Assigning event handle... ");
		osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (osWrite.hEvent == NULL)
		{
			printf("Error.\n");
			return;
		}
		printf("Done.\n");
		Sleep(STATUS_CHECK_TIMEOUT);
		// Issue write.
		printf("WRITE: Sending data... ");
		if (!WriteFile(conf->portHandle, data, dwToWrite, &dwWritten, &osWrite)) {
			printf("... ");
			if (GetLastError() != ERROR_IO_PENDING) {
				// WriteFile failed, but it isn't delayed. Report error and abort.
				printf("WriteFile failed, but it wasnt't delayed. Abort.\n");
				fRes = FALSE;
				return;
			}
			else {
				// Write is pending.
				printf("Write operation is pending... ");
				if (!GetOverlappedResult(conf->portHandle, &osWrite, &dwWritten, TRUE))
				{
					printf("And apparently was lost in the serial writer buffer.\n ");
					fRes = FALSE;
				}
				else
				{
					printf("And was done successfully.\n");
					// Write operation completed successfully.
					fRes = TRUE;
				}
			}
		}
		else
		{
			// WriteFile completed immediately.
			printf("Success.\n");
			fRes = TRUE;
		}
	}

	CloseHandle(osWrite.hEvent);
}

struct configStruct Config()
{
	//structure that contains all the configuration
	struct configStruct result = { 0 };

	//Port config definition
	LPCSTR                lpFileName = "\\\\.\\COM10";
	DWORD                 dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
	DWORD                 dwCreationDisposition = OPEN_EXISTING;
	DWORD                 dwFlagsAndAttributes = FILE_FLAG_OVERLAPPED;
	result.portHandle = CreateFileA(
		lpFileName,
		dwDesiredAccess,
		NULL,
		NULL,
		dwCreationDisposition,
		dwFlagsAndAttributes,
		NULL
	);

	return result;
}

