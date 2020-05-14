// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma region Includes
#include "pnpbridge.h"
#include "pnpbridgesvc.h"
#pragma endregion


PnpBridgeSvc::PnpBridgeSvc(PWSTR pszServiceName,
                            char * configFilePath,
                            BOOL fCanStop,
                            BOOL fCanShutdown,
                            BOOL fCanPauseContinue)
: ServiceBase(pszServiceName, fCanStop, fCanShutdown, fCanPauseContinue)
{
    mallocAndStrcpy_s(&m_configFilePath, (const char*) configFilePath);
    m_fStopping = FALSE;

    // Create a manual-reset event that is not signaled at first to indicate 
    // the stopped signal of the service.
    m_hStoppedEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_hStoppedEvent == NULL)
    {
        throw GetLastError();
    }
	
    // Create work with the callback environment.
    m_Work = CreateThreadpoolWork(PnpBridgeSvc::ServiceWorkerThread, this, NULL);
	if (m_Work == NULL)
	{
        throw GetLastError();
    }
}


PnpBridgeSvc::~PnpBridgeSvc(void)
{
    if (m_hStoppedEvent)
    {
        CloseHandle(m_hStoppedEvent);
        m_hStoppedEvent = NULL;
    }

    if (m_Work) {
        CloseThreadpoolWork(m_Work);
    }
}


//
//   FUNCTION: PnpBridgeSvc::OnStart(DWORD, LPWSTR *)
//
//   PURPOSE: The function is executed when a Start command is sent to the 
//   service by the SCM or when the operating system starts (for a service 
//   that starts automatically). It specifies actions to take when the 
//   service starts. In this code sample, OnStart logs a service-start 
//   message to the Application log, and queues the main service function for 
//   execution in a thread pool worker thread.
//
//   PARAMETERS:
//   * dwArgc   - number of command line arguments
//   * lpszArgv - array of command line arguments
//
//   NOTE: A service application is designed to be long running. Therefore, 
//   it usually polls or monitors something in the system. The monitoring is 
//   set up in the OnStart method. However, OnStart does not actually do the 
//   monitoring. The OnStart method must return to the operating system after 
//   the service's operation has begun. It must not loop forever or block. To 
//   set up a simple monitoring mechanism, one general solution is to create 
//   a timer in OnStart. The timer would then raise events in your code 
//   periodically, at which time your service could do its monitoring. The 
//   other solution is to spawn a new thread to perform the main service 
//   functions, which is demonstrated in this code sample.
//
void PnpBridgeSvc::OnStart(DWORD dwArgc, LPWSTR *lpszArgv)
{
    UNREFERENCED_PARAMETER(dwArgc);
    UNREFERENCED_PARAMETER(lpszArgv);

    // Log a service start message to the Application log.
    WriteEventLogEntry(L"PnpBridgeSvc in OnStart", 
        EVENTLOG_INFORMATION_TYPE);

	//
    // Submit the work to the pool. Because this was a pre-allocated
    // work item (using CreateThreadpoolWork), it is guaranteed to execute.
    //
    SubmitThreadpoolWork(m_Work);
}


//
//   FUNCTION: PnpBridgeSvc::ServiceWorkerThread(void)
//
//   PURPOSE: The method performs the main function of the service. It runs 
//   on a thread pool worker thread.
//
void PnpBridgeSvc::ServiceWorkerThread(PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work)
{
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Work);

    PnpBridgeSvc* svc = static_cast<PnpBridgeSvc*>(Context);
    // Periodically check if the service is stopping.
    while (!svc->m_fStopping)
    {
        // Perform main service function here...
        ::PnpBridge_Main(svc->m_configFilePath);
    }

    // Signal the stopped event.
    SetEvent(svc->m_hStoppedEvent);
}

//
//   FUNCTION: PnpBridgeSvc::OnStop(void)
//
//   PURPOSE: The function is executed when a Stop command is sent to the 
//   service by SCM. It specifies actions to take when a service stops 
//   running. In this code sample, OnStop logs a service-stop message to the 
//   Application log, and waits for the finish of the main service function.
//
//   COMMENTS:
//   Be sure to periodically call ReportServiceStatus() with 
//   SERVICE_STOP_PENDING if the procedure is going to take long time. 
//
void PnpBridgeSvc::OnStop()
{
    // Log a service stop message to the Application log.
    WriteEventLogEntry(L"PnpBridgeSvc in OnStop", 
        EVENTLOG_INFORMATION_TYPE);

    // Indicate that the service is stopping and wait for the finish of the 
    // main service function (ServiceWorkerThread).
    m_fStopping = TRUE;
    ::PnpBridge_Stop();

    if (WaitForSingleObject(m_hStoppedEvent, INFINITE) != WAIT_OBJECT_0)
    {
        throw GetLastError();
    }
}