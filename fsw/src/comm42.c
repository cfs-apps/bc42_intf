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
**    1. BC42_LIB's API should be used to access 42 functions. 
**
*/

/*
** Include Files:
*/

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "comm42.h"

/***********************/
/** Macro Definitions **/
/***********************/

#define  CHILDMGR_OBJ  (&(Comm42->ChildMgr))
#define  AC42_(field)  (Comm42->Bc42->AcVar.field) 

/**********************/
/** File Global Data **/
/**********************/

static COMM42_Class_t *Comm42 = NULL;


/*******************************/
/** Local Function Prototypes **/
/*******************************/

static bool ControlLoopTask(CHILDMGR_Class_t *ChildMgr);
static bool ProcessActuatorCmdMsg(const BC42_INTF_ActuatorCmdMsg_t *ActuatorCmdMsg);
static int32 ProcessDataPipe(void);
static bool SendSensorDataMsg(const BC42_Ac_t *Ac42);
static void StopSim(void);

/******************************************************************************
** Function: COMM42_Constructor
**
** Initialize the 42 interface.
**
** Notes:
**   1. This must be called prior to any other function.
*/
void COMM42_Constructor(COMM42_Class_t *Comm42Obj, const INITBL_Class_t *IniTbl)
{

   CFE_SB_Qos_t SbQos;
   
   Comm42 = Comm42Obj;

   // Booleans set to false:  TerminateChildTask, InitCycle, SimActive, ActuatorCmdSentTo42 
   CFE_PSP_MemSet((void*)Comm42, 0, sizeof(COMM42_Class_t));
  
   Comm42->Bc42Port              = INITBL_GetIntConfig(IniTbl, CFG_BC42_SOCKET_PORT);
   Comm42->UnclosedCycleLim      = INITBL_GetIntConfig(IniTbl, CFG_CHILD_UNCLOSED_CYCLE_LIM);
   Comm42->ActuatorCmdMsgTimeout = INITBL_GetIntConfig(IniTbl, CHILD_ACTUATOR_MSG_TIMEOUT);
   Comm42->ActuatorCmdMsgMid     = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(IniTbl, CFG_BC42_INTF_ACTUATOR_CMD_MSG_TOPICID));
       
   /* Child Manager constructor sends error events */
   Comm42->ChildTask.TaskName  = INITBL_GetStrConfig(IniTbl, CFG_CHILD_NAME);
   Comm42->ChildTask.StackSize = INITBL_GetIntConfig(IniTbl, CFG_CHILD_STACK_SIZE);
   Comm42->ChildTask.Priority  = INITBL_GetIntConfig(IniTbl, CFG_CHILD_PRIORITY);
   Comm42->ChildTask.PerfId    = INITBL_GetIntConfig(IniTbl, CFG_CHILD_PERF_ID);

   CHILDMGR_Constructor(CHILDMGR_OBJ, ChildMgr_TaskMainCallback,
                        ControlLoopTask, &Comm42->ChildTask); 

   SbQos.Priority    = 0;
   SbQos.Reliability = 0;
   CFE_SB_CreatePipe(&Comm42->DataPipe, INITBL_GetIntConfig(IniTbl, CFG_CHILD_PIPE_DEPTH), INITBL_GetStrConfig(IniTbl, CFG_CHILD_PIPE_NAME));
   CFE_SB_SubscribeEx(Comm42->ActuatorCmdMsgMid, Comm42->DataPipe, SbQos, INITBL_GetIntConfig(IniTbl, CFG_CHILD_PIPE_ACTUATOR_MSG_LIM));

   CFE_MSG_Init(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader), 
                CFE_SB_ValueToMsgId(INITBL_GetIntConfig(IniTbl, CFG_BC42_INTF_SENSOR_DATA_MSG_TOPICID)),
                sizeof(BC42_INTF_SensorDataMsg_t));
    
   BC42_Constructor();
   
} /* End COMM42_Constructor() */


/******************************************************************************
** Function:  COMM42_ResetStatus
**
** Only counters are reset, boolean state information remains intact. If
** changes are made be sure to check all of the calling scenarios to make
** sure any assumptions aren't violated.
*/
void COMM42_ResetStatus(void)
{

   Comm42->ExecuteCycleCnt   = 0;
   Comm42->SensorDataMsgCnt  = 0;
   Comm42->ActuatorCmdMsgCnt = 0;
   Comm42->UnclosedCycleCnt  = 0;

   CHILDMGR_ResetStatus(CHILDMGR_OBJ);
   
} /* End COMM42_ResetStatus() */


/******************************************************************************
** Function: COMM42_SetCtrlPeriodCmd
**
** TODO: BC42_INTF_ConfigExecuteCmd() ecide whether to adpat to COMM42 pipe pend mod 
*/

bool COMM42_SetCtrlPeriodCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const BC42_INTF_SetCtrlPeriodCmd_Payload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, BC42_INTF_SetCtrlPeriod_t);
   bool RetStatus = true;
   
   CFE_EVS_SendEvent(COMM42_SET_CTRL_PERIOD_EID, CFE_EVS_EventType_INFORMATION,
                     "Control period set to %d (TODO: Implement the command)", Cmd->CtrlPeriod);   
   
      
   return RetStatus;

} /* End COMM42_SetCtrlPeriodCmd() */


/******************************************************************************
** Function: COMM42_Shutdown
**
** Close the socket and force a child task exit. The sequence of this code must
** work with the COMM42_SocketTask() logic to avoid strange. 
** 
*/
void COMM42_Shutdown(void)
{
   
   BC42_StopSim();
   
} /* End COMM42_Shutdown() */


/******************************************************************************
** Function: COMM42_StartSimCmd
**
*/
bool COMM42_StartSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   bool RetStatus = false;
   
   if (!Comm42->SimActive)
   {
      if (BC42_StartSim(Comm42->Bc42Port))
      {
         COMM42_ResetStatus();
         Comm42->SimActive = true;
         Comm42->InitCycle = true;
         CFE_EVS_SendEvent(COMM42_START_SIM_EID, CFE_EVS_EventType_INFORMATION,
                           "Started 42 simulation on port %d", Comm42->Bc42Port);
         RetStatus = true;
      }
      else
      {
         CFE_EVS_SendEvent(COMM42_START_SIM_EID, CFE_EVS_EventType_ERROR,
                           "Failed to start 42 simulation on port %d. Verify 42 has been started in standlaone mode.", Comm42->Bc42Port);
      }
   } /* End if !SimActive */
   else
   {
      CFE_EVS_SendEvent(COMM42_START_SIM_EID, CFE_EVS_EventType_ERROR,
                        "Start Sim command rejected, simulation in progress.");      
   }   

   return RetStatus;

} /* End COMM42_StartSimCmd() */


/******************************************************************************
** Function: COMM42_StopSimCmd
**
** Notes:
**   1. Must match CMDMGR_CmdFuncPtr function signature
*/
bool COMM42_StopSimCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   StopSim();

   CFE_EVS_SendEvent(COMM42_STOP_SIM_EID, CFE_EVS_EventType_INFORMATION,
                     "Stopped 42 simulation");
   
   return true;
   
} /* End COMM42_StopSimCmd() */


/******************************************************************************
** Function: ControlLoopTask
**
** Notes:
**   1. This function is continuously called by the app_c_fw's ChildMgr
**      service. Returning false causes the child task to terminate.
**   2. This infinite loop design proved to be the most robust. I tried to 
**      create/terminate the child task with a start/stop sim and
**      something didn't seem to get cleaned up properly and the system would
**      hang on a second start sim command. 
**
*/
static bool ControlLoopTask(CHILDMGR_Class_t *ChildMgr)
{
   
   bool  RetStatus = true;
   const BC42_Ac_t *Ac42;
   
   if (Comm42->TerminateChildTask)
   {
      RetStatus = false;
      CFE_EVS_SendEvent(COMM42_CONTROL_LOOP_TASK_ERR_EID, CFE_EVS_EventType_ERROR,
                        "Terminating BC42 Interface App's COMM42 child task");  
   }
   else
   {
      if (Comm42->SimActive)
      {
         
         if (BC42_ReadSensorData(&Ac42))
         {
            Comm42->InitCycle = false;
            SendSensorDataMsg(Ac42);
            
            // Pend for actuator command message */ 
            if (ProcessDataPipe() == CFE_SUCCESS)
            {
               Comm42->ExecuteCycleCnt++;
               OS_TaskDelay(250); // TODO: Use a delay that corresponds to 42's FSW Sample Time
            }
            else
            {
               Comm42->UnclosedCycleCnt++;
               if (Comm42->UnclosedCycleCnt > Comm42->UnclosedCycleLim)
               {
                  StopSim();        
                  CFE_EVS_SendEvent(COMM42_CONTROL_LOOP_TASK_ERR_EID, CFE_EVS_EventType_ERROR,
                                    "Failed to receive actuator command messages in %d control cycles",
                                    Comm42->UnclosedCycleCnt);
               }
               else
               {
                  Comm42->UnclosedCycleCnt = 0;
               }
            }
         }
         else
         {
            StopSim();
            RetStatus = false;
            CFE_EVS_SendEvent(COMM42_CONTROL_LOOP_TASK_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Error reading 42 sensor data");  
         }
            
      } /* End if ChildTaskEnabled */
      else
      {
         OS_TaskDelay(Comm42->ActuatorCmdMsgTimeout);
         
      } /* End if child task disabled */
   } /* End if not terminating task */
   
   return RetStatus;
   
} /* End ControlLoopTask() */


/******************************************************************************
** Function: ProcessDataPipe
**
** TODO: Should this function do anything for non success?
*/
static int32 ProcessDataPipe(void)
{

   int32  SbStatus;
   int32  MsgStatus;
   
   CFE_SB_Buffer_t  *SbBufPtr;
   CFE_SB_MsgId_t   MsgId = CFE_SB_INVALID_MSG_ID;
      
   CFE_ES_PerfLogExit(Comm42->ChildTask.PerfId);
   SbStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, Comm42->DataPipe, Comm42->ActuatorCmdMsgTimeout);
   CFE_ES_PerfLogEntry(Comm42->ChildTask.PerfId);
 
   if (SbStatus == CFE_SUCCESS)
   {
      
      MsgStatus = CFE_MSG_GetMsgId(&SbBufPtr->Msg, &MsgId);
   
      if (MsgStatus == CFE_SUCCESS)
      {
         if (CFE_SB_MsgId_Equal(MsgId, Comm42->ActuatorCmdMsgMid))
         {
            ProcessActuatorCmdMsg((BC42_INTF_ActuatorCmdMsg_t *)&SbBufPtr->Msg);
         } 
         else
         {            
            CFE_EVS_SendEvent(COMM42_DATA_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
                              "BC42_INTF's COMM42 child task received invalid data packet, MID = 0x%04X(%d)", 
                              CFE_SB_MsgIdToValue(MsgId), CFE_SB_MsgIdToValue(MsgId));
         }
     
      } /* End valid message ID */
   } /* End if SB received a packet */
   
   return SbStatus;
   
} /* End ProcessDataPipe() */


/******************************************************************************
** Function: ProcessActuatorCmdMsg
**
*/
static bool ProcessActuatorCmdMsg(const BC42_INTF_ActuatorCmdMsg_t *ActuatorCmdMsg)
{

   bool  RetStatus = false;
   const BC42_INTF_ActuatorCmdMsg_Payload_t *ActuatorCmd = &ActuatorCmdMsg->Payload;
    
   if (BC42_WriteActuatorData(ActuatorCmd->Tcmd, ActuatorCmd->Mcmd, ActuatorCmd->SaGcmd))
   {
      Comm42->ActuatorCmdMsgCnt++;
      Comm42->ActuatorCmdSentTo42 = true;
      RetStatus = true;
   }
   else
   {
      StopSim();
      RetStatus = false;
      CFE_EVS_SendEvent(COMM42_WRITE_ACTUATOR_DATA_ERR_EID, CFE_EVS_EventType_ERROR,
                        "Error writing 42 actuator data. 42 simulation time may have expired.");        
   }

   return RetStatus;
   
} /* End ProcessActuatorCmdMsg() */


/******************************************************************************
** Function: SendSensorDataMsg
**
*/
static bool SendSensorDataMsg(const BC42_Ac_t *Ac42)
{
   
   int   i;
   int32 CfeStatus;
   bool  RetStatus = false;
   
   BC42_INTF_SensorDataMsg_Payload_t *SensorData = &Comm42->SensorDataMsg.Payload;
     
   SensorData->GpsTime   = Ac42->Time; 
   SensorData->GpsValid  = Ac42->GPS->Valid;
   SensorData->StValid   = Ac42->StValid;
   SensorData->SunValid  = Ac42->SunValid;
   SensorData->InitCycle = Comm42->InitCycle;

   for (i=0; i < 3; i++) 
   {
   
      SensorData->PosN[i] = Ac42->GPS->PosN[i];  /* GPS */
      SensorData->VelN[i] = Ac42->GPS->VelN[i];
   
      SensorData->qbn[i]  = Ac42->qbn[i];   /* ST */

      SensorData->wbn[i]  = Ac42->wbn[i];   /* Gyro */
   
      SensorData->svb[i]  = Ac42->svb[i];   /* CSS/FSS */

      SensorData->bvb[i]  = Ac42->bvb[i];   /* MTB */
   
      SensorData->WhlH[i] = Ac42->Whl[i].H; /* Wheels */
   }

   SensorData->qbn[3]  = Ac42->qbn[3];
   SensorData->WhlH[3] = Ac42->Whl[3].H;
      
   CFE_SB_TimeStampMsg(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader));
   CfeStatus = CFE_SB_TransmitMsg(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader), true);
   
   if (CfeStatus == CFE_SUCCESS)
   {
      RetStatus = true;  
      Comm42->SensorDataMsgCnt++;
      Comm42->ActuatorCmdSentTo42 = false;
   }

   return RetStatus;

} /* SendSensorDataMsg() */


/******************************************************************************
** Function: StopSim
**
*/
static void StopSim(void)
{
   
   Comm42->SimActive = false;
   BC42_StopSim();
   COMM42_ResetStatus();

   
} /* StopSim() */
