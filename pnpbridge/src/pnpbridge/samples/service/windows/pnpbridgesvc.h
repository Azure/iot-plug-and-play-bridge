// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "ServiceBase.h"

class PnpBridgeSvc : public ServiceBase
{
public:

    PnpBridgeSvc(PWSTR pszServiceName,
        char * configFilePath,
        BOOL fCanStop = TRUE,
        BOOL fCanShutdown = TRUE,
        BOOL fCanPauseContinue = FALSE);
    virtual ~PnpBridgeSvc(void);

protected:

    virtual void OnStart(DWORD dwArgc, PWSTR *pszArgv);
    virtual void OnStop();

    static void
    __stdcall
    ServiceWorkerThread(
        PTP_CALLBACK_INSTANCE Instance,
        PVOID Context,
        PTP_WORK Work);

private:

    BOOL m_fStopping;
    HANDLE m_hStoppedEvent;
	PTP_WORK m_Work;
    char* m_configFilePath;
};