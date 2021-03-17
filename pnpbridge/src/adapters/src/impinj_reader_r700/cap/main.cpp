/*
 *****************************************************************************
 *                                                                           *
 *                 IMPINJ CONFIDENTIAL AND PROPRIETARY                       *
 *                                                                           *
 * Copyright  Impinj, Inc. 2015.  All rights reserved.                       *
 * Use, modification and/or reproduction permitted only with Impinj          *
 * SpeedwayR, xPortal, and xArray products solely in accordance with terms   *
 * and conditions of applicable Impinj license agreement.                    *
 *                                                                           *
 *****************************************************************************/

#include <iostream>
#include <string>
#include <algorithm>
#include <signal.h>
#include <stdio.h>
#include <cstdlib>
#include "ltkcpp.h"
#include "impinj_ltkcpp.h"
#include "time.h"

#define TID_OP_SPEC_ID          123
#define USER_MEMORY_OP_SPEC_ID  321
#define NUM_ANTENNAS    4

using namespace LLRP;
using namespace std;

class CMyApplication
{
    unsigned int m_messageID;
    unsigned int m_numAntennas;

  public:
    int                         m_Verbose;
    bool                        exitFlag;

    /** Connection to the LLRP reader */
    CConnection *               m_pConnectionToReader;

    inline
    CMyApplication (void)
     : m_Verbose(0), m_pConnectionToReader(NULL)
    {
        m_messageID = 0;
        m_numAntennas = NUM_ANTENNAS;
    }

    int
    run (
      char *                    pReaderHostName,
      unsigned int              numAntennas);

    int
    checkConnectionStatus (void);

    int
    enableImpinjExtensions (void);

    int
    resetConfigurationToFactoryDefaults (void);

    int
    addROSpec (void);

    int
    enableROSpec (void);

    int
    startROSpec (void);

    int
    stopROSpec (void);

    int
    enableGpi(int port);

    int
    awaitAndPrintReport (int timeoutSec);

    void
    printTagReportData (
      CRO_ACCESS_REPORT *       pRO_ACCESS_REPORT);

    void
    printOneTagReportData (
      CTagReportData *          pTagReportData);

    void
    formatOneEPC (
      CParameter *          pEpcParameter,
      char *                buf,
      int                   buflen);

    string
    formatOneReadResult (CParameter *pOpSpecReadResult);

    void
    handleReaderEventNotification (
      CReaderEventNotificationData *pNtfData);

    void
    handleAntennaEvent (
      CAntennaEvent *           pAntennaEvent);

    void
    handleReaderExceptionEvent (
      CReaderExceptionEvent *   pReaderExceptionEvent);

    int
    checkLLRPStatus (
      CLLRPStatus *             pLLRPStatus,
      char *                    pWhatStr);

    CMessage *
    transact (
      CMessage *                pSendMsg);

    CMessage *
    recvMessage (
      int                       nMaxMS);

    int
    sendMessage (
      CMessage *                pSendMsg);

    void
    printXMLMessage (
      CMessage *                pMessage);

    llrp_u1v_t
      hexStrToBitArray(string hex, int lenBits);
};


/* BEGIN forward declarations */
int
main (
  int                           ac,
  char *                        av[]);

CMyApplication              myApp;

void signalHandler(int sig)
{
    cout << "\nSignal " << sig
         << " received. Exiting application.\n";

    myApp.exitFlag = true;
}


/**
 *****************************************************************************
 **
 ** @brief  Command main routine
 **
 ** Command synopsis:
 **
 **     example1 READERHOSTNAME
 **
 ** @exitcode   0               Everything *seemed* to work.
 **             1               Bad usage
 **
 *****************************************************************************/

int
main (
  int                           ac,
  char *                        av[])
{
    char *                      pReaderHostName;
    unsigned int                numAntennas = NUM_ANTENNAS;
    int                         rc;

    // Register signal handles to catch CTRL-C, etc.
    signal(SIGABRT, &signalHandler);
    signal(SIGTERM, &signalHandler);
    signal(SIGINT, &signalHandler);

    // Process command arguments
    if (ac == 1)
    {
        // No arguments provided.
        // Assume the application is running the
        // reader and use loopback address.
        pReaderHostName = "localhost";
    }
    else if (ac == 2)
    {
        pReaderHostName = av[1];
    }
    else if (ac == 3)
    {
        pReaderHostName = av[1];
        numAntennas = atoi(av[2]);
        if (numAntennas < 1 || numAntennas > 52)
        {
            numAntennas = NUM_ANTENNAS;
        }
    }
    else
    {
        // Wrong number of arguments
        return 1;
    }

    // Run application, capture return value for exit status
    rc = myApp.run(pReaderHostName, numAntennas);

    printf("INFO: Done\n");

    return 0;
}

/**
 *****************************************************************************
 **
 ** @brief  Run the application
 **
 ** The steps:
 **     - Instantiate connection
 **     - Connect to LLRP reader (TCP)
 **     - Make sure the connection status is good
 **     - Clear (scrub) the reader configuration
 **     - Configure for what we want to do
 **     - Run inventory operation 5 times
 **     - Again, clear (scrub) the reader configuration
 **     - Disconnect from reader
 **     - Destruct connection
 **
 ** @param[in]  pReaderHostName String with reader name
 **
 ** @return      0              Everything worked.
 **             -1              Failed allocation of type registry
 **             -2              Failed construction of connection
 **             -3              Could not connect to reader
 **
 *****************************************************************************/

int
CMyApplication::run (
  char *                        pReaderHostName,
  unsigned int                  numAntennas)
{
    CTypeRegistry *             pTypeRegistry;
    CConnection *               pConn;
    int                         rc;

    exitFlag = false;
    m_numAntennas = numAntennas;

    /*
     * Allocate the type registry. This is needed
     * by the connection to decode.
     */
    pTypeRegistry = getTheTypeRegistry();
    if(NULL == pTypeRegistry)
    {
        printf("ERROR: getTheTypeRegistry failed\n");
        return -1;
    }

    /*
     * Enroll impinj extension types into the
     * type registry, in preparation for using
     * Impinj extension params.
     */
    LLRP::enrollImpinjTypesIntoRegistry(pTypeRegistry);

    /*
     * Construct a connection (LLRP::CConnection).
     * Using a 32kb max frame size for send/recv.
     * The connection object is ready for business
     * but not actually connected to the reader yet.
     */
    pConn = new CConnection(pTypeRegistry, 32u*1024u);
    if(NULL == pConn)
    {
        printf("ERROR: new CConnection failed\n");
        return -2;
    }

    /*
     * Open the connection to the reader
     */
    if(m_Verbose)
    {
        printf("INFO: Connecting to %s....\n", pReaderHostName);
    }

    rc = pConn->openConnectionToReader(pReaderHostName);
    if(0 != rc)
    {
        printf("ERROR: connect: %s (%d)\n", pConn->getConnectError(), rc);
        delete pConn;
        return -3;
    }

    /*
     * Record the pointer to the connection object so other
     * routines can use it.
     */
    m_pConnectionToReader = pConn;

    if(m_Verbose)
    {
        printf("INFO: Connected, checking status....\n");
    }


    // Configure the reader and wait
    // for tags until CTRL-C is entered
    checkConnectionStatus();
    enableImpinjExtensions();
    resetConfigurationToFactoryDefaults();
    addROSpec();
    enableROSpec();
    //startROSpec();
    while (!exitFlag)
    {
        awaitAndPrintReport(1);
    }
    stopROSpec();
    resetConfigurationToFactoryDefaults();

    /*
     * Close the connection and release its resources
     */
    pConn->closeConnectionToReader();
    delete pConn;

    /*
     * Done with the registry.
     */
    delete pTypeRegistry;

    /*
     * When we get here all allocated memory should have been deallocated.
     */

    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Await and check the connection status message from the reader
 **
 ** We are expecting a READER_EVENT_NOTIFICATION message that
 ** tells us the connection is OK. The reader is suppose to
 ** send the message promptly upon connection.
 **
 ** If there is already another LLRP connection to the
 ** reader we'll get a bad Status.
 **
 ** The message should be something like:
 **
 **     <READER_EVENT_NOTIFICATION MessageID='0'>
 **       <ReaderEventNotificationData>
 **         <UTCTimestamp>
 **           <Microseconds>1184491439614224</Microseconds>
 **         </UTCTimestamp>
 **         <ConnectionAttemptEvent>
 **           <Status>Success</Status>
 **         </ConnectionAttemptEvent>
 **       </ReaderEventNotificationData>
 **     </READER_EVENT_NOTIFICATION>
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::checkConnectionStatus (void)
{
    CMessage *                  pMessage;
    CREADER_EVENT_NOTIFICATION *pNtf;
    CReaderEventNotificationData *pNtfData;
    CConnectionAttemptEvent *   pEvent;

    /*
     * Expect the notification within 10 seconds.
     * It is suppose to be the very first message sent.
     */
    pMessage = recvMessage(10000);

    /*
     * recvMessage() returns NULL if something went wrong.
     */
    if(NULL == pMessage)
    {
        /* recvMessage already tattled */
        goto fail;
    }

    /*
     * Check to make sure the message is of the right type.
     * The type label (pointer) in the message should be
     * the type descriptor for READER_EVENT_NOTIFICATION.
     */
    if(&CREADER_EVENT_NOTIFICATION::s_typeDescriptor != pMessage->m_pType)
    {
        goto fail;
    }

    /*
     * Now that we are sure it is a READER_EVENT_NOTIFICATION,
     * traverse to the ReaderEventNotificationData parameter.
     */
    pNtf = (CREADER_EVENT_NOTIFICATION *) pMessage;
    pNtfData = pNtf->getReaderEventNotificationData();
    if(NULL == pNtfData)
    {
        goto fail;
    }

    /*
     * The ConnectionAttemptEvent parameter must be present.
     */
    pEvent = pNtfData->getConnectionAttemptEvent();
    if(NULL == pEvent)
    {
        goto fail;
    }

    /*
     * The status in the ConnectionAttemptEvent parameter
     * must indicate connection success.
     */
    if(ConnectionAttemptStatusType_Success != pEvent->getStatus())
    {
        goto fail;
    }

    /*
     * Done with the message
     */
    delete pMessage;

    if(m_Verbose)
    {
        printf("INFO: Connection status OK\n");
    }

    /*
     * Victory.
     */
    return 0;

  fail:
    /*
     * Something went wrong. Tattle. Clean up. Return error.
     */
    printf("ERROR: checkConnectionStatus failed\n");
    delete pMessage;
    return -1;
}

llrp_u1v_t
CMyApplication::hexStrToBitArray(string hex, int lenBits)
{
    int i, temp, ptr;
    string cleanHex = "";

    // Remove leading or trialing spaces.
    for (i = 0; i < hex.length(); i++)
    {
        if (hex[i] != ' ')
            cleanHex += hex[i];
    }

    // Convert string to upper case.
    transform(cleanHex.begin(), cleanHex.end(), cleanHex.begin(), ::toupper);

    // Pad out the hex string if necessary
    while ((cleanHex.length() % 2) != 0)
    {
        cleanHex += "0";
    }

    hex = cleanHex;

    // Convert the hex string into an array of bytes.
    llrp_u1v_t result(lenBits);
    ptr = 0;
    for (i = 0; i < hex.length(); i+=2)
    {
        sscanf(hex.c_str() + i, "%2X", &temp);
        result.m_pValue[ptr] = temp;
        ptr++;
    }

    return result;
}

int
CMyApplication::enableGpi(int port)
{
    CSET_READER_CONFIG *pCmd;
    CMessage *pRspMsg;
    CSET_READER_CONFIG_RESPONSE *pRsp;

    // Compose the command message
    pCmd = new CSET_READER_CONFIG();

    // Enable the GPI port
    CGPIPortCurrentState* pGpiPortCurrentState = new CGPIPortCurrentState();
    pGpiPortCurrentState->setGPIPortNum(port);
    pGpiPortCurrentState->setConfig(1);

    pCmd->addGPIPortCurrentState(pGpiPortCurrentState);

    // Send the message
    pRspMsg = transact(pCmd);

    // Done with the command message
    delete pCmd;

    // transact() returns NULL if something went wrong.
    if (pRspMsg == NULL) return -1;

    // Cast to a SET_READER_CONFIG_RESPONSE message.
    pRsp = (CSET_READER_CONFIG_RESPONSE *) pRspMsg;

    // Check the LLRPStatus parameter.
    if (checkLLRPStatus(pRsp->getLLRPStatus(),
                        "enableGpi") != 0)
    {
        delete pRspMsg;
        return -1;
    }

    // Done with the response message.
    delete pRspMsg;

    return 0;
}

/**
 *****************************************************************************
 **
 ** @brief  Send an IMPINJ_ENABLE_EXTENSION_MESSAGE
 **
 ** NB: Send the message to enable the impinj extension.  This must
 ** be done every time we connect to the reader.
 **
 ** The message is:
 ** <Impinj:IMPINJ_ENABLE_EXTENSIONS MessageID="X">
 ** </Impinj:IMPINJ_ENABLE_EXTENSIONS >
 **
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/
int
CMyApplication::enableImpinjExtensions (void)
{
    CIMPINJ_ENABLE_EXTENSIONS *        pCmd;
    CMessage *                         pRspMsg;
    CIMPINJ_ENABLE_EXTENSIONS_RESPONSE *pRsp;

    /*
     * Compose the command message
     */
    pCmd = new CIMPINJ_ENABLE_EXTENSIONS();
    pCmd->setMessageID(m_messageID++);
    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a CIMPINJ_ENABLE_EXTENSIONS_RESPONSE message.
     */
    pRsp = (CIMPINJ_ENABLE_EXTENSIONS_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(),
                        "enableImpinjExtensions"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress, maybe
     */
    if(m_Verbose)
    {
        printf("INFO: Impinj Extensions are enabled\n");
    }

    /*
     * Victory.
     */
    return 0;
}

/**
 *****************************************************************************
 **
 ** @brief  Send a SET_READER_CONFIG message that resets the
 **         reader to factory defaults.
 **
 ** NB: The ResetToFactoryDefault semantics vary between readers.
 **     It might have no effect because it is optional.
 **
 ** The message is:
 **
 **     <SET_READER_CONFIG MessageID='X'>
 **       <ResetToFactoryDefault>1</ResetToFactoryDefault>
 **     </SET_READER_CONFIG>
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::resetConfigurationToFactoryDefaults (void)
{
    CSET_READER_CONFIG *        pCmd;
    CMessage *                  pRspMsg;
    CSET_READER_CONFIG_RESPONSE *pRsp;

    /*
     * Compose the command message
     */
    pCmd = new CSET_READER_CONFIG();
    pCmd->setMessageID(m_messageID++);
    pCmd->setResetToFactoryDefault(1);

    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a SET_READER_CONFIG_RESPONSE message.
     */
    pRsp = (CSET_READER_CONFIG_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(),
                        "resetConfigurationToFactoryDefaults"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress, maybe
     */
    if(m_Verbose)
    {
        printf("INFO: Configuration reset to factory defaults\n");
    }

    /*
     * Victory.
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Add our ROSpec using ADD_ROSPEC message
 **
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::addROSpec (void)
{
    int i, j;

    // Start trigger
    CROSpecStartTrigger *pROSpecStartTrigger = new CROSpecStartTrigger();

    /*
    ///////////////////////////////////////////////////////////////////////////
    // Null (no start trigger. must call START_ROSPEC explicitly)
    ///////////////////////////////////////////////////////////////////////////
    pROSpecStartTrigger->setROSpecStartTriggerType(ROSpecStartTriggerType_Null);
    */

    ///////////////////////////////////////////////////////////////////////////
    // Immediate (start as soon as the ROSpec is added)
    ///////////////////////////////////////////////////////////////////////////
    pROSpecStartTrigger->setROSpecStartTriggerType(ROSpecStartTriggerType_Immediate);

    /*
    ///////////////////////////////////////////////////////////////////////////
    // Periodic start trigger
    ///////////////////////////////////////////////////////////////////////////
    pROSpecStartTrigger->setROSpecStartTriggerType(ROSpecStartTriggerType_Periodic);
    CPeriodicTriggerValue *pPeriodicTriggerValue = new CPeriodicTriggerValue();
    // Offset: Unsigned Integer. Time offset specified in milliseconds.
    pPeriodicTriggerValue->setOffset(1000);
    // Period: Unsigned Integer. Time period specified in milliseconds
    pPeriodicTriggerValue->setPeriod(5000);

    // UTC Time: <UTCTimestamp Parameter> [Optional]
    CUTCTimestamp* pUTCTimestamp = new CUTCTimestamp();
    pUTCTimestamp->setMicroseconds(1456055582000000ULL);
    pPeriodicTriggerValue->setUTCTimestamp(pUTCTimestamp);

    pROSpecStartTrigger->setPeriodicTriggerValue(pPeriodicTriggerValue);
    ///////////////////////////////////////////////////////////////////////////
    */

    /*
    ///////////////////////////////////////////////////////////////////////////
    // GPI start trigger
    ///////////////////////////////////////////////////////////////////////////
    // Enable the GPI first
    enableGpi(1);
    pROSpecStartTrigger->setROSpecStartTriggerType(ROSpecStartTriggerType_GPI);
    CGPITriggerValue *pGpiStartTriggerValue = new CGPITriggerValue();
    pGpiStartTriggerValue->setGPIPortNum(1);
    pGpiStartTriggerValue->setGPIEvent(1);
    pROSpecStartTrigger->setGPITriggerValue(pGpiStartTriggerValue);
    ///////////////////////////////////////////////////////////////////////////
    */

    // Stop trigger
    CROSpecStopTrigger *pROSpecStopTrigger = new CROSpecStopTrigger();

    ///////////////////////////////////////////////////////////////////////////
    // Null (no stop trigger)
    ///////////////////////////////////////////////////////////////////////////
    pROSpecStopTrigger->setROSpecStopTriggerType(ROSpecStopTriggerType_Null);

    /*
    ///////////////////////////////////////////////////////////////////////////
    // Duration stop trigger
    ///////////////////////////////////////////////////////////////////////////
    pROSpecStopTrigger->setROSpecStopTriggerType(ROSpecStopTriggerType_Duration);
    pROSpecStopTrigger->setDurationTriggerValue(1000);
    ///////////////////////////////////////////////////////////////////////////
    */

    /*
    ///////////////////////////////////////////////////////////////////////////
    // GPI stop trigger
    ///////////////////////////////////////////////////////////////////////////
    // Enable the GPI first
    enableGpi(1);
    pROSpecStopTrigger->setROSpecStopTriggerType(ROSpecStopTriggerType_GPI_With_Timeout);
    CGPITriggerValue *pGpiStopTriggerValue = new CGPITriggerValue();
    pGpiStopTriggerValue->setGPIPortNum(1);
    pGpiStopTriggerValue->setGPIEvent(0);
    pGpiStopTriggerValue->setTimeout(0);
    pROSpecStopTrigger->setGPITriggerValue(pGpiStopTriggerValue);
    */

    // ROBoundarySpec includes start and stop triggers.
    CROBoundarySpec *pROBoundarySpec = new CROBoundarySpec();
    pROBoundarySpec->setROSpecStartTrigger(pROSpecStartTrigger);
    pROBoundarySpec->setROSpecStopTrigger(pROSpecStopTrigger);

    /*
    ///////////////////////////////////////////////////////////////////////////
    // Low duty cycle
    ///////////////////////////////////////////////////////////////////////////
    CImpinjLowDutyCycle *pLowDutyCycle = new CImpinjLowDutyCycle();
    pLowDutyCycle->setLowDutyCycleMode(ImpinjLowDutyCycleMode_Enabled);
    pLowDutyCycle->setEmptyFieldTimeout(500);
    pLowDutyCycle->setFieldPingInterval(200);
    pC1G2InventoryCommand->addCustom(pLowDutyCycle);
    */

    /*
    ///////////////////////////////////////////////////////////////////////////
    // C1G2 filter
    ///////////////////////////////////////////////////////////////////////////
    CC1G2TagInventoryMask *pMask = new CC1G2TagInventoryMask();
    pMask->setMB(1);
    // The actual EPC starts at bit 32, past the CRC on PC
    pMask->setPointer(32);
    pMask->setTagMask(hexStrToBitArray("ABBA", 16));
    CC1G2Filter *pFilter = new CC1G2Filter();
    pFilter->setC1G2TagInventoryMask(pMask);
    pC1G2InventoryCommand->addC1G2Filter(pFilter);
    */

    // ROReportSpec
    CROReportSpec *pROrs = new CROReportSpec();
    pROrs->setROReportTrigger(ROReportTriggerType_Upon_N_Tags_Or_End_Of_ROSpec);
    pROrs->setN(1);
    CTagReportContentSelector *pROcontent = new CTagReportContentSelector();
    pROcontent->setEnableAccessSpecID(false);
    pROcontent->setEnableAntennaID(true);
    pROcontent->setEnableChannelIndex(false);
    pROcontent->setEnableFirstSeenTimestamp(true);
    pROcontent->setEnableInventoryParameterSpecID(false);
    pROcontent->setEnableLastSeenTimestamp(false);
    pROcontent->setEnablePeakRSSI(true);
    pROcontent->setEnableROSpecID(false);
    pROcontent->setEnableSpecIndex(false);
    pROcontent->setEnableTagSeenCount(false);
    CC1G2EPCMemorySelector *pC1G2Mem = new CC1G2EPCMemorySelector();
    pC1G2Mem->setEnableCRC(false);
    pC1G2Mem->setEnablePCBits(false);
    pROcontent->addAirProtocolEPCMemorySelector(pC1G2Mem);
    pROrs->setTagReportContentSelector(pROcontent);

    // AISpec stop trigger
    CAISpecStopTrigger *pAISpecStopTrigger = new CAISpecStopTrigger();
    pAISpecStopTrigger->setAISpecStopTriggerType(AISpecStopTriggerType_Null);
    pAISpecStopTrigger->setDurationTrigger(0);

    // Inventory parameter spec
    CInventoryParameterSpec *pInventoryParameterSpec =
                                    new CInventoryParameterSpec();
    pInventoryParameterSpec->setInventoryParameterSpecID(1);
    pInventoryParameterSpec->setProtocolID(AirProtocols_EPCGlobalClass1Gen2);

    // Transmit Power and Receive Sensitivity for each antenna
    for (i=1; i <= m_numAntennas; i++)
    {
        // Reader mode
        CC1G2RFControl* pC1G2RFControl = new CC1G2RFControl();
        pC1G2RFControl->setModeIndex(2);

        // Session
        CC1G2SingulationControl* pC1G2SingulationControl = new CC1G2SingulationControl();
        pC1G2SingulationControl->setSession(2);
        pC1G2SingulationControl->setTagPopulation(32);

        // Inventory search mode
        CImpinjInventorySearchMode *pImpIsm = new CImpinjInventorySearchMode();
        pImpIsm->setInventorySearchMode(ImpinjInventorySearchType_Dual_Target);

        // C1G2InventoryCommand
        CC1G2InventoryCommand* pC1G2InventoryCommand = new CC1G2InventoryCommand();
        pC1G2InventoryCommand->setC1G2RFControl(pC1G2RFControl);
        pC1G2InventoryCommand->setC1G2SingulationControl(pC1G2SingulationControl);
        pC1G2InventoryCommand->addCustom(pImpIsm);

        // Transmitter
        CRFTransmitter* pRFTransmitter = new CRFTransmitter();
        pRFTransmitter->setHopTableID(1);
        pRFTransmitter->setChannelIndex(1);

        // Receiver
        CRFReceiver* pRFReceiver = new CRFReceiver();

        // Set transmit power and receive sensitivity
        // for each antenna
        switch (i)
        {
            case 1:
                pRFTransmitter->setTransmitPower(81);
                pRFReceiver->setReceiverSensitivity(1);
                break;
            case 2:
                pRFTransmitter->setTransmitPower(71);
                pRFReceiver->setReceiverSensitivity(2);
                break;
            case 3:
                pRFTransmitter->setTransmitPower(61);
                pRFReceiver->setReceiverSensitivity(3);
                break;
            case 4:
                pRFTransmitter->setTransmitPower(51);
                pRFReceiver->setReceiverSensitivity(4);
                break;
            default:
                pRFTransmitter->setTransmitPower(81);
                pRFReceiver->setReceiverSensitivity(1);
                break;
        }

        // Antenna config
        CAntennaConfiguration* pAntennaConfig = new CAntennaConfiguration();
        pAntennaConfig->setAntennaID(i);
        pAntennaConfig->setRFTransmitter(pRFTransmitter);
        pAntennaConfig->setRFReceiver(pRFReceiver);
        pAntennaConfig->addAirProtocolInventoryCommandSettings(pC1G2InventoryCommand);
        pInventoryParameterSpec->addAntennaConfiguration(pAntennaConfig);
    }

    // This is used to add Impinj custom fields to the tag report
    CImpinjTagReportContentSelector *pImpContentSelector = new CImpinjTagReportContentSelector();

    // FastID
    CImpinjEnableSerializedTID* pEnableFastId = new CImpinjEnableSerializedTID();
    pEnableFastId->setSerializedTIDMode(ImpinjSerializedTIDMode_Enabled);
    pImpContentSelector->setImpinjEnableSerializedTID(pEnableFastId);

    // Optimized read
    CImpinjEnableOptimizedRead *pOptimizedRead = new CImpinjEnableOptimizedRead();
    pOptimizedRead->setOptimizedReadMode(ImpinjOptimizedReadMode_Enabled);

    // Optimized read TID
    CC1G2Read *pTidOpSpec = new CC1G2Read();
    pTidOpSpec->setAccessPassword(0);
    pTidOpSpec->setMB(2);
    pTidOpSpec->setOpSpecID(TID_OP_SPEC_ID);
    pTidOpSpec->setWordPointer(0);
    pTidOpSpec->setWordCount(2);
    pOptimizedRead->addC1G2Read(pTidOpSpec);

    // Optimized read User memory
    CC1G2Read *pUmOpSpec = new CC1G2Read();
    pUmOpSpec->setAccessPassword(0);
    pUmOpSpec->setMB(3);
    pUmOpSpec->setOpSpecID(USER_MEMORY_OP_SPEC_ID);
    pUmOpSpec->setWordPointer(0);
    pUmOpSpec->setWordCount(1);
    pOptimizedRead->addC1G2Read(pUmOpSpec);

    // Add the optimized read operations to the ROReportSpec
    pImpContentSelector->setImpinjEnableOptimizedRead(pOptimizedRead);
    pROrs->addCustom(pImpContentSelector);

    // One AISpec
    llrp_u16v_t AntennaIDs = llrp_u16v_t(1);
    // Antenna ID zero means that this AISpec applies to all antennas
    AntennaIDs.m_pValue[0] = 0;
    CAISpec* pAISpec = new CAISpec();
    pAISpec->setAntennaIDs(AntennaIDs);
    pAISpec->setAISpecStopTrigger(pAISpecStopTrigger);
    pAISpec->addInventoryParameterSpec(pInventoryParameterSpec);

    // ROSpec
    CROSpec* pROSpec = new CROSpec();
    pROSpec->setROSpecID(1111);
    pROSpec->setPriority(0);
    pROSpec->setCurrentState(ROSpecState_Disabled);
    pROSpec->setROBoundarySpec(pROBoundarySpec);
    pROSpec->addSpecParameter(pAISpec);
    pROSpec->setROReportSpec(pROrs);

    CADD_ROSPEC *               pCmd;
    CMessage *                  pRspMsg;
    CADD_ROSPEC_RESPONSE *      pRsp;

    /*
     * Compose the command message.
     * N.B.: After the message is composed, all the parameters
     *       constructed, immediately above, are considered "owned"
     *       by the command message. When it is destructed so
     *       too will the parameters be.
     */
    pCmd = new CADD_ROSPEC();
    pCmd->setMessageID(m_messageID++);
    pCmd->setROSpec(pROSpec);

    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message.
     * N.B.: And the parameters
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a ADD_ROSPEC_RESPONSE message.
     */
    pRsp = (CADD_ROSPEC_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(), "addROSpec"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress, maybe
     */
    if(m_Verbose)
    {
        printf("INFO: ROSpec added\n");
    }

    /*
     * Victory.
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Enable our ROSpec using ENABLE_ROSPEC message
 **
 ** Enable the ROSpec that was added above.
 **
 ** The message we send is:
 **     <ENABLE_ROSPEC MessageID='X'>
 **       <ROSpecID>123</ROSpecID>
 **     </ENABLE_ROSPEC>
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::enableROSpec (void)
{
    CENABLE_ROSPEC *            pCmd;
    CMessage *                  pRspMsg;
    CENABLE_ROSPEC_RESPONSE *   pRsp;

    /*
     * Compose the command message
     */
    pCmd = new CENABLE_ROSPEC();
    pCmd->setMessageID(m_messageID++);
    pCmd->setROSpecID(1111);

    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a ENABLE_ROSPEC_RESPONSE message.
     */
    pRsp = (CENABLE_ROSPEC_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(), "enableROSpec"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress, maybe
     */
    if(m_Verbose)
    {
        printf("INFO: ROSpec enabled\n");
    }

    /*
     * Victory.
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Start our ROSpec using START_ROSPEC message
 **
 ** Start the ROSpec that was added above.
 **
 ** The message we send is:
 **     <START_ROSPEC MessageID='X'>
 **       <ROSpecID>123</ROSpecID>
 **     </START_ROSPEC>
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::startROSpec (void)
{
    CSTART_ROSPEC *             pCmd;
    CMessage *                  pRspMsg;
    CSTART_ROSPEC_RESPONSE *    pRsp;

    /*
     * Compose the command message
     */
    pCmd = new CSTART_ROSPEC();
    pCmd->setMessageID(m_messageID++);
    pCmd->setROSpecID(1111);

    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a START_ROSPEC_RESPONSE message.
     */
    pRsp = (CSTART_ROSPEC_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(), "startROSpec"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress
     */
    if(m_Verbose)
    {
        printf("INFO: ROSpec started\n");
    }

    /*
     * Victory.
     */
    return 0;
}

/**
 *****************************************************************************
 **
 ** @brief  Stop our ROSpec using STOP_ROSPEC message
 **
 ** Stop the ROSpec that was added above.
 **
 ** The message we send is:
 **     <STOP_ROSPEC MessageID='203'>
 **       <ROSpecID>123</ROSpecID>
 **     </STOP_ROSPEC>
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::stopROSpec (void)
{
    CSTOP_ROSPEC *             pCmd;
    CMessage *                  pRspMsg;
    CSTOP_ROSPEC_RESPONSE *    pRsp;

    /*
     * Compose the command message
     */
    pCmd = new CSTOP_ROSPEC();
    pCmd->setMessageID(m_messageID++);
    pCmd->setROSpecID(1111);

    /*
     * Send the message, expect the response of certain type
     */
    pRspMsg = transact(pCmd);

    /*
     * Done with the command message
     */
    delete pCmd;

    /*
     * transact() returns NULL if something went wrong.
     */
    if(NULL == pRspMsg)
    {
        /* transact already tattled */
        return -1;
    }

    /*
     * Cast to a STOP_ROSPEC_RESPONSE message.
     */
    pRsp = (CSTOP_ROSPEC_RESPONSE *) pRspMsg;

    /*
     * Check the LLRPStatus parameter.
     */
    if(0 != checkLLRPStatus(pRsp->getLLRPStatus(), "stopROSpec"))
    {
        /* checkLLRPStatus already tattled */
        delete pRspMsg;
        return -1;
    }

    /*
     * Done with the response message.
     */
    delete pRspMsg;

    /*
     * Tattle progress
     */
    if(m_Verbose)
    {
        printf("INFO: ROSpec stopped\n");
    }

    /*
     * Victory.
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Receive and print the RO_ACCESS_REPORT
 **
 ** Receive messages for timeout seconds and then stop. Typically
 ** for simple applications, this is sufficient.  For applications with
 ** asyncrhonous reporting or other asyncrhonous activity, it is recommended
 ** to create a thread to perform the report listening.
 **
 ** @param[in]                  timeout
 **
 ** This shows how to determine the type of a received message.
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong
 **
 *****************************************************************************/

int
CMyApplication::awaitAndPrintReport (int timeout)
{
    int                         bDone = 0;
    int                         retVal = 0;
    time_t                      startTime = time(NULL);
    time_t                      tempTime;
    /*
     * Keep receiving messages until done or until
     * something bad happens.
     */
    while(!bDone)
    {
        CMessage *              pMessage;
        const CTypeDescriptor * pType;

        /*
         * Wait up to 1 second for a report.  Check
         * That way, we can check the timestamp even if
         * there are no reports coming in
         */
        pMessage = recvMessage(1000);

        /* validate the timestamp */
        tempTime = time(NULL);
        if(difftime(tempTime, startTime) > timeout)
        {
            bDone=1;
        }

        if(NULL == pMessage)
        {
            continue;
        }

        /*
         * What happens depends on what kind of message
         * received. Use the type label (m_pType) to
         * discriminate message types.
         */
        pType = pMessage->m_pType;

        /*
         * Is it a tag report? If so, print it out.
         */
        if(&CRO_ACCESS_REPORT::s_typeDescriptor == pType)
        {
            CRO_ACCESS_REPORT * pNtf;

            pNtf = (CRO_ACCESS_REPORT *) pMessage;

            printTagReportData(pNtf);
        }

        /*
         * Is it a reader event? This example only recognizes
         * AntennaEvents.
         */
        else if(&CREADER_EVENT_NOTIFICATION::s_typeDescriptor == pType)
        {
            CREADER_EVENT_NOTIFICATION *pNtf;
            CReaderEventNotificationData *pNtfData;

            pNtf = (CREADER_EVENT_NOTIFICATION *) pMessage;

            pNtfData = pNtf->getReaderEventNotificationData();
            if(NULL != pNtfData)
            {
                handleReaderEventNotification(pNtfData);
            }
            else
            {
                /*
                 * This should never happen. Using continue
                 * to keep indent depth down.
                 */
                printf("WARNING: READER_EVENT_NOTIFICATION without data\n");
            }
        }

        /*
         * Hmmm. Something unexpected. Just tattle and keep going.
         */
        else
        {
            printf("WARNING: Ignored unexpected message during monitor: %s\n",
                pType->m_pName);
        }

        /*
         * Done with the received message
         */
        delete pMessage;
    }

    return retVal;
}


/**
 *****************************************************************************
 **
 ** @brief  Helper routine to print a tag report
 **
 ** The report is printed in list order, which is arbitrary.
 **
 ** TODO: It would be cool to sort the list by EPC and antenna,
 **       then print it.
 **
 ** @return     void
 **
 *****************************************************************************/

void
CMyApplication::printTagReportData (
  CRO_ACCESS_REPORT *           pRO_ACCESS_REPORT)
{
    std::list<CTagReportData *>::iterator Cur;

    unsigned int                nEntry = 0;

    /*
     * Loop through and count the number of entries
     */
    for(
        Cur = pRO_ACCESS_REPORT->beginTagReportData();
        Cur != pRO_ACCESS_REPORT->endTagReportData();
        Cur++)
    {
        nEntry++;
    }

    if(m_Verbose)
    {
    printf("INFO: %u tag report entries\n", nEntry);
    }

    /*
     * Loop through again and print each entry.
     */
    for(
        Cur = pRO_ACCESS_REPORT->beginTagReportData();
        Cur != pRO_ACCESS_REPORT->endTagReportData();
        Cur++)
    {
        printOneTagReportData(*Cur);
    }
}


/**
 *****************************************************************************
 **
 ** @brief  Helper routine to print one EPC data parameter
 **
 ** @return     void
 **
 *****************************************************************************/
void
CMyApplication::formatOneEPC (
  CParameter *pEPCParameter,
  char *buf,
  int buflen)
{
    char *              p = buf;
    int                 bufsize = buflen;
    int                 written = 0;

    if(NULL != pEPCParameter)
    {
        const CTypeDescriptor *     pType;
        llrp_u96_t          my_u96;
        llrp_u1v_t          my_u1v;
        llrp_u8_t *         pValue = NULL;
        unsigned int        n, i;

        pType = pEPCParameter->m_pType;
        if(&CEPC_96::s_typeDescriptor == pType)
        {
            CEPC_96             *pEPC_96;

            pEPC_96 = (CEPC_96 *) pEPCParameter;
            my_u96 = pEPC_96->getEPC();
            pValue = my_u96.m_aValue;
            n = 12u;
        }
        else if(&CEPCData::s_typeDescriptor == pType)
        {
            CEPCData *          pEPCData;

            pEPCData = (CEPCData *) pEPCParameter;
            my_u1v = pEPCData->getEPC();
            pValue = my_u1v.m_pValue;
            n = (my_u1v.m_nBit + 7u) / 8u;
        }

        if(NULL != pValue)
        {
            for(i = 0; i < n; i++)
            {
                if(0 < i && i%2 == 0 && 1 < bufsize)
                {
                    *p++ = '-';
                    bufsize--;
                }
                if(bufsize > 2)
                {
                    written = snprintf(p, bufsize, "%02X", pValue[i]);
                    bufsize -= written;
                    p+= written;
                }
            }
        }
        else
        {
            written = snprintf(p, bufsize, "%s", "---unknown-epc-data-type---");
            bufsize -= written;
            p += written;
        }
    }
    else
    {
        written = snprintf(p, bufsize, "%s", "--null epc---");
        bufsize -= written;
        p += written;
    }

    // null terminate this for good practice
    buf[buflen-1] = '\0';

}

/**
 *****************************************************************************
 **
 ** @brief  Helper routine to print one tag report entry on one line
 **
 ** @return     void
 **
 *****************************************************************************/
void
CMyApplication::printOneTagReportData (
  CTagReportData *              pTagReportData)
{
    char                        aBuf[128];
    string epc, tid, userMemory, fastId;
    int custIndex, i;

    /*
     * Print the EPC. It could be an 96-bit EPC_96 parameter
     * or an variable length EPCData parameter.
     */

    CParameter *                pEPCParameter =
                                    pTagReportData->getEPCParameter();

    formatOneEPC(pEPCParameter, aBuf, 128);
    epc = string(aBuf);

    llrp_u16_t antennaId = pTagReportData->getAntennaID()->getAntennaID();
    llrp_u64_t timestamp =
        pTagReportData->getFirstSeenTimestampUTC()->getMicroseconds();
    llrp_s8_t peakRssi = pTagReportData->getPeakRSSI()->getPeakRSSI();

    // Results from an optimized read
    std::list<CParameter *>::iterator Result;
    for (
        Result = pTagReportData->beginAccessCommandOpSpecResult();
        Result != pTagReportData->endAccessCommandOpSpecResult();
        Result++)
    {
        // Is this the result of a read operation?
        if( (*Result)->m_pType == &CC1G2ReadOpSpecResult::s_typeDescriptor)
        {
            CC1G2ReadOpSpecResult *pread = (CC1G2ReadOpSpecResult*) *Result;
            // Are these the TID results?
            if (pread->getOpSpecID() == TID_OP_SPEC_ID)
            {
                tid = formatOneReadResult(*Result);
            }
            // Are these the user memory results?
            else if (pread->getOpSpecID() == USER_MEMORY_OP_SPEC_ID)
            {
                userMemory = formatOneReadResult(*Result);
            }
        }
    }

    // Check for Impinj custom fields in the report.
    // FastId (Serialized TID) will appear here.
    for (
        Result = pTagReportData->beginCustom();
        Result != pTagReportData->endCustom();
        Result++)
    {
        if ((*Result)->m_pType == &CImpinjSerializedTID::s_typeDescriptor)
        {
            CImpinjSerializedTID* pSerializedTid = (CImpinjSerializedTID*) *Result;
            llrp_u16v_t rawTid = pSerializedTid->getTID();
            char tidBuf[5];
            fastId = "";
            for (i = 0; i < rawTid.m_nValue; i++)
            {
                sprintf(tidBuf, "%04X", rawTid.m_pValue[i]);
                fastId += string(tidBuf);
            }
        }
    }


    cout << "Tag report\n"
         << "-----------------------------------\n"
         << "Antenna ID : " << antennaId << endl
         << "EPC : " << epc << endl
         << "Timestamp : " << timestamp << endl
         << "Peak RSSI : " << (int) peakRssi << endl
         << "FastID : " << fastId << endl
         << "TID : " << tid << endl
         << "User memory (Word 0) : " << userMemory << endl
         << "\n\n";
}

string
CMyApplication::formatOneReadResult (CParameter *pOpSpecReadResult)
{
    char buffer[256];
    char *p = buffer;

    // Cast the parameter to the correct type
    CC1G2ReadOpSpecResult *pread = (CC1G2ReadOpSpecResult*) pOpSpecReadResult;

    // Check the result of the read operation
    EC1G2ReadResultType result = pread->getResult();

    // If it was successful, then return the results
    if(result == C1G2ReadResultType_Success)
    {
        llrp_u16v_t readData = pread->getReadData();

        for(int i = 0; i < readData.m_nValue; i++)
        {
            int count = sprintf(p, "%04X", readData.m_pValue[i]);
            p += count;
        }
    }
    else
    {
        // The read failed. Return an empty string
        buffer[0] = '\0';
    }

    *p = '\0';
    return string(buffer);
}


/**
 *****************************************************************************
 **
 ** @brief  Handle a ReaderEventNotification
 **
 ** Handle the payload of a READER_EVENT_NOTIFICATION message.
 ** This routine simply dispatches to handlers of specific
 ** event types.
 **
 ** @return     void
 **
 *****************************************************************************/

void
CMyApplication::handleReaderEventNotification (
  CReaderEventNotificationData *pNtfData)
{
    CAntennaEvent *             pAntennaEvent;
    CReaderExceptionEvent *     pReaderExceptionEvent;
    int                         nReported = 0;

    pAntennaEvent = pNtfData->getAntennaEvent();
    if(NULL != pAntennaEvent)
    {
        handleAntennaEvent(pAntennaEvent);
        nReported++;
    }

    pReaderExceptionEvent = pNtfData->getReaderExceptionEvent();
    if(NULL != pReaderExceptionEvent)
    {
        handleReaderExceptionEvent(pReaderExceptionEvent);
        nReported++;
    }

    /*
     * Similarly handle other events here:
     *      HoppingEvent
     *      GPIEvent
     *      ROSpecEvent
     *      ReportBufferLevelWarningEvent
     *      ReportBufferOverflowErrorEvent
     *      RFSurveyEvent
     *      AISpecEvent
     *      ConnectionAttemptEvent
     *      ConnectionCloseEvent
     *      Custom
     */

    if(0 == nReported)
    {
        printf("NOTICE: Unexpected (unhandled) ReaderEvent\n");
    }
}


/**
 *****************************************************************************
 **
 ** @brief  Handle an AntennaEvent
 **
 ** An antenna was disconnected or (re)connected. Tattle.
 **
 ** @return     void
 **
 *****************************************************************************/

void
CMyApplication::handleAntennaEvent (
  CAntennaEvent *               pAntennaEvent)
{
    EAntennaEventType           eEventType;
    llrp_u16_t                  AntennaID;
    char *                      pStateStr;

    eEventType = pAntennaEvent->getEventType();
    AntennaID = pAntennaEvent->getAntennaID();

    switch(eEventType)
    {
    case AntennaEventType_Antenna_Disconnected:
        pStateStr = "disconnected";
        break;

    case AntennaEventType_Antenna_Connected:
        pStateStr = "connected";
        break;

    default:
        pStateStr = "?unknown-event?";
        break;
    }

    printf("NOTICE: Antenna %d is %s\n", AntennaID, pStateStr);
}


/**
 *****************************************************************************
 **
 ** @brief  Handle a ReaderExceptionEvent
 **
 ** Something has gone wrong. There are lots of details but
 ** all this does is print the message, if one.
 **
 ** @return     void
 **
 *****************************************************************************/

void
CMyApplication::handleReaderExceptionEvent (
  CReaderExceptionEvent *       pReaderExceptionEvent)
{
    llrp_utf8v_t                Message;

    Message = pReaderExceptionEvent->getMessage();

    if(0 < Message.m_nValue && NULL != Message.m_pValue)
    {
        printf("NOTICE: ReaderException '%.*s'\n",
             Message.m_nValue, Message.m_pValue);
    }
    else
    {
        printf("NOTICE: ReaderException but no message\n");
    }
}


/**
 *****************************************************************************
 **
 ** @brief  Helper routine to check an LLRPStatus parameter
 **         and tattle on errors
 **
 ** Helper routine to interpret the LLRPStatus subparameter
 ** that is in all responses. It tattles on an error, if one,
 ** and tries to safely provide details.
 **
 ** This simplifies the code, above, for common check/tattle
 ** sequences.
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong, already tattled
 **
 *****************************************************************************/

int
CMyApplication::checkLLRPStatus (
  CLLRPStatus *                 pLLRPStatus,
  char *                        pWhatStr)
{
    /*
     * The LLRPStatus parameter is mandatory in all responses.
     * If it is missing there should have been a decode error.
     * This just makes sure (remember, this program is a
     * diagnostic and suppose to catch LTKC mistakes).
     */
    if(NULL == pLLRPStatus)
    {
        printf("ERROR: %s missing LLRP status\n", pWhatStr);
        return -1;
    }

    /*
     * Make sure the status is M_Success.
     * If it isn't, print the error string if one.
     * This does not try to pretty-print the status
     * code. To get that, run this program with -vv
     * and examine the XML output.
     */
    if(StatusCode_M_Success != pLLRPStatus->getStatusCode())
    {
        llrp_utf8v_t            ErrorDesc;

        ErrorDesc = pLLRPStatus->getErrorDescription();

        if(0 == ErrorDesc.m_nValue)
        {
            printf("ERROR: %s failed, no error description given\n",
                pWhatStr);
        }
        else
        {
            printf("ERROR: %s failed, %.*s\n",
                pWhatStr, ErrorDesc.m_nValue, ErrorDesc.m_pValue);
        }
        return -2;
    }

    /*
     * Victory. Everything is fine.
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Wrapper routine to do an LLRP transaction
 **
 ** Wrapper to transact a request/resposne.
 **     - Print the outbound message in XML if verbose level is at least 2
 **     - Send it using the LLRP_Conn_transact()
 **     - LLRP_Conn_transact() receives the response or recognizes an error
 **     - Tattle on errors, if any
 **     - Print the received message in XML if verbose level is at least 2
 **     - If the response is ERROR_MESSAGE, the request was sufficiently
 **       misunderstood that the reader could not send a proper reply.
 **       Deem this an error, free the message.
 **
 ** The message returned resides in allocated memory. It is the
 ** caller's obligtation to free it.
 **
 ** @return     ==NULL          Something went wrong, already tattled
 **             !=NULL          Pointer to a message
 **
 *****************************************************************************/

CMessage *
CMyApplication::transact (
  CMessage *                    pSendMsg)
{
    CConnection *               pConn = m_pConnectionToReader;
    CMessage *                  pRspMsg;

    /*
     * Print the XML text for the outbound message if
     * verbosity is 2 or higher.
     */
    if(1 < m_Verbose)
    {
        printf("\n===================================\n");
        printf("INFO: Transact sending\n");
        printXMLMessage(pSendMsg);
    }

    /*
     * Send the message, expect the response of certain type.
     * If LLRP::CConnection::transact() returns NULL then there was
     * an error. In that case we try to print the error details.
     */
    pRspMsg = pConn->transact(pSendMsg, 5000);

    if(NULL == pRspMsg)
    {
        const CErrorDetails *   pError = pConn->getTransactError();

        printf("ERROR: %s transact failed, %s\n",
            pSendMsg->m_pType->m_pName,
            pError->m_pWhatStr ? pError->m_pWhatStr : "no reason given");

        if(NULL != pError->m_pRefType)
        {
            printf("ERROR: ... reference type %s\n",
                pError->m_pRefType->m_pName);
        }

        if(NULL != pError->m_pRefField)
        {
            printf("ERROR: ... reference field %s\n",
                pError->m_pRefField->m_pName);
        }

        return NULL;
    }

    /*
     * Print the XML text for the inbound message if
     * verbosity is 2 or higher.
     */
    if(1 < m_Verbose)
    {
        printf("\n- - - - - - - - - - - - - - - - - -\n");
        printf("INFO: Transact received response\n");
        printXMLMessage(pRspMsg);
    }

    /*
     * If it is an ERROR_MESSAGE (response from reader
     * when it can't understand the request), tattle
     * and declare defeat.
     */
    if(&CERROR_MESSAGE::s_typeDescriptor == pRspMsg->m_pType)
    {
        const CTypeDescriptor * pResponseType;

        pResponseType = pSendMsg->m_pType->m_pResponseType;

        printf("ERROR: Received ERROR_MESSAGE instead of %s\n",
            pResponseType->m_pName);
        delete pRspMsg;
        pRspMsg = NULL;
    }

    return pRspMsg;
}


/**
 *****************************************************************************
 **
 ** @brief  Wrapper routine to receive a message
 **
 ** This can receive notifications as well as responses.
 **     - Recv a message using the LLRP_Conn_recvMessage()
 **     - Tattle on errors, if any
 **     - Print the message in XML if verbose level is at least 2
 **
 ** The message returned resides in allocated memory. It is the
 ** caller's obligtation to free it.
 **
 ** @param[in]  nMaxMS          -1 => block indefinitely
 **                              0 => just peek at input queue and
 **                                   socket queue, return immediately
 **                                   no matter what
 **                             >0 => ms to await complete frame
 **
 ** @return     ==NULL          Something went wrong, already tattled
 **             !=NULL          Pointer to a message
 **
 *****************************************************************************/

CMessage *
CMyApplication::recvMessage (
  int                           nMaxMS)
{
    CConnection *               pConn = m_pConnectionToReader;
    CMessage *                  pMessage;

    /*
     * Receive the message subject to a time limit
     */
    pMessage = pConn->recvMessage(nMaxMS);

    /*
     * If LLRP::CConnection::recvMessage() returns NULL then there was
     * an error. In that case we try to print the error details.
     */
    if(NULL == pMessage)
    {
        const CErrorDetails *   pError = pConn->getRecvError();

        /* don't warn on timeout since this is a polling example */
        if(pError->m_eResultCode != RC_RecvTimeout)
        {
        printf("ERROR: recvMessage failed, %s\n",
            pError->m_pWhatStr ? pError->m_pWhatStr : "no reason given");
        }

        if(NULL != pError->m_pRefType)
        {
            printf("ERROR: ... reference type %s\n",
                pError->m_pRefType->m_pName);
        }

        if(NULL != pError->m_pRefField)
        {
            printf("ERROR: ... reference field %s\n",
                pError->m_pRefField->m_pName);
        }

        return NULL;
    }

    /*
     * Print the XML text for the inbound message if
     * verbosity is 2 or higher.
     */
    if(1 < m_Verbose)
    {
        printf("\n===================================\n");
        printf("INFO: Message received\n");
        printXMLMessage(pMessage);
    }

    return pMessage;
}


/**
 *****************************************************************************
 **
 ** @brief  Wrapper routine to send a message
 **
 ** Wrapper to send a message.
 **     - Print the message in XML if verbose level is at least 2
 **     - Send it using the LLRP_Conn_sendMessage()
 **     - Tattle on errors, if any
 **
 ** @param[in]  pSendMsg        Pointer to message to send
 **
 ** @return     ==0             Everything OK
 **             !=0             Something went wrong, already tattled
 **
 *****************************************************************************/

int
CMyApplication::sendMessage (
  CMessage *                    pSendMsg)
{
    CConnection *               pConn = m_pConnectionToReader;

    /*
     * Print the XML text for the outbound message if
     * verbosity is 2 or higher.
     */
    if(1 < m_Verbose)
    {
        printf("\n===================================\n");
        printf("INFO: Sending\n");
        printXMLMessage(pSendMsg);
    }

    /*
     * If LLRP::CConnection::sendMessage() returns other than RC_OK
     * then there was an error. In that case we try to print
     * the error details.
     */
    if(RC_OK != pConn->sendMessage(pSendMsg))
    {
        const CErrorDetails *   pError = pConn->getSendError();

        printf("ERROR: %s sendMessage failed, %s\n",
            pSendMsg->m_pType->m_pName,
            pError->m_pWhatStr ? pError->m_pWhatStr : "no reason given");

        if(NULL != pError->m_pRefType)
        {
            printf("ERROR: ... reference type %s\n",
                pError->m_pRefType->m_pName);
        }

        if(NULL != pError->m_pRefField)
        {
            printf("ERROR: ... reference field %s\n",
                pError->m_pRefField->m_pName);
        }

        return -1;
    }

    /*
     * Victory
     */
    return 0;
}


/**
 *****************************************************************************
 **
 ** @brief  Helper to print a message as XML text
 **
 ** Print a LLRP message as XML text
 **
 ** @param[in]  pMessage        Pointer to message to print
 **
 ** @return     void
 **
 *****************************************************************************/

void
CMyApplication::printXMLMessage (
  CMessage *                    pMessage)
{
    char                        aBuf[100*1024];

    /*
     * Convert the message to an XML string.
     * This fills the buffer with either the XML string
     * or an error message. The return value could
     * be checked.
     */

    pMessage->toXMLString(aBuf, sizeof aBuf);

    /*
     * Print the XML Text to the standard output.
     */
    printf("%s", aBuf);
}
