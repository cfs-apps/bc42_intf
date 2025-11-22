/*
**  Copyright 2022 bitValence, Inc.
**  All Rights Reserved.
**
**  This program is free software; you can modify and/or redistribute it
**  under the terms of the GNU Affero General Public License
**  as published by the Free Software Foundation; version 3 with
**  attribution addendums as found in the LICENSE.txt
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Affero General Public License for more details.
**
**  Purpose:
**    Define the 42 FSW interface application
**
**  Notes:
**    1. This is part of prototype effort to port a 42 simulator FSW controller
**       component into a cFS-based application. See bc42_lib.h for more
**       information.
**
*/

#ifndef _bc42_intf_app_
#define _bc42_intf_app_

/*
** Includes
*/

#include "app_cfg.h"
#include "comm42.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define BC42_INTF_IP_ADDR_STR_LEN  16

/*
** Events Message IDs
*/

#define BC42_INTF_INIT_APP_EID         (BC42_INTF_BASE_EID + 0)
#define BC42_INTF_EXIT_EID             (BC42_INTF_BASE_EID + 1)
#define BC42_INTF_EXECUTE_CMD_EID      (BC42_INTF_BASE_EID + 2)
#define BC42_INTF_NOOP_EID             (BC42_INTF_BASE_EID + 3)
#define BC42_INTF_PROCESS_CMD_PIPE_EID (BC42_INTF_BASE_EID + 4)
#define BC42_INTF_DEBUG_EID            (BC42_INTF_BASE_EID + 5)


/**********************/
/** Type Definitions **/
/**********************/


/******************************************************************************
** Command Packets
** - See EDS command definitions in bc42_intf.xml
*/


/******************************************************************************
** Telemetry Packets
** - See EDS command definitions in bc42_intf.xml
*/


/******************************************************************************
** BC42_INTF_Class
*/

typedef struct
{

   /*
   ** App Framework
   */

   INITBL_Class_t      IniTbl; 
   CFE_SB_PipeId_t     CmdPipe;
   CMDMGR_Class_t      CmdMgr;

   /*
   ** App State
   */ 
   
   uint32          PerfId;
   CFE_SB_MsgId_t  CmdMid;
   CFE_SB_MsgId_t  StatusMid;
 
   /*
   ** Telemetry Packets
   */
   BC42_INTF_StatusTlm_t  StatusTlm;
   
   /*
   ** App Objects
   */ 
   COMM42_Class_t  Comm42;

} BC42_INTF_APP_Class_t;


/*******************/
/** Exported Data **/
/*******************/


extern BC42_INTF_APP_Class_t  Bc42Intf;


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: BC42_INTF_AppMain
**
*/
void BC42_INTF_AppMain(void);


/******************************************************************************
** Function: BC42_INTF_StartSimCmd
**
** Notes:
**   1. Must match CMDMGR_CmdFuncPtr function signature
*/
bool BC42_INTF_StartSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: BC42_INTF_StopSimCmd
**
** Notes:
**   1. Must match CMDMGR_CmdFuncPtr function signature
*/
bool BC42_INTF_StopSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: BC42_INTF_NoOpCmd
**
*/
bool BC42_INTF_NoOpCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: BC42_INTF_ResetAppCmd
**
*/
bool BC42_INTF_ResetAppCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


#endif /* _bc42_intf_app_ */
