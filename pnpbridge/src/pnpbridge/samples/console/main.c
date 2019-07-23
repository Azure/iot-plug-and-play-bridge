#include <pnpbridge.h>
#include "azure_c_shared_utility/xlogging.h"

#ifdef WIN32
#include <windows.h>

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        // Handle the CTRL-C signal. 
    case CTRL_C_EVENT:
        PnpBridge_Stop();
        return TRUE;

    default:
        return FALSE;
        //
    }
}
#else 
#include <signal.h>
void CtrlHandler(int s) {
    PnpBridge_Stop();
}
#endif

int main()
{
    LogInfo("\n -- Press Ctrl+C to stop PnpBridge\n");

#ifdef WIN32
    SetConsoleCtrlHandler(CtrlHandler, TRUE);
#else
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = CtrlHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
#endif

    PnpBridge_Main();

    return 0;
}
