/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/*
 *  Manage the listen-mode routing table.
 */
#include <malloc.h>
#include <string.h>
#include "RoutingManager.h"
#include "nativeNfcManager.h"

extern "C"
{
    #include "phNxpConfig.h"
    #include "phNxpLog.h"
    #include "nfc_api.h"
    #include "nfa_api.h"
    #include "nfa_ce_api.h"
    #include "nci_config.h"
}

#define MAX_CE_RX_BUFFER_SIZE       1024

static unsigned char T4T_CHECK_NDEF_APDU[] = {
        0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00
    };

#define T4T_CHECK_NDEF_APDU_LENGTH      13

extern Mutex gSyncMutex;
extern void checkforTranscation(UINT8 connEvent, void* eventData);

RoutingManager::RoutingManager ()
: mRxDataBufferLen(0),
  mActiveSe(ROUTE_HOST),
  mSeTechMask(0x0),
  mDefaultEe(ROUTE_HOST),
  mHostListnEnable (true),
  mFwdFuntnEnable (true),
  mCallback(NULL)
{
    NXPLOG_API_D("%s: default route is 0x%02X\n",
                 "RoutingManager::RoutingManager()", mDefaultEe);
    mRxDataBuffer = (UINT8*)malloc(MAX_CE_RX_BUFFER_SIZE * sizeof(UINT8));
    memset(mRxDataBuffer, 0, MAX_CE_RX_BUFFER_SIZE);
    mRxDataBufferLen = 0;
}

RoutingManager::~RoutingManager ()
{
    NXPLOG_API_D ("%s:~RoutingManager()", __FUNCTION__);
    NFA_EeDeregister (nfaEeCallback);
    mCallback = NULL;
    free(mRxDataBuffer);
}

bool RoutingManager::initialize ()
{
    unsigned long tech = 0;
    UINT8 mActualNumEe = 0;
    tNFA_EE_INFO mEeInfo [mActualNumEe];

    if ((GetNumValue(NAME_NXP_FWD_FUNCTIONALITY_ENABLE, &tech, sizeof(tech))))
    {
        mFwdFuntnEnable = tech;
        NXPLOG_API_E ("%s:NXP_FWD_FUNCTIONALITY_ENABLE=%d;", __FUNCTION__, mFwdFuntnEnable);
    }

    tNFA_STATUS nfaStat;
    {
        SyncEventGuard guard (mEeRegisterEvent);
        NXPLOG_API_D ("%s: try ee register", "RoutingManager::initialize()");
        nfaStat = NFA_EeRegister (nfaEeCallback);
        if (nfaStat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("%s: fail ee register; error=0x%X",
                          "RoutingManager::initialize()", nfaStat);
            return false;
        }
        mEeRegisterEvent.wait ();
    }
    if(mHostListnEnable)
    {
        // Tell the host-routing to only listen on Nfc-A/Nfc-B
        nfaStat = NFA_CeRegisterAidOnDH (NULL, 0, stackCallback);
        if (nfaStat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("Failed to register wildcard AID for DH");
        }
        // Tell the host-routing to only listen on Nfc-A/Nfc-B
        nfaStat = NFA_CeSetIsoDepListenTech(NFA_TECHNOLOGY_MASK_A | NFA_TECHNOLOGY_MASK_B);
        if (nfaStat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("Failed to configure CE IsoDep technologies");
        }
    }
    memset(mRxDataBuffer, 0, MAX_CE_RX_BUFFER_SIZE);

    if ((nfaStat = NFA_AllEeGetInfo (&mActualNumEe, mEeInfo)) != NFA_STATUS_OK)
    {
        NXPLOG_API_E ("%s: fail get info; error=0x%X",
                      "RoutingManager::initialize()", nfaStat);
        mActualNumEe = 0;
    }
    return true;
}

void RoutingManager::finalize()
{
    NXPLOG_API_D ("%s:RoutingManager::finalize()", __FUNCTION__);
    NFA_EeDeregister (nfaEeCallback);
    //NFA_CeDeregisterAidOnDH(mHostHandle);
    mCallback = NULL;
    mRxDataBufferLen = 0;
}

RoutingManager& RoutingManager::getInstance ()
{
    static RoutingManager manager;
    return manager;
}

void RoutingManager::enableRoutingToHost()
{
    tNFA_STATUS nfaStat;
    NXPLOG_API_D ("%s enter", "RoutingManager::enableRoutingToHost()");
    {
        SyncEventGuard guard (mRoutingEvent);

        // Route Nfc-A & B to host
        if (mSeTechMask == 0)
        {
            nfaStat = NFA_EeSetDefaultProtoRouting(mDefaultEe ,
                                        NFC_PROTOCOL_MASK_ISO7816|NFA_PROTOCOL_MASK_ISO_DEP,
                                        0, 0, 0, 0);
            if (nfaStat == NFA_STATUS_OK)
            {
                mRoutingEvent.wait ();
            }
            else
            {
                NXPLOG_API_E ("Fail to set  iso7816 routing");
            }
        }
    }
    commitRouting();
}

void RoutingManager::disableRoutingToHost()
{
    tNFA_STATUS nfaStat;
    NXPLOG_API_D ("%s enter", "RoutingManager::disableRoutingToHost()");
    {
        SyncEventGuard guard (mRoutingEvent);
        // Default routing for NFC-A & B technology if we don't have a SE
        if (mSeTechMask == 0)
        {
            nfaStat = NFA_EeSetDefaultProtoRouting(mDefaultEe, 0, 0, 0, 0, 0);
            if (nfaStat == NFA_STATUS_OK)
            {
                mRoutingEvent.wait ();
            }
            else
            {
                NXPLOG_API_E ("Fail to set  iso7816 routing");
            }
        }
    }
    commitRouting();
}

bool RoutingManager::commitRouting()
{
    tNFA_STATUS nfaStat = NFA_EeUpdateNow();
    return (nfaStat == NFA_STATUS_OK);
}

void RoutingManager::registerHostCallback(nfcHostCardEmulationCallback_t *callback)
{
    mCallback = callback;
}

void RoutingManager::deregisterHostCallback()
{
    mCallback = NULL;
}

void RoutingManager::notifyHceActivated()
{
    if (nativeNfcManager_isNfcActive())
    {
        if (mCallback)
        {
            mCallback->onHostCardEmulationActivated();
        }
    }
}

void RoutingManager::notifyHceDeactivated()
{
    if (nativeNfcManager_isNfcActive())
    {
        if (mCallback)
        {
            mCallback->onHostCardEmulationDeactivated();
        }
    }
}

void RoutingManager::handleData (const UINT8* data, UINT32 dataLen, tNFA_STATUS status)
{
    tNFA_STATUS nfaStat = NFA_STATUS_OK;

    if (dataLen <= 0)
    {
        NXPLOG_API_E("no data");
        goto TheEnd;
    }

    if (status == NFA_STATUS_CONTINUE)
    {
        memcpy((mRxDataBuffer + mRxDataBufferLen), data, dataLen);
        mRxDataBufferLen += dataLen;
        return; //expect another NFA_CE_DATA_EVT to come
    }
    else if (status == NFA_STATUS_OK)
    {
        memcpy(mRxDataBuffer, data, dataLen);
        mRxDataBufferLen = dataLen;
        //entire data packet has been received; no more NFA_CE_DATA_EVT
    }
    else if (status == NFA_STATUS_FAILED)
    {
        NXPLOG_API_E("RoutingManager::handleData: read data fail");
        goto TheEnd;
    }
    if (mRxDataBufferLen == T4T_CHECK_NDEF_APDU_LENGTH && memcmp(mRxDataBuffer, T4T_CHECK_NDEF_APDU, T4T_CHECK_NDEF_APDU_LENGTH) == 0)
    {
        //ignore check Ndef command, interop with PN544
        nfaStat = NFA_Deactivate (FALSE);
        if (nfaStat != NFA_STATUS_OK)
        {
            NXPLOG_API_E ("RoutingManager::handleData: deactivate failed; error=0x%X", nfaStat);
        }
        goto TheEnd;
    }
    if (nativeNfcManager_isNfcActive())
    {
        if (mCallback)
        {
            mCallback->onDataReceived(mRxDataBuffer, mRxDataBufferLen);
        }
    }
TheEnd:
    memset(mRxDataBuffer, 0, MAX_CE_RX_BUFFER_SIZE);
    mRxDataBufferLen = 0;
}

void RoutingManager::stackCallback (UINT8 event, tNFA_CONN_EVT_DATA* eventData)
{
    NXPLOG_API_D("%s: event=0x%X", "RoutingManager::stackCallback", event);

    switch (event)
    {
    case NFA_CE_REGISTERED_EVT:
        {
            NXPLOG_API_D("%s: NFA_CE_REGISTERED_EVT; status=0x%X; h=0x%X",
                         "RoutingManager::stackCallback",
                         eventData->ce_registered.status,
                         eventData->ce_registered.handle);
        }
        break;

    case NFA_CE_DEREGISTERED_EVT:
        {
            NXPLOG_API_D("%s: NFA_CE_DEREGISTERED_EVT; h=0x%X",
                         "RoutingManager::stackCallback",
                         eventData->ce_deregistered.handle);
        }
        break;

    case NFA_CE_ACTIVATED_EVT:
        {
            checkforTranscation(NFA_CE_ACTIVATED_EVT, (void *)eventData);
            getInstance().notifyHceActivated();
        }
        break;
    case NFA_DEACTIVATED_EVT:
    case NFA_CE_DEACTIVATED_EVT:
        {
            checkforTranscation(NFA_CE_DEACTIVATED_EVT, (void *)eventData);
            getInstance().notifyHceDeactivated();
        }
        break;
    case NFA_CE_DATA_EVT:
        {
            tNFA_CE_DATA& ce_data = eventData->ce_data;
            NXPLOG_API_D("%s: NFA_CE_DATA_EVT; stat=0x%X; h=0x%X; data len=%u",
                         "RoutingManager::stackCallback", ce_data.status,
                         ce_data.handle, ce_data.len);
            getInstance().handleData(ce_data.p_data, ce_data.len, ce_data.status);
        }
        break;
    }
}

/*******************************************************************************
**
** Function:        nfaEeCallback
**
** Description:     Receive execution environment-related events from stack.
**                  event: Event code.
**                  eventData: Event data.
**
** Returns:         None
**
*******************************************************************************/
void RoutingManager::nfaEeCallback (tNFA_EE_EVT event, tNFA_EE_CBACK_DATA* eventData)
{
    RoutingManager& routingManager = RoutingManager::getInstance();

    switch (event)
    {
        case NFA_EE_REGISTER_EVT:
        {
            SyncEventGuard guard (routingManager.mEeRegisterEvent);
            NXPLOG_API_D ("%s: NFA_EE_REGISTER_EVT; status=%u",
                          "RoutingManager::nfaEeCallback",
                          eventData->ee_register);
            routingManager.mEeRegisterEvent.notifyOne();
        }
        break;

        case NFA_EE_MODE_SET_EVT:
        {
            SyncEventGuard guard (routingManager.mEeSetModeEvent);
            NXPLOG_API_D ("%s: NFA_EE_MODE_SET_EVT; status: 0x%04X  handle: 0x%04X ",
                          "RoutingManager::nfaEeCallback",
                          eventData->mode_set.status,
                          eventData->mode_set.ee_handle);
            routingManager.mEeSetModeEvent.notifyOne();
            //se.notifyModeSet(eventData->mode_set.ee_handle, !(eventData->mode_set.status),eventData->mode_set.ee_status );
        }
        break;

        case NFA_EE_SET_TECH_CFG_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_SET_TECH_CFG_EVT; status=0x%X",
                          "RoutingManager::nfaEeCallback", eventData->status);
            SyncEventGuard guard(routingManager.mRoutingEvent);
            routingManager.mRoutingEvent.notifyOne();
        }
        break;

        case NFA_EE_SET_PROTO_CFG_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_SET_PROTO_CFG_EVT; status=0x%X",
                          "RoutingManager::nfaEeCallback", eventData->status);
            SyncEventGuard guard(routingManager.mRoutingEvent);
            routingManager.mRoutingEvent.notifyOne();
        }
        break;

        case NFA_EE_ACTION_EVT:
        {
            tNFA_EE_ACTION& action = eventData->action;
            checkforTranscation(NFA_EE_ACTION_EVT, (void *)eventData);
            if (action.trigger == NFC_EE_TRIG_SELECT)
            {
                NXPLOG_API_D ("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=select (0x%X)",
                              "RoutingManager::nfaEeCallback", action.ee_handle,
                              action.trigger);
            }
            else if (action.trigger == NFC_EE_TRIG_APP_INIT)
            {
                tNFC_APP_INIT& app_init = action.param.app_init;
                NXPLOG_API_D ("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=app-init (0x%X); aid len=%u; data len=%u",
                              "RoutingManager::nfaEeCallback", action.ee_handle,
                              action.trigger, app_init.len_aid,
                              app_init.len_data);
                //if app-init operation is successful;
                //app_init.data[] contains two bytes, which are the status codes of the event;
                //app_init.data[] does not contain an APDU response;
                //see EMV Contactless Specification for Payment Systems; Book B; Entry Point Specification;
                //version 2.1; March 2011; section 3.3.3.5;
                if ( (app_init.len_data > 1) &&
                     (app_init.data[0] == 0x90) &&
                     (app_init.data[1] == 0x00) )
                {
                    //getInstance().notifyAidSelected(app_init.aid, app_init.len_aid, app_init.data, app_init.len_data);
                }
            }
            else if (action.trigger == NFC_EE_TRIG_RF_PROTOCOL)
            {
                NXPLOG_API_D ("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=rf protocol (0x%X)",
                              "RoutingManager::nfaEeCallback", action.ee_handle,
                              action.trigger);
            }
            else if (action.trigger == NFC_EE_TRIG_RF_TECHNOLOGY)
            {
                NXPLOG_API_D ("%s: NFA_EE_ACTION_EVT; h=0x%X; trigger=rf tech (0x%X)",
                              "RoutingManager::nfaEeCallback", action.ee_handle,
                              action.trigger);
            }
            else
            {
                NXPLOG_API_E ("%s: NFA_EE_ACTION_EVT; h=0x%X; unknown trigger (0x%X)",
                              "RoutingManager::nfaEeCallback", action.ee_handle,
                              action.trigger);
            }
        }
        break;

        case NFA_EE_DISCOVER_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_DISCOVER_EVT; status=0x%X; num ee=%u",
                          __FUNCTION__,eventData->status,
                          eventData->ee_discover.num_ee);
        }
        break;

        case NFA_EE_DISCOVER_REQ_EVT:
            NXPLOG_API_D ("%s: NFA_EE_DISCOVER_REQ_EVT; status=0x%X; num ee=%u", __FUNCTION__,
                eventData->discover_req.status, eventData->discover_req.num_ee);
        break;

        case NFA_EE_NO_CB_ERR_EVT:
            NXPLOG_API_D ("%s: NFA_EE_NO_CB_ERR_EVT  status=%u",
                      "RoutingManager::nfaEeCallback", eventData->status);
        break;

        case NFA_EE_ADD_AID_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_ADD_AID_EVT  status=%u",
                          "RoutingManager::nfaEeCallback", eventData->status);
        }
        break;

        case NFA_EE_REMOVE_AID_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_REMOVE_AID_EVT  status=%u",
                          "RoutingManager::nfaEeCallback", eventData->status);
        }
        break;

        case NFA_EE_NEW_EE_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_NEW_EE_EVT  h=0x%X; status=%u",
                          "RoutingManager::nfaEeCallback",
                          eventData->new_ee.ee_handle,
                          eventData->new_ee.ee_status);
        }
        break;
        case NFA_EE_ROUT_ERR_EVT:
        {
            NXPLOG_API_D ("%s: NFA_EE_ROUT_ERR_EVT  status=%u",
                          "RoutingManager::nfaEeCallback", eventData->status);
        }
        break;
    default:
        NXPLOG_API_E ("%s: unknown event=%u ????",
                      "RoutingManager::nfaEeCallback", event);
        break;
    }
}

