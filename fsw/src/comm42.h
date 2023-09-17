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

#define COMM42_CREATE_SEM_EID       (COMM42_BASE_EID + 0)
#define COMM42_SOCKET_OPEN_EID      (COMM42_BASE_EID + 1)
#define COMM42_SOCKET_BIND_EID      (COMM42_BASE_EID + 2)
#define COMM42_SOCKET_CLOSE_EID     (COMM42_BASE_EID + 3)
#define COMM42_SKIP_INIT_CYCLE_EID  (COMM42_BASE_EID + 4)
#define COMM42_NO_ACTUATOR_CMD_EID  (COMM42_BASE_EID + 5)
#define COMM42_DEBUG_EID            (COMM42_BASE_EID + 6)


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
   ** Object Data
   */

   uint32  ChildTaskId;
   uint32  WakeUpSemaphore;
   
   bool    InitCycle;
   bool    ActuatorCmdMsgSent;     /* Used for each control cycle */
   uint32  SensorDataMsgCnt;
   uint32  ActuatorCmdMsgCnt;
   uint32  ExecuteCycleCnt;
   uint16  UnclosedCycleCnt;    /* 'Unclosed' is when ManageExecution() called but sesnor-ctrl-actuator cycle didn't finish */ 
   uint16  UnclosedCycleLim;
   
   bool           SocketConnected;
   osal_id_t      SocketId;
   OS_SockAddr_t  SocketAddr;
 
   /*
   ** Contained Objects
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
** Function: COMM42_Close
**
*/
void COMM42_Close(void);


/******************************************************************************
** Function:  COMM42_ConnectSocket
**
*/
bool COMM42_ConnectSocket(const char *AddrStr, uint32 Port);


/******************************************************************************
** Function:  COMM42_ManageExecution
**
*/
void COMM42_ManageExecution(void);


/******************************************************************************
** Function: COMM42_RecvSensorData
**
** Read sensor data from 42 socket and load process sensor data packet.
*/
bool COMM42_RecvSensorData(BC42_INTF_SensorDataMsg_t *SensorDataMsg);


/******************************************************************************
** Function: COMM42_ResetStatus
**
*/
void COMM42_ResetStatus(void);


/******************************************************************************
** Function: COMM42_SendActuatorCmds
**
** Send actuator commandd data to 42.
*/
bool COMM42_SendActuatorCmds(const BC42_INTF_ActuatorCmdMsg_t *ActuatorCmdMsg); 


/******************************************************************************
** Function: COMM42_SocketTask
**
** Notes:
**   1. Returning false causes the child task to terminate.
**
*/
bool COMM42_SocketTask(CHILDMGR_Class_t* ChildMgr);


#endif /* _comm42_ */
