// ------------------------------------------------------------------
// CMD
// ------------------------------------------------------------------
// Author:    nlv10677
// Copyright: NXP B.V. 2014. All rights reserved
// ------------------------------------------------------------------

/** \addtogroup zb
 * \file
 * \brief JSON parser to "cmd" messages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "zcb.h"
#include "queue.h"
#include "systemtable.h"
#include "SerialLink.h"

#include "iotSleep.h"
#include "nibbles.h"
#include "jsonCreate.h"
#include "parsing.h"
#include "dump.h"
#include "newLog.h"
#include "cmd.h"

// #define CMD_DEBUG  1

#ifdef CMD_DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif /* CMD_DEBUG */

// ------------------------------------------------------------------
// External prototype
// ------------------------------------------------------------------

extern void dispatchMessage( int queue, char * message );
extern int writeControl( char * cmd );

// ------------------------------------------------------------------
// Attributes
// ------------------------------------------------------------------
    
#define NUMINTATTRS  6
static char * intAttrs[NUMINTATTRS] = { "reset", "startnwk",
    "erase", "getversion", "getpermit", "duration" };

#define NUMSTRINGATTRS  4
static char * stringAttrs[NUMSTRINGATTRS] = {
    "chanmask", "setpermit", "authorize", "linkkey" };
static int    stringMaxlens[NUMSTRINGATTRS] = {
         8,          16,         16,         32 };

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

static teZcbStatus eAuthenticateDevice(uint64_t u64IEEEAddress, uint8_t *pau8LinkKey ) {

    struct _AuthenticateRequest {
        uint64_t    u64IEEEAddress;
        uint8_t     au8LinkKey[16];
    } __attribute__((__packed__)) sAuthenticateRequest;
    
    uint8_t u8SequenceNo;
    teZcbStatus eStatus = E_ZCB_COMMS_FAILED;
    
    sAuthenticateRequest.u64IEEEAddress = htobe64(u64IEEEAddress);
    memcpy(sAuthenticateRequest.au8LinkKey, pau8LinkKey, 16);
    
    if (eSL_SendMessage(E_SL_MSG_AUTHENTICATE_DEVICE_REQUEST, sizeof(struct _AuthenticateRequest),
             &sAuthenticateRequest, &u8SequenceNo) == E_SL_OK) {
        eStatus = E_ZCB_OK;
    }

    return eStatus;
}

int eGetPermitJoining(void) {
    if (eSL_SendMessage(E_SL_MSG_GET_PERMIT_JOIN, 0, NULL, NULL) != E_SL_OK) {
        return 0;
    }
    return 1;
}

static teZcbStatus eSetPermitJoining(uint8_t u8Interval) {

    struct _PermitJoiningMessage {
        uint16_t    u16TargetAddress;
        uint8_t     u8Interval;
        uint8_t     u8TCSignificance;
    } __attribute__((__packed__)) sPermitJoiningMessage;

    sPermitJoiningMessage.u16TargetAddress  = htons(E_ZB_BROADCAST_ADDRESS_ROUTERS);
    sPermitJoiningMessage.u8Interval        = u8Interval;
    sPermitJoiningMessage.u8TCSignificance  = 0;
    
    dump( (char *)&sPermitJoiningMessage, sizeof(struct _PermitJoiningMessage) );

    if (eSL_SendMessage(E_SL_MSG_PERMIT_JOINING_REQUEST, sizeof(struct _PermitJoiningMessage),
           &sPermitJoiningMessage, NULL) != E_SL_OK) {
        return E_ZCB_COMMS_FAILED;
    }
    eGetPermitJoining();
    return E_ZCB_OK;
}

static teZcbStatus eSetChannelMask(uint32_t u32ChannelMask) {

    struct _SetChannelMaskMessage {
        uint32_t    u32ChannelMask;
    } __attribute__((__packed__)) sSetChannelMaskMessage;

    sSetChannelMaskMessage.u32ChannelMask  = htonl(u32ChannelMask);

    if (eSL_SendMessage(E_SL_MSG_SET_CHANNELMASK, sizeof(struct _SetChannelMaskMessage),
            &sSetChannelMaskMessage, NULL) != E_SL_OK) {
        return E_ZCB_COMMS_FAILED;
    }
    newDbSystemSaveIntval( "zcb_channel", u32ChannelMask );
    return E_ZCB_OK;
}

static teZcbStatus eSetDeviceType(teModuleMode eModuleMode) {

    struct _SetDeviceTypeMessage {
        uint16_t    u8ModuleMode;
    } __attribute__((__packed__)) sSetDeviceTypeMessage;

    sSetDeviceTypeMessage.u8ModuleMode = eModuleMode;

    if (eSL_SendMessage(E_SL_MSG_SET_DEVICETYPE, sizeof(struct _SetDeviceTypeMessage),
            &sSetDeviceTypeMessage, NULL) != E_SL_OK) {
        return E_ZCB_COMMS_FAILED;
    }

    return E_ZCB_OK;
}

static teZcbStatus eStartNetwork(void) {
    if (eSL_SendMessage(E_SL_MSG_START_NETWORK, 0, NULL, NULL) != E_SL_OK) {
        return E_ZCB_COMMS_FAILED;
    }
    
    return E_ZCB_OK;
}

// ------------------------------------------------------------------
// Response Handlers
// ------------------------------------------------------------------

static void HandleAuthResponse(void *pvUser, uint16_t u16Length, void *pvMessage) {
    printf( "\n************ HandleAuthResponse\n" );
    dump( (char *)pvMessage, u16Length );

    struct _AuthenticateResponse {
        uint64_t    u64IEEEAddress;
        uint8_t     au8NetworkKey[16];
        uint8_t     au8MIC[4];
        uint64_t    u64TrustCenterAddress;
        uint8_t     u8KeySequenceNumber;
        uint8_t     u8Channel;
        uint16_t    u16PanID;
        uint64_t    u64PanID;
    } __attribute__((__packed__)) *psAuthenticateResponse = (struct _AuthenticateResponse *)pvMessage;

    psAuthenticateResponse->u64IEEEAddress        = be64toh(psAuthenticateResponse->u64IEEEAddress);
    psAuthenticateResponse->u64TrustCenterAddress = be64toh(psAuthenticateResponse->u64TrustCenterAddress);
    psAuthenticateResponse->u16PanID = ntohs(psAuthenticateResponse->u16PanID);
    psAuthenticateResponse->u64PanID = be64toh(psAuthenticateResponse->u64PanID);
            
    DEBUG_PRINTF( "Got authentication data for device 0x%016llX\n",
                  (unsigned long long int)psAuthenticateResponse->u64IEEEAddress);
            
    DEBUG_PRINTF( "Trust center address: 0x%016llX\n",
                  (unsigned long long int)psAuthenticateResponse->u64TrustCenterAddress);
    DEBUG_PRINTF( "Key sequence number : %02d\n", psAuthenticateResponse->u8KeySequenceNumber);
    DEBUG_PRINTF( "NWKEY               : " ); dump( (char *)psAuthenticateResponse->au8NetworkKey, 16 );
    DEBUG_PRINTF( "MIC                 : " ); dump( (char *)psAuthenticateResponse->au8MIC, 4 );
    DEBUG_PRINTF( "Channel             : %02d\n", psAuthenticateResponse->u8Channel);
    DEBUG_PRINTF( "Short PAN           : 0x%04X\n", psAuthenticateResponse->u16PanID);
    DEBUG_PRINTF( "Extended PAN        : 0x%016llX\n", (unsigned long long int)psAuthenticateResponse->u64PanID);
                        
    char  mac_str[16+2];
    char  tcaddr_str[16+2];
    char  nwkey_str[32+2];
    char  mic_str[8+2];
    char  pan_str[4+2];
    char  extpan_str[16+2];
    hex2nibblestr( nwkey_str, (char *)psAuthenticateResponse->au8NetworkKey, 16 );
    hex2nibblestr( mic_str,   (char *)psAuthenticateResponse->au8MIC,        4 );
    u642nibblestr( psAuthenticateResponse->u64IEEEAddress,        mac_str );
    u642nibblestr( psAuthenticateResponse->u64TrustCenterAddress, tcaddr_str );
    u642nibblestr( psAuthenticateResponse->u64PanID,              extpan_str );
    sprintf( pan_str, "%04X", psAuthenticateResponse->u16PanID );

    dispatchMessage( QUEUE_KEY_SECURE_JOINER, jsonZcbAuthorizeResponse( mac_str, nwkey_str,
                      mic_str, tcaddr_str, psAuthenticateResponse->u8KeySequenceNumber,
                      psAuthenticateResponse->u8Channel, pan_str, extpan_str ) );
}


static void HandleVersionResponse(void *pvUser, uint16_t u16Length, void *pvMessage) {
    static int counter = 0;
    printf( "\n************ HandleVersionResponse\n" );
    // dump( (char *)pvMessage, u16Length );

    struct _VersionResponse {
        uint32_t    u32Version;
    } __attribute__((__packed__)) *psVersionResponse = (struct _VersionResponse *)pvMessage;

    uint32_t version = ntohl(psVersionResponse->u32Version);
    newDbSystemSaveIntval( "zcb_version", version );

    int major = ( version >> 16 ) & 0xFFFF;
    int minor = version & 0xFFFF;
    printf("Connected to control bridge version r%d.%d\n", major, minor );
    
    char buf[20];
    sprintf( buf, "r%d.%d", major, minor );
    // Save version to database
    newDbSystemSaveVal( "version", counter++, buf );
    
    newLogAdd( NEWLOG_FROM_ZCB_OUT, buf );
}

static void HandleGetPermitResponse(void *pvUser, uint16_t u16Length, void *pvMessage) {
    printf( "\n************ HandleGetPermitResponse\n" );
    // dump( (char *)pvMessage, u16Length );

    struct _GetPermitResponse {
        uint8_t    u8Status;
    } __attribute__((__packed__)) *psGetPermitResponse = (struct _GetPermitResponse *)pvMessage;

    newDbSystemSaveIntval( "zcb_permit", psGetPermitResponse->u8Status );
    newLogAdd( NEWLOG_FROM_ZCB_OUT, ( psGetPermitResponse->u8Status  ) ? "Permit open" : "Permit closed" );
}

static void HandleRestart(void *pvUser, uint16_t u16Length, void *pvMessage, int factoryNew) {
    printf( "=====******* ZCB_HandleRestart %d ==============\n", factoryNew );
    const char *pcStatus = NULL;

    struct _tsRestart {
        uint8_t     u8Status;
    } __attribute__((__packed__)) *psRestart = (struct _tsRestart *)pvMessage;

    switch (psRestart->u8Status)
    {
#define STATUS(a, b) case(a): pcStatus = b; break
        STATUS(0, "STARTUP");
        STATUS(1, "WAIT_START");
        STATUS(2, "NFN_START");
        STATUS(3, "DISCOVERY");
        STATUS(4, "NETWORK_INIT");
        STATUS(5, "RESCAN");
        STATUS(6, "RUNNING");
#undef STATUS
        default: pcStatus = "Unknown";
    }
    printf( "Control bridge restarted, status %d (%s)\n", psRestart->u8Status, pcStatus);

    if ( factoryNew ) {
        eSetDeviceType(E_MODE_COORDINATOR);
        writeControl( jsonCmdResetRsp( 0 ) );
    } else {
        writeControl( jsonCmdResetRsp( 1 ) );
    }
    return;
}

static void HandleRestartProvisioned(void *pvUser, uint16_t u16Length, void *pvMessage) {
    HandleRestart( pvUser, u16Length, pvMessage, 0 );
}

static void HandleRestartFactoryNew(void *pvUser, uint16_t u16Length, void *pvMessage) {
    HandleRestart( pvUser, u16Length, pvMessage, 1 );
}

// ------------------------------------------------------------------
// Functions
// ------------------------------------------------------------------

/**
 * \brief Init the JSON Parser for this type of commands
 */

void cmdInit( void ) {
    int i;
    for ( i=0; i<NUMINTATTRS; i++ ) {
        parsingAddIntAttr( intAttrs[i] );
    }
    for ( i=0; i<NUMSTRINGATTRS; i++ ) {
        parsingAddStringAttr( stringAttrs[i], stringMaxlens[i] );
    }

    eSL_AddListener(E_SL_MSG_AUTHENTICATE_DEVICE_RESPONSE, HandleAuthResponse,       NULL);
    eSL_AddListener(E_SL_MSG_VERSION_LIST,                 HandleVersionResponse,    NULL);
    eSL_AddListener(E_SL_MSG_GET_PERMIT_JOIN_RESPONSE,     HandleGetPermitResponse,  NULL);
    eSL_AddListener(E_SL_MSG_RESTART_PROVISIONED,          HandleRestartProvisioned, NULL);
    eSL_AddListener(E_SL_MSG_RESTART_FACTORY_NEW,          HandleRestartFactoryNew,  NULL);
}

#ifdef CMD_DEBUG
static void cmdDump ( void ) {
    printf( "CMD dump:\n" );
    int i;
    for ( i=0; i<NUMINTATTRS; i++ ) {
        printf( "- %-12s = %d\n", intAttrs[i], parsingGetIntAttr( intAttrs[i] ) );
    }
    for ( i=0; i<NUMSTRINGATTRS; i++ ) {
        printf( "- %-12s = %s\n", stringAttrs[i], parsingGetStringAttr( stringAttrs[i] ) );
    }
}
#endif

// ------------------------------------------------------------------
// Handle
// ------------------------------------------------------------------

/**
 * \brief JSON Parser for cmd commands (e.g. reset, set channel). Commands are typically sent to
 * the ZCB-JenOS via the serial port.
 * \retval E_SL_OK When OK, else an error number
 */

int cmdHandle( void ) {
    int ret = E_SL_OK;
#ifdef CMD_DEBUG
    cmdDump();
#endif

    int reset = parsingGetIntAttr( "reset" );
    if ( reset >= 0 ) {
        if ( reset ) {
            printf( "Reset command %d\n", reset );
        } else {
            printf( "Factory-Reset command %d\n", reset );
            ret = eSL_SendMessage(E_SL_MSG_ERASE_PERSISTENT_DATA, 0, NULL, NULL);
            if ( ret == E_SL_NOMESSAGE ) {
                // This is a normal response to the erase command because it takes so long...

                /* The erase persistent data command could take a while */
                uint16_t u16Length;
                tsSL_Msg_Status *psStatus = NULL;
            
                int eStatus = eSL_MessageWait(E_SL_MSG_STATUS, 5000, &u16Length, (void**)&psStatus);
            
                if (eStatus == E_SL_OK) {
                    eStatus = psStatus->eStatus;
                    free(psStatus);
                } else {            
                    return E_ZCB_COMMS_FAILED;
                }

                IOT_SLEEP( 2 );
                ret = E_SL_OK;
            }
        }
        ret += eSL_SendMessage(E_SL_MSG_RESET, 0, NULL, NULL);
        if (ret != E_SL_OK) {
            printf( "**** Error %d resetting ZCB\n", ret );
        }
    }

    int startnwk = parsingGetIntAttr( "startnwk" );
    if ( startnwk >= 0 ) {
        printf( "StartNwk command %d\n", startnwk );
        eStartNetwork();
    }

    int erase = parsingGetIntAttr( "erase" );
    if ( erase >= 0 ) {
        printf( "Erase command %d -> NOT IMPLEMENTED\n", erase );
    }

    int getversion = parsingGetIntAttr( "getversion" );
    if ( getversion >= 0 ) {
        printf( "GetVersion command %d\n", getversion );
        if (eSL_SendMessage(E_SL_MSG_GET_VERSION, 0, NULL, NULL) == E_SL_OK) {
            // Rest handled by callback
        }
    }

    int getpermit = parsingGetIntAttr( "getpermit" );
    if ( getpermit >= 0 ) {
        printf( "GetPermit command %d\n", getpermit );
        eGetPermitJoining();
    }

    // Check for the authorize response
    char * mac    = parsingGetStringAttr( "authorize" );
    if ( !isEmptyString( mac ) ) {
        char * linkkey = parsingGetStringAttr( "linkkey" );
        uint8_t linkkey_hex[20];
        nibblestr2hex( (char *)linkkey_hex, 16, linkkey, 32 );
        printf( "Authorize command %s, %s\n", mac, linkkey );

        uint64_t u64mac = nibblestr2u64( mac );

        printf( "mac: 0x%x%x\n", (int)(u64mac >> 32), (int)(u64mac & 0xFFFFFFFF) );
        printf( "linkkey_hex: " );
        dump( (char *)linkkey_hex, 16 );

        eAuthenticateDevice( u64mac, linkkey_hex );
    }

    // Check for setpermit message
    mac = parsingGetStringAttr( "setpermit" );
    if ( !isEmptyString( mac ) ) {
        // MAC not yet implemeted
        int duration = parsingGetIntAttr( "duration" );
        if ( duration >= 0 ) {
            printf( "SetPermit command %d\n", duration );
            eSetPermitJoining( duration );
        }
    }

    // Check for chanmask message
    char * chanmask = parsingGetStringAttr( "chanmask" );
    if ( !isEmptyString( chanmask ) ) {
        int mask = (int)nibblestr2u64( chanmask );
        int i = 0, r = rand(), bit, chan = 15;
        while ( i < 32 ) {
            bit = ( i + r ) % 32;
            printf( "i = %d, bit = 0x%x\n", i, bit );
            if ( mask & ( 1 << bit ) ) {
                chan = bit;
                break;
            }
            i++;
        }
        printf( "Chanmask command %s -> 0x%x -> %d\n", chanmask, mask, chan );
        eSetChannelMask( chan );   // Was: mask
    }
    
    return( ret );
}

// ------------------------------------------------------------------
// End of file
// ------------------------------------------------------------------

