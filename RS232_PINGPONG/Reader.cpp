#include <stdio.h>
#include "Read.h" 
#include "mttty.h"
#include <commctrl.h>

#define AMOUNT_TO_READ          512
#define NUM_READSTAT_HANDLES    4

void ErrorInRead(const char * err)
{
	printf("READ: %s\n", err);
}

void ReportStatusEvent(unsigned long val)
{
	printf("READ: Report Status Event: %l\n", val);
}

DWORD WINAPI Read(void * lpV)
{
	OVERLAPPED osReader = { 0 };  // overlapped structure for read operations
	OVERLAPPED osStatus = { 0 };  // overlapped structure for status operations
	HANDLE     hArray[NUM_READSTAT_HANDLES];
	DWORD      dwStoredFlags = 0xFFFFFFFF;      // local copy of event flags
	DWORD      dwCommEvent;     // result from WaitCommEvent
	DWORD      dwOvRes;         // result from GetOverlappedResult
	DWORD 	   dwRead;          // bytes actually read
	DWORD      dwRes;           // result from WaitForSingleObject
	BOOL       fWaitingOnRead = FALSE;
	BOOL       fWaitingOnStat = FALSE;
	BOOL       fThreadDone = FALSE;
	char   	   lpBuf[AMOUNT_TO_READ];
	HWND  	   hTTY;
	hTTY = (HWND)(LPVOID) NULL;


	//
	// create two overlapped structures, one for read events
	// and another for status events
	//
	osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osReader.hEvent == NULL)
		printf("READ: Error - CreateEvent (Reader Event)\n");

	osStatus.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osStatus.hEvent == NULL)
		printf("READ: Error - CreateEvent (Status Event)\n");

	//
	// We want to detect the following events:
	//   Read events (from ReadFile)
	//   Status events (from WaitCommEvent)
	//   Status message events (from our UpdateStatus)
	//   Thread exit evetns (from our shutdown functions)
	//
	hArray[0] = osReader.hEvent;
	hArray[1] = osStatus.hEvent;
	hArray[2] = 0;//ghStatusMessageEvent;
	hArray[3] = 0;// ghThreadExitEvent;


	while (!fThreadDone) {

		//
		// If no reading is allowed, then set flag to
		// make it look like a read is already outstanding.
		//
		fWaitingOnRead = TRUE;

		//
		// if no read is outstanding, then issue another one
		//
		if (!fWaitingOnRead) {
			if (!ReadFile(COMDEV(TTYInfo), lpBuf, AMOUNT_TO_READ, &dwRead, &osReader)) {
				if (GetLastError() != ERROR_IO_PENDING)	  // read not delayed?
					printf("READ: Error - ReadFile in ReaderAndStatusProc\n");

				fWaitingOnRead = TRUE;
			}
			else {    // read completed immediately
				if ((dwRead != MAX_READ_BUFFER) && SHOWTIMEOUTS(TTYInfo))
					printf("READ: Error - Read timed out immediately.\r\n");

				if (dwRead)
					//OutputABuffer(hTTY, lpBuf, dwRead);
					printf("output *****************\n");
			}
		}

		//
		// If status flags have changed, then reset comm mask.
		// This will cause a pending WaitCommEvent to complete
		// and the resultant event flag will be NULL.
		//
		if (dwStoredFlags != EVENTFLAGS(TTYInfo)) {
			dwStoredFlags = EVENTFLAGS(TTYInfo);
			if (!SetCommMask(COMDEV(TTYInfo), dwStoredFlags))
				printf("READ: SetCommMask error\n");
		}

		//
		// If event checks are not allowed, then make it look
		// like an event check operation is outstanding
		//
		if (NOEVENTS(TTYInfo))
			fWaitingOnStat = TRUE;
		//
		// if no status check is outstanding, then issue another one
		//
		if (!fWaitingOnStat) {
			if (NOEVENTS(TTYInfo))
				fWaitingOnStat = TRUE;
			else {
				if (!WaitCommEvent(COMDEV(TTYInfo), &dwCommEvent, &osStatus)) {
					if (GetLastError() != ERROR_IO_PENDING)	  // Wait not delayed?
						printf("READ: Error at WaitCommEvent\n");
					else
						fWaitingOnStat = TRUE;
				}
				else
					// WaitCommEvent returned immediately
					ReportStatusEvent(dwCommEvent);
			}
		}

		//
		// wait for pending operations to complete
		//
		if (fWaitingOnStat && fWaitingOnRead) {
			dwRes = WaitForMultipleObjects(NUM_READSTAT_HANDLES, hArray, FALSE, STATUS_CHECK_TIMEOUT);
			switch (dwRes)
			{
				//
				// read completed
				//
			case WAIT_OBJECT_0:
				if (!GetOverlappedResult(COMDEV(TTYInfo), &osReader, &dwRead, FALSE)) {
					if (GetLastError() == ERROR_OPERATION_ABORTED)
						printf("Read aborted\r\n");
					else
						printf("READ: Error - GetOverlappedResult (in Reader)\n");
				}
				else {      // read completed successfully
					if ((dwRead != MAX_READ_BUFFER) && SHOWTIMEOUTS(TTYInfo))
						printf("Read timed out overlapped.\r\n");

					if (dwRead)
						//OutputABuffer(hTTY, lpBuf, dwRead);
						printf("***************************+++\n");
				}

				fWaitingOnRead = FALSE;
				break;

				//
				// status completed
				//
			case WAIT_OBJECT_0 + 1:
				if (!GetOverlappedResult(COMDEV(TTYInfo), &osStatus, &dwOvRes, FALSE)) {
					if (GetLastError() == ERROR_OPERATION_ABORTED)
						printf("WaitCommEvent aborted\r\n");
					else
						printf("READ: Error - GetOverlappedResult (in Reader)\n");
				}
				else       // status check completed successfully
					ReportStatusEvent(dwCommEvent);

				fWaitingOnStat = FALSE;
				break;

				//
				// status message event
				//
			case WAIT_OBJECT_0 + 2:
				StatusMessage();
				break;

				//
				// thread exit event
				//
			case WAIT_OBJECT_0 + 3:
				fThreadDone = TRUE;
				break;

			case WAIT_TIMEOUT:
				//
				// timeouts are not reported because they happen too often
				// OutputDebugString("Timeout in Reader & Status checking\n\r");
				//

				//
				// if status checks are not allowed, then don't issue the
				// modem status check nor the com stat check
				//
				if (!NOSTATUS(TTYInfo)) {
					CheckModemStatus(FALSE);    // take this opportunity to do
					CheckComStat(FALSE);        //   a modem status check and
												//   a comm status check
				}

				break;

			default:
				printf("READ: Error - WaitForMultipleObjects(Reader & Status handles)\n");
				break;
			}
		}
	}

	//
	// close event handles
	//
	CloseHandle(osReader.hEvent);
	CloseHandle(osStatus.hEvent);

	return 1;
}