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
**    Implement the 42 FSW Interface application
**
**  Notes:
**    1. This is part of prototype effort to port a 42 simulator FSW controller
**       component into a cFS-based application 
**    2. A single input pipe is used for all data and control because the 42
**       socket interface provides all sensor data in a single read and it keeps
**       the design simple. A flight data ingest app would typically be more
**       complex.
**
*/

/*
** Includes
*/

#include <string.h>
#include "bc42_intf_app.h"
#include "bc42_intf_eds_cc.h"

/***********************/
/** Macro Definitions **/
/***********************/

/* Convenience macros */
#define  INITBL_OBJ   (&(Bc42Intf.IniTbl))
#define  CMDMGR_OBJ   (&(Bc42Intf.CmdMgr))
#define  CHILDMGR_OBJ (&(Bc42Intf.ChildMgr))
#define  COMM42_OBJ   (&(Bc42Intf.Comm42))

/*******************************/
/** Local Function Prototypes **/
/*******************************/

static int32 InitApp(void);
static int32 ProcessCmdPipe(void);
static void AppTermCallback(void);

static void SendHousekeepingPkt(void);

/**********************/
/** File Global Data **/
/**********************/

/* 
** Must match DECLARE ENUM() declaration in app_cfg.h
** Defines "static INILIB_CfgEnum IniCfgEnum"
*/
DEFINE_ENUM(Config,APP_CONFIG)  

static CFE_EVS_BinFilter_t  EventFilters[] =
{  
   /* Event ID                           Mask */
   {COMM42_DEBUG_EID,    CFE_EVS_FIRST_64_STOP}, //CFE_EVS_NO_FILTER
   {BC42_INTF_DEBUG_EID, CFE_EVS_FIRST_64_STOP}
};

/*****************/
/** Global Data **/
/*****************/

BC42_INTF_APP_Class_t   Bc42Intf;


/******************************************************************************
** Function: BC42_INTF_AppMain
**
*/
void BC42_INTF_AppMain(void)
{

   uint32 RunStatus = CFE_ES_RunStatus_APP_ERROR;

   CFE_EVS_Register(EventFilters, sizeof(EventFilters)/sizeof(CFE_EVS_BinFilter_t),
                    CFE_EVS_EventFilter_BINARY);

   if (InitApp() == CFE_SUCCESS)      /* Performs initial CFE_ES_PerfLogEntry() call */
   {
      RunStatus = CFE_ES_RunStatus_APP_RUN; 
   }

   /*
   ** Main process loop
   */
   while (CFE_ES_RunLoop(&RunStatus)) 
   {

      RunStatus = ProcessCmdPipe();
      
   } /* End CFE_ES_RunLoop */


   /* Write to system log in case events not working */

   CFE_ES_WriteToSysLog("Bascamp 42 Interface App terminating, run status = 0x%08X\n", RunStatus);   /* Use SysLog, events may not be working */

   CFE_EVS_SendEvent(BC42_INTF_EXIT_EID, CFE_EVS_EventType_CRITICAL, "Bascamp 42 Interface App terminating, run status = 0x%08X", RunStatus);

   CFE_ES_ExitApp(RunStatus);  /* Let cFE kill the task (and any child tasks) */

} /* End of BC42_INTF_AppMain() */


/******************************************************************************
** Function: BC42_INTF_ConfigExecuteCmd
**
*/

bool BC42_INTF_ConfigExecuteCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{
   
   const BC42_INTF_ConfigExecuteCmd_Payload_t *Cmd = CMDMGR_PAYLOAD_PTR(MsgPtr, BC42_INTF_ConfigExecuteCmd_t);
   bool RetStatus = false;
   
   uint16  SavedExecuteMsgCycles  = Bc42Intf.ExecuteMsgCycles;
   uint16  SavedExecuteCycleDelay = Bc42Intf.ExecuteCycleDelay;
  
   if ((Cmd->MsgCycles >= Bc42Intf.ExecuteMsgCycleMin) &&
       (Cmd->MsgCycles <= Bc42Intf.ExecuteMsgCycleMax))
   {
      
      if ((Cmd->CycleDelay >= Bc42Intf.ExecuteCycleDelayMin) &&
          (Cmd->CycleDelay <= Bc42Intf.ExecuteCycleDelayMax))
      {
      
         Bc42Intf.ExecuteMsgCycles  = Cmd->MsgCycles;
         Bc42Intf.ExecuteCycleDelay = Cmd->CycleDelay;

         RetStatus = true;
         CFE_EVS_SendEvent(BC42_INTF_EXECUTE_CMD_EID, CFE_EVS_EventType_INFORMATION,
                           "Execution cycle changed from %d to %d and cycle delay changed from %d to %d",
                           SavedExecuteMsgCycles, Bc42Intf.ExecuteMsgCycles, SavedExecuteCycleDelay, Bc42Intf.ExecuteCycleDelay);
      
      } /* End if valid delay */
      else
      {
      
         CFE_EVS_SendEvent(BC42_INTF_EXECUTE_CMD_EID, CFE_EVS_EventType_ERROR,
                           "Configure execute command rejected. Invalid cycle delay %d, must be in range %d..%d",
                           Cmd->CycleDelay, Bc42Intf.ExecuteCycleDelayMin, Bc42Intf.ExecuteCycleDelayMax);
      }

   }  /* End if valid cycles */
   else
   {
   
      CFE_EVS_SendEvent(BC42_INTF_EXECUTE_CMD_EID, CFE_EVS_EventType_ERROR,
                        "Configure execute command rejected. Invalid cycles %d, must be in range %d..%d",
                        Cmd->MsgCycles, Bc42Intf.ExecuteCycleDelayMin, Bc42Intf.ExecuteCycleDelayMax);
   }
   
   return RetStatus;

} /* End BC42_INTF_ConfigExecuteCmd() */

        		  
/******************************************************************************
** Function: BC42_INTF_ConnectCmd
**
*/
bool BC42_INTF_ConnectCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   int32             CfeStatus;
   CFE_ES_AppId_t    AppId;
   CFE_ES_AppInfo_t  AppInfo;
   bool FailedToGetAppInfo = true;
   bool RetStatus = false;
   
   COMM42_Close();
   
   if (COMM42_ConnectSocket(Bc42Intf.IpAddrStr, Bc42Intf.IpPort))
   {
      
      RetStatus = true;
      CFE_EVS_SendEvent(BC42_INTF_CONNECT_TO_42_EID, CFE_EVS_EventType_INFORMATION,
                        "Connected to 42 simulator on %s port %d", Bc42Intf.IpAddrStr, Bc42Intf.IpPort);

      CfeStatus = CFE_ES_GetAppID(&AppId);
      if (CfeStatus == CFE_SUCCESS)
      { 
         CfeStatus = CFE_ES_GetAppInfo(&AppInfo, AppId);
         if (CfeStatus == CFE_SUCCESS)
         {
            FailedToGetAppInfo = false;
            if (AppInfo.NumOfChildTasks == 0)
            {
         
               CfeStatus = CHILDMGR_Constructor(CHILDMGR_OBJ, ChildMgr_TaskMainCallback, COMM42_SocketTask, &Bc42Intf.ChildTask);      
               
               if (CfeStatus != CFE_SUCCESS)
               {   
                  CFE_EVS_SendEvent(BC42_INTF_CONNECT_TO_42_EID, CFE_EVS_EventType_ERROR,
                                    "Failed to create child task durng socket connection, status=0x%8X", (int)CfeStatus);               
                  COMM42_Close();
                  RetStatus = false;
               }
            }
         }
      }
      if (FailedToGetAppInfo)
      {
         CFE_EVS_SendEvent(BC42_INTF_CONNECT_TO_42_EID, CFE_EVS_EventType_INFORMATION,
                           "Failed to get app info durng socket connection, status=0x%8X", (int)CfeStatus);                        
      }
      
   } /* End if connected */

   return RetStatus;

} /* End BC42_INTF_Connect42Cmd() */


/******************************************************************************
** Function: BC42_INTF_DisconnectCmd
**
** Notes:
**   1. Must match CMDMGR_CmdFuncPtr function signature
*/
bool BC42_INTF_DisconnectCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   COMM42_Close();
   return true;
   
} /* End BC42_INTF_DisconnectCmd() */


/******************************************************************************
** Function: BC42_INTF_NoOpCmd
**
*/

bool BC42_INTF_NoOpCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CFE_EVS_SendEvent (BC42_INTF_NOOP_EID, CFE_EVS_EventType_INFORMATION,
                      "No operation command received for BC42_INTF App version %d.%d.%d",
                      BC42_INTF_MAJOR_VER, BC42_INTF_MINOR_VER, BC42_INTF_PLATFORM_REV);

   return true;


} /* End BC42_INTF_NoOpCmd() */


/******************************************************************************
** Function: BC42_INTF_ResetAppCmd
**
*/

bool BC42_INTF_ResetAppCmd(void *ObjDataPtr, const CFE_MSG_Message_t *MsgPtr)
{

   CMDMGR_ResetStatus(CMDMGR_OBJ);
   CHILDMGR_ResetStatus(CHILDMGR_OBJ);
   
   COMM42_ResetStatus();
   
   return true;

} /* End BC42_INTF_ResetAppCmd() */


/******************************************************************************
** Function: AppTermCallback
**
** This function is called when the app is terminated. This should
** never occur but if it does a graceful termination is attempted.
*/
static void AppTermCallback(void)
{
   
   CFE_ES_WriteToSysLog("BC42_INTF app termination function shutting down COMM42 interface\n");   /* Use SysLog, events may not be working */
   COMM42_Shutdown();
   
} /* End AppTermCallback() */


/******************************************************************************
** Function: InitApp
**
*/
static int32 InitApp(void)
{

   int32 RetStatus = APP_C_FW_CFS_ERROR;
   CFE_SB_Qos_t SbQos;
   

   CFE_PSP_MemSet((void*)&Bc42Intf, 0, sizeof(BC42_INTF_APP_Class_t));
   
   /*
   ** Read JSON INI Table & class variable defaults defined in JSON  
   */

   if (INITBL_Constructor(INITBL_OBJ, BC42_INTF_INI_FILENAME, &IniCfgEnum))
   {
   
      Bc42Intf.IpPort = INITBL_GetIntConfig(INITBL_OBJ, CFG_BC42_SOCKET_PORT);
      strncpy(Bc42Intf.IpAddrStr, INITBL_GetStrConfig(INITBL_OBJ, CFG_BC42_SOCKET_ADDR_STR), BC42_INTF_IP_ADDR_STR_LEN);
   
      Bc42Intf.ExecuteMsgCycles   = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_MSG_CYCLES_DEF);
      Bc42Intf.ExecuteMsgCycleMin = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_MSG_CYCLES_MIN);
      Bc42Intf.ExecuteMsgCycleMax = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_MSG_CYCLES_MAX);
      
      Bc42Intf.ExecuteCycleDelay    = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_CYCLE_DELAY_DEF);
      Bc42Intf.ExecuteCycleDelayMin = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_CYCLE_DELAY_MIN);
      Bc42Intf.ExecuteCycleDelayMax = INITBL_GetIntConfig(INITBL_OBJ, CFG_EXE_CYCLE_DELAY_MAX);

      Bc42Intf.PerfId = INITBL_GetIntConfig(INITBL_OBJ, APP_PERF_ID);  

      Bc42Intf.CmdMid            = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_BC42_INTF_CMD_TOPICID));
      Bc42Intf.ExecuteMid        = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_BC_SCH_1_HZ_TOPICID));
      Bc42Intf.ActuatorCmdMsgMid = CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_BC42_INTF_ACTUATOR_CMD_MSG_TOPICID));

      COMM42_Constructor(COMM42_OBJ, INITBL_OBJ);
   
      /* Child Manager constructor sends error events */
      Bc42Intf.ChildTask.TaskName  = INITBL_GetStrConfig(INITBL_OBJ, CFG_CHILD_NAME);
      Bc42Intf.ChildTask.StackSize = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_STACK_SIZE);
      Bc42Intf.ChildTask.Priority  = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_PRIORITY);
      Bc42Intf.ChildTask.PerfId    = INITBL_GetIntConfig(INITBL_OBJ, CFG_CHILD_PERF_ID);

      RetStatus = CHILDMGR_Constructor(CHILDMGR_OBJ, ChildMgr_TaskMainCallback,
                                       COMM42_SocketTask, &Bc42Intf.ChildTask); 
                                         
      /*
      ** Initialize app level interfaces
      */

      SbQos.Priority    = 0;
      SbQos.Reliability = 0;
      CFE_SB_CreatePipe(&Bc42Intf.CmdPipe, INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_DEPTH), INITBL_GetStrConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_NAME));
      CFE_SB_Subscribe(Bc42Intf.CmdMid, Bc42Intf.CmdPipe);
      CFE_SB_SubscribeEx(Bc42Intf.ExecuteMid, Bc42Intf.CmdPipe, SbQos, INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_EXE_MSG_LIM));
      CFE_SB_SubscribeEx(Bc42Intf.ActuatorCmdMsgMid, Bc42Intf.CmdPipe, SbQos, INITBL_GetIntConfig(INITBL_OBJ, CFG_APP_CMD_PIPE_ACT_MSG_LIM));

      CMDMGR_Constructor(CMDMGR_OBJ);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, BC42_INTF_NOOP_CC,           NULL, BC42_INTF_NoOpCmd,          0);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, BC42_INTF_RESET_CC,          NULL, BC42_INTF_ResetAppCmd,      0);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, BC42_INTF_CONFIG_EXECUTE_CC, NULL, BC42_INTF_ConfigExecuteCmd, sizeof(BC42_INTF_ConfigExecuteCmd_Payload_t));
      CMDMGR_RegisterFunc(CMDMGR_OBJ, BC42_INTF_CONNECT_CC,        NULL, BC42_INTF_ConnectCmd,       0);
      CMDMGR_RegisterFunc(CMDMGR_OBJ, BC42_INTF_DISCONNECT_CC,     NULL, BC42_INTF_DisconnectCmd,    0);

      CFE_MSG_Init(CFE_MSG_PTR(Bc42Intf.StatusTlm.TelemetryHeader), CFE_SB_ValueToMsgId(INITBL_GetIntConfig(INITBL_OBJ, CFG_BC42_INTF_STATUS_TLM_TOPICID)), sizeof(BC42_INTF_StatusTlm_t));

          
      OS_TaskInstallDeleteHandler(AppTermCallback); /* Call when application terminates */

      /*
      ** Application startup event message
      */
      CFE_EVS_SendEvent(BC42_INTF_INIT_APP_EID, CFE_EVS_EventType_INFORMATION,
                        "BC42_INTF App Initialized. Version %d.%d.%d",
                        BC42_INTF_MAJOR_VER, BC42_INTF_MINOR_VER, BC42_INTF_PLATFORM_REV);

   } /* End if INITBL Constructed */ 
   
   return RetStatus;

} /* End of InitApp() */


/******************************************************************************
** Function: ProcessCmdPipe
**
** This ExecuteLoop allows a user some control over how many simulator control
** cycles are executed for each scheduler wakeup. COMM42 manages the interface 
** details such as whether the interface is connected, previous control cycle
** has completed, etc.
*/
static int32 ProcessCmdPipe(void)
{

   int32   RetStatus = CFE_ES_RunStatus_APP_RUN;
   int32   SbStatus;
   int32   MsgStatus;
   bool    ExecuteLoop  = false;
   uint16  ExecuteCycle = 0;
   
   CFE_SB_Buffer_t  *SbBufPtr;
   CFE_SB_MsgId_t   MsgId = CFE_SB_INVALID_MSG_ID;
   
   
   CFE_ES_PerfLogExit(Bc42Intf.PerfId);
   SbStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, Bc42Intf.CmdPipe, CFE_SB_PEND_FOREVER);
   CFE_ES_PerfLogEntry(Bc42Intf.PerfId);
   
   do
   {

      if (SbStatus == CFE_SUCCESS)
      {
         
         MsgStatus = CFE_MSG_GetMsgId(&SbBufPtr->Msg, &MsgId);
      
         if (MsgStatus == CFE_SUCCESS)
         {
   
            CFE_EVS_SendEvent(BC42_INTF_DEBUG_EID, CFE_EVS_EventType_DEBUG,
                              "***ProcessSbPipe(): ExecuteLoop=%d,  ExecuteCycle=%d, MsgId=0x%04X, SensorDataMsg.InitCycle=%d",
                              ExecuteLoop,ExecuteCycle,CFE_SB_MsgIdToValue(MsgId),Bc42Intf.Comm42.SensorDataMsg.Payload.InitCycle);
         
            if (CFE_SB_MsgId_Equal(MsgId, Bc42Intf.CmdMid))
            {
               CMDMGR_DispatchFunc(CMDMGR_OBJ, &SbBufPtr->Msg);
            }
            else if (CFE_SB_MsgId_Equal(MsgId, Bc42Intf.ExecuteMid))
            {
               COMM42_ManageExecution();
               ++ExecuteCycle;
               ExecuteLoop = true;
               SendHousekeepingPkt();
            }
            else if (CFE_SB_MsgId_Equal(MsgId, Bc42Intf.ActuatorCmdMsgMid))
            {
               COMM42_SendActuatorCmds((BC42_INTF_ActuatorCmdMsg_t *)&SbBufPtr->Msg);
            }
            else
            {            
               CFE_EVS_SendEvent(BC42_INTF_PROCESS_CMD_PIPE_EID, CFE_EVS_EventType_ERROR,
                                 "Received invalid command packet, MID = 0x%04X(%d)", 
                                 CFE_SB_MsgIdToValue(MsgId), CFE_SB_MsgIdToValue(MsgId));
            }
        
         } /* End valid message ID */

      } /* End if SB received a packet */
      
      if (ExecuteLoop) 
      {
         if (ExecuteCycle < Bc42Intf.ExecuteMsgCycles)
         {
            OS_TaskDelay(Bc42Intf.ExecuteCycleDelay);
            COMM42_ManageExecution();
            ++ExecuteCycle;
         }
         else 
         {
            ExecuteLoop = false;
         }
      }
      
      CFE_ES_PerfLogExit(Bc42Intf.PerfId);
      SbStatus = CFE_SB_ReceiveBuffer(&SbBufPtr, Bc42Intf.CmdPipe, CFE_SB_POLL);
      CFE_ES_PerfLogEntry(Bc42Intf.PerfId);
   
   } while (ExecuteLoop || (SbStatus == CFE_SUCCESS));
   
   if (SbStatus != CFE_SB_NO_MESSAGE)
   {
      RetStatus = CFE_ES_RunStatus_APP_ERROR;
   }
      
   return RetStatus;
   
} /* End ProcessCmdPipe() */


/******************************************************************************
** Function: SendHousekeepingPkt
**
*/
void SendHousekeepingPkt(void)
{
   
   BC42_INTF_StatusTlm_Payload_t *Payload = &Bc42Intf.StatusTlm.Payload;

   /*
   ** Framework Data
   */

   Payload->ValidCmdCnt    = Bc42Intf.CmdMgr.ValidCmdCnt;
   Payload->InvalidCmdCnt  = Bc42Intf.CmdMgr.InvalidCmdCnt;

   /*
   ** COMM42 Data
   */
   
   Payload->ExecuteCycleCnt   = Bc42Intf.Comm42.ExecuteCycleCnt;
   Payload->ActuatorCmdMsgCnt = Bc42Intf.Comm42.ActuatorCmdMsgCnt;
   Payload->SensorDataMsgCnt  = Bc42Intf.Comm42.SensorDataMsgCnt;
   Payload->SocketConnected   = Bc42Intf.Comm42.SocketConnected;

   CFE_SB_TimeStampMsg(CFE_MSG_PTR(Bc42Intf.StatusTlm.TelemetryHeader));
   CFE_SB_TransmitMsg(CFE_MSG_PTR(Bc42Intf.StatusTlm.TelemetryHeader), true);

} /* End SendHousekeepingPkt() */
