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
**    Provide a socket interface to 42 simulator
**
**  Notes:
**    1. This is part of prototype effort to port a 42 simulator FSW controller
**       component into a cFS-based application.
**    2. This object wraps all of the 42 interface details and all BC42_INTF_APP
**       42 communications should be through this interface. See bc42_lib.h for
**       more 42 AcApp porting information.
**    3. In a more realistic design there would be different interfaces for 
**       sensors and actuators so BC42_INTF_APP's functionality may be 
**       distributed. Even with multiple interface apps, having a single app to
**       aggregate and synchronize sensor data is often helpful.
**    TODO: Consider a pause sim command
**
*/
#ifndef _comm42_
#define _comm42_

/*
** Includes
*/

#include "app_cfg.h"
#include "bc42.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define COMM42_SEM_INVALID      0xFFFFFFFF

/*
** Event Message IDs
*/

#define COMM42_CREATE_SEM_EID              (COMM42_BASE_EID + 0)
#define COMM42_START_SIM_EID               (COMM42_BASE_EID + 1)
#define COMM42_STOP_SIM_EID                (COMM42_BASE_EID + 2)
#define COMM42_DATA_PIPE_ERR_EID           (COMM42_BASE_EID + 3)
#define COMM42_CONTROL_LOOP_TASK_ERR_EID   (COMM42_BASE_EID + 4)
#define COMM42_SET_CTRL_PERIOD_EID         (COMM42_BASE_EID + 5)
#define COMM42_WRITE_ACTUATOR_DATA_ERR_EID (COMM42_BASE_EID + 6)

/**********************/
/** Type Definitions **/
/**********************/

/******************************************************************************
** Commands
** - See EDS command definitions in bc42_intf.xml
*/


/******************************************************************************
** Telemetry
** - See EDS telemetry definitions in bc42_intf.xml
*/


/******************************************************************************
** COMM42 Class
*/
typedef struct
{

   /*
   ** App Framework
   */
   
   CFE_SB_PipeId_t     DataPipe;
   CHILDMGR_Class_t    ChildMgr;
   CHILDMGR_TaskInit_t ChildTask;
   
   /*
   ** Object Data
   */

   bool    TerminateChildTask;
      
   bool    InitCycle;
   bool    SimActive;
   bool    ActuatorCmdSentTo42;  /* Set for each control cycle */
   uint32  SensorDataMsgCnt;
   uint32  ActuatorCmdMsgCnt;
   uint32  ExecuteCycleCnt;
   uint16  UnclosedCycleCnt;     /* 'Unclosed' cycle is when a SB pend TIMEOUT occurs for an actuator messsage */ 
   uint16  UnclosedCycleLim;     /* Current sim is terminated when this limit is reached */

   uint32  ActuatorCmdMsgTimeout;
   CFE_SB_MsgId_t  ActuatorCmdMsgMid;
    
   uint16  Bc42Port;
      
   /*
   ** Referenced Objects
   */
   
   BC42_Class_t *Bc42;
   
   /*
   ** Telemetry
   */
   
   BC42_INTF_SensorDataMsg_t SensorDataMsg;

   
} COMM42_Class_t;


/************************/
/** Exported Functions **/
/************************/


/******************************************************************************
** Function: COMM42_Constructor
**
** Notes:
**   1. This must be called prior to any other function.
**
*/
void COMM42_Constructor(COMM42_Class_t *Bc42Intf, const INITBL_Class_t *IniTbl);


/******************************************************************************
** Function: COMM42_ResetStatus
**
*/
void COMM42_ResetStatus(void);


/******************************************************************************
** Function: COMM42_SetCtrlPeriodCmd
**
*/
bool COMM42_SetCtrlPeriodCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: COMM42_Shutdown
**
** Close the socket and force a child task exit.
** 
*/
void COMM42_Shutdown(void);


/******************************************************************************
** Function:  COMM42_StartSim
**
*/
bool COMM42_StartSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


/******************************************************************************
** Function: COMM42_StopSimCmd
**
** Close the socket and force a child task exit.
*/
bool COMM42_StopSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr);


#endif /* _comm42_ */
