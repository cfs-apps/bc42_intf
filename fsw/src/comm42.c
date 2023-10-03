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
**    1. Use BC42_LIB's BC42_ReadFromSocket() and BC42_WriteToSocket()
**       because they contain autogenerated interface code that uses
**       AcStruct. Don't use 42's iokit's InitSocketClient() because
**      it exits the program on errors.
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

//~bc~ 42 doesn't define in a header
extern int ReadFromSocket(SOCKET Socket, struct AcType *AC); //~bc~ 
extern void WriteToSocket(SOCKET Socket, struct AcType *AC); //~bc~ 

/***********************/
/** Macro Definitions **/
/***********************/

/* 42 AC struct access macros */
#define AC42          &(Comm42->Bc42->AcVar)  
#define AC42_(field)  (Comm42->Bc42->AcVar.field)  


/**********************/
/** File Global Data **/
/**********************/

static COMM42_Class_t *Comm42 = NULL;


/*******************************/
/** Local Function Prototypes **/
/*******************************/


/******************************************************************************
** Function: COMM42_Constructor
**
** Initialize the 42 interface.
**
** Notes:
**   1. This must be called prior to any other function.
**   2. AC structure initialization mimics 42's AcApp.c dynamic memory 
**      configuration so AcApp's functions can be used unmodified.
**   3. Socket configuration parameters from the JSON init file are
**      are saved but a socket connection is not attempted. A socket 
**      is performed in response to a connect command.
*/
void COMM42_Constructor(COMM42_Class_t *Comm42Obj, const INITBL_Class_t *IniTbl)
{

   int32 CfeStatus;

   Comm42 = Comm42Obj;

   CFE_PSP_MemSet((void*)Comm42, 0, sizeof(COMM42_Class_t));
  
   Comm42->ChildTaskRun = true;
   Comm42->SocketConnected = false;
   Comm42->UnclosedCycleLim = INITBL_GetIntConfig(IniTbl, CFG_EXE_UNCLOSED_CYCLE_LIM);

   CFE_MSG_Init(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader), 
                CFE_SB_ValueToMsgId(INITBL_GetIntConfig(IniTbl, CFG_BC42_INTF_SENSOR_DATA_MSG_TOPICID)),
                sizeof(BC42_INTF_SensorDataMsg_t));
   
   /* Create semaphore (given by parent to wake-up child) */
   CfeStatus = OS_BinSemCreate(&Comm42->WakeUpSemaphore, INITBL_GetStrConfig(IniTbl, CFG_CHILD_SEM_NAME), OS_SEM_EMPTY, 0);
   
   if (CfeStatus != CFE_SUCCESS)
   {
      CFE_EVS_SendEvent(COMM42_CREATE_SEM_EID, CFE_EVS_EventType_ERROR,
                        "Failed to create %s semaphore. Status=0x%8X", 
                        INITBL_GetStrConfig(IniTbl, CFG_CHILD_SEM_NAME), (int)CfeStatus);
   }

   BC42_Constructor();
   
} /* End COMM42_Constructor() */


/******************************************************************************
** Function: COMM42_Close
**
*/
void COMM42_Close(void)
{

   if (Comm42->SocketConnected == true)
   {
    
      OS_close(Comm42->SocketId);
   
      Comm42->SocketConnected = false;
      
      CFE_EVS_SendEvent(COMM42_SOCKET_CLOSE_EID, CFE_EVS_EventType_INFORMATION,
                        "Successfully closed socket");

  
   } /* End if connected */
   else
   {
      CFE_EVS_SendEvent(COMM42_SOCKET_CLOSE_EID, CFE_EVS_EventType_DEBUG,
                        "Attempt to close socket without a connection");
   }

   Comm42->ActuatorCmdMsgSent = false;
   COMM42_ResetStatus();

} /* End COMM42_Close() */


/******************************************************************************
** Function:  COMM42_ConnectSocket
**
*/
bool COMM42_ConnectSocket(const char *AddrStr, uint32 Port)
{
   int32 Status;
   
   Comm42->SocketConnected = false;
   
   Status = OS_SocketOpen(&Comm42->SocketId, OS_SocketDomain_INET, OS_SocketType_STREAM);
   if (Status == OS_SUCCESS)
   {
      
      CFE_EVS_SendEvent(COMM42_SOCKET_OPEN_EID, CFE_EVS_EventType_INFORMATION, 
                        "Successfully opened socket, ID = %d", (int)Comm42->SocketId);

      OS_SocketAddrInit(&Comm42->SocketAddr, OS_SocketDomain_INET);
      OS_SocketAddrFromString(&Comm42->SocketAddr, AddrStr);   
      OS_SocketAddrSetPort(&Comm42->SocketAddr, Port);
   
      Status = OS_SocketConnect(Comm42->SocketId, &Comm42->SocketAddr, 2000);
      if (Status == OS_SUCCESS)
      {
         Comm42->InitCycle = true;
         Comm42->SocketConnected = true;
         CFE_EVS_SendEvent(COMM42_SOCKET_CONNECT_EID, CFE_EVS_EventType_INFORMATION, 
                           "Successfully connected to socket address %s, listening on port %d",
                           AddrStr, (int)Port);
         
      }
      else
      {
         CFE_EVS_SendEvent(COMM42_SOCKET_CONNECT_EID, CFE_EVS_EventType_ERROR, 
                           "Error connecting to socket address %s, OS_SocketBind() return code = %d", 
                           AddrStr, (int)Status);
      }
   } /* End if open */
   else
   {
      CFE_EVS_SendEvent(COMM42_SOCKET_OPEN_EID, CFE_EVS_EventType_ERROR, 
                        "Error opening socket. OS_SocketOpen() return code = %d", (int)Status);
   }
   
   OS_BinSemGive(Comm42->WakeUpSemaphore); //TODO: Need to think through this
   
   return Comm42->SocketConnected;
      
} /* End COMM42_ConnectSocket() */


/******************************************************************************
** Function:  COMM42_ManageExecution
**
** Notes:
**   1. Giving the semaphore signals the child task to receive sensor data
**   2. An 'unclosed cycle' is when the sensor-controller-actuator cycle didn't
**      complete before this function was called again. If this persists past
**      a limit then the logic will force a new cycle to begin by giving the
**      the child task the semaphore 
**   3. The WakeUpSemaphore can become invalid when the child task is
**      intentionally terminated in a disconnect scenario so don't take any
**      action. 
*/
void COMM42_ManageExecution(void)
{

   CFE_EVS_SendEvent(COMM42_DEBUG_EID, CFE_EVS_EventType_DEBUG, 
                     "*** COMM42_App::ManageExecution(%d): WakeUpSemaphore=%08X, ActuatorPktSent=%d",
                     Comm42->ExecuteCycleCnt, Comm42->WakeUpSemaphore, Comm42->ActuatorCmdMsgSent);
   
   if (Comm42->SocketConnected)
   {
    
      if (Comm42->InitCycle)
      {
               
         CFE_EVS_SendEvent(COMM42_SKIP_INIT_CYCLE_EID, CFE_EVS_EventType_INFORMATION,
                           "Skipping scheduler execution request during init cycle");
      
         return;
      }
      
      if (Comm42->WakeUpSemaphore != COMM42_SEM_INVALID)  // TODO: Not currently being set
      { 
         
         if (Comm42->ActuatorCmdMsgSent == true)
         {
         
            CFE_EVS_SendEvent(COMM42_DEBUG_EID, CFE_EVS_EventType_DEBUG,
                              "**** COMM42_ManageExecution(): Giving semaphore - WakeUpSemaphore=%08X, ActuatorPktSent=%d",
                              Comm42->WakeUpSemaphore,Comm42->ActuatorCmdMsgSent);
            
            OS_BinSemGive(Comm42->WakeUpSemaphore);
            Comm42->UnclosedCycleCnt = 0;
         
         }
         else
         {
            
            ++Comm42->UnclosedCycleCnt;
            if (Comm42->UnclosedCycleCnt > Comm42->UnclosedCycleLim)
            {
         
               /* Consider restarting child task if this doesn't fix the issue. If the issue occurs! */
               CFE_EVS_SendEvent(COMM42_NO_ACTUATOR_CMD_EID, CFE_EVS_EventType_ERROR,
                                 "Actuator command not received for %d execution cycles. Giving child semaphore",
                                 Comm42->UnclosedCycleCnt);

               OS_BinSemGive(Comm42->WakeUpSemaphore); 
               Comm42->UnclosedCycleCnt = 0;
            
            }/* End if unclosed cycle */
         } /* End if no actuator packet */
      } /* End if semaphore valid */
       
   } /* End if connected */
   else
   {
      COMM42_ResetStatus();
   }

} /* COMM42_ManageExecution() */


/******************************************************************************
** Function: COMM42_RecvSensorData
**
*/
bool COMM42_RecvSensorData(BC42_INTF_SensorDataMsg_t *SensorDataMsg)
{

   int   i, NumBytesRead;
   bool  RetStatus = false;
   
   BC42_INTF_SensorDataMsg_Payload_t *SensorData = &SensorDataMsg->Payload;
   
   CFE_EVS_SendEvent(COMM42_DEBUG_EID, CFE_EVS_EventType_DEBUG,
                     "**** COMM42_RecvSensorData(): ExecCnt=%d, SensorCnt=%d, ActuatorCnt=%d, ActuatorSent=%d",
                     Comm42->ExecuteCycleCnt, Comm42->SensorDataMsgCnt, Comm42->ActuatorCmdMsgCnt, Comm42->ActuatorCmdMsgSent);

   Comm42->Bc42 = BC42_TakePtr();
   AC42_(EchoEnabled) = false;

   NumBytesRead = BC42_ReadFromSocket(Comm42->SocketId, &Comm42->SocketAddr, &Comm42->Bc42->AcVar);
   
   if (NumBytesRead > 0)
   {
      GyroProcessing(AC42);
      MagnetometerProcessing(AC42);
      CssProcessing(AC42);
      FssProcessing(AC42);
      StarTrackerProcessing(AC42);
      GpsProcessing(AC42);
      
      SensorData->GpsTime = AC42_(Time); 
      SensorData->GpsValid  = true;
      SensorData->StValid   = true;
      SensorData->SunValid  = AC42_(SunValid);
      SensorData->InitCycle = Comm42->InitCycle;

      for (i=0; i < 3; i++) {
      
         SensorData->PosN[i] = AC42_(PosN[i]);  /* GPS */
         SensorData->VelN[i] = AC42_(VelN[i]);
      
         SensorData->qbn[i]  = AC42_(qbn[i]);   /* ST */

         SensorData->wbn[i]  = AC42_(wbn[i]);   /* Gyro */
      
         SensorData->svb[i]  = AC42_(svb[i]);   /* CSS/FSS */

         SensorData->bvb[i]  = AC42_(bvb[i]);   /* MTB */
      
         SensorData->WhlH[i] = AC42_(Whl[i].H); /* Wheels */
      }

      SensorData->qbn[3]  = AC42_(qbn[3]);
      SensorData->WhlH[3] = AC42_(Whl[3].H);
      
      RetStatus = true;
   }

   BC42_GivePtr(Comm42->Bc42);

   return RetStatus;

} /* COMM42_RecvSensorData() */


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
  
} /* End COMM42_ResetStatus() */


/******************************************************************************
** Function: COMM42_SendActuatorCmds
**
*/
bool COMM42_SendActuatorCmds(const BC42_INTF_ActuatorCmdMsg_t *ActuatorCmdMsg) 
{

   int i;
   const BC42_INTF_ActuatorCmdMsg_Payload_t *ActuatorCmd = &ActuatorCmdMsg->Payload;
   

   CFE_EVS_SendEvent(COMM42_DEBUG_EID, CFE_EVS_EventType_DEBUG,
                     "**** COMM42_SendActuatorCmds(): ExeCnt=%d, SnrCnt=%d, ActCnt=%d, ActSent=%d",
                     Comm42->ExecuteCycleCnt, Comm42->SensorDataMsgCnt, Comm42->ActuatorCmdMsgCnt, Comm42->ActuatorCmdMsgSent);
   
   Comm42->Bc42 = BC42_TakePtr();

   for (i=0; i < 3; i++) 
   {
      AC42_(Tcmd[i]) = ActuatorCmd->Tcmd[i];
      AC42_(Mcmd[i]) = ActuatorCmd->Mcmd[i];
   }
   
   AC42_(G[0].Cmd.Ang[0]) = ActuatorCmd->SaGcmd;

   WheelProcessing(AC42);
   MtbProcessing(AC42);
   
   BC42_WriteToSocket(Comm42->SocketId, &Comm42->SocketAddr, AC42);
   
   BC42_GivePtr(Comm42->Bc42);
   
   Comm42->InitCycle = false;
   ++Comm42->ActuatorCmdMsgCnt;
   Comm42->ActuatorCmdMsgSent = true;
      
   return true;

} /* End COMM42_SendActuatorCmds() */


/******************************************************************************
** Function: COMM42_Shutdown
**
** Close the socket and force a child task exit. The sequence of this code must
** work with the COMM42_SocketTask() logic to avoid strange. 
** 
*/
void COMM42_Shutdown(void)
{
   
   COMM42_Close();
   Comm42->ChildTaskRun = false;
   OS_BinSemGive(Comm42->WakeUpSemaphore);
   
} /* End COMM42_Shutdown() */


/******************************************************************************
** Function: COMM42_SocketTask
**
** Notes:
**   1. This function is continuously called by the app_c_fw's ChildMgr
**      service. Returning false causes the child task to terminate.
**   2. This infinite loop design proved to be the most robust. I tried to 
**      create/terminate the child task with a socket connect/disconnect and
**      something didn't seem to get cleaned up properly and the system would
**      hang on a second connect cmd. 
**
*/
bool COMM42_SocketTask(CHILDMGR_Class_t* ChildMgr)
{
      
   int32 CfeStatus;
   
   if (Comm42->SocketConnected)
   {
      
      CFE_EVS_SendEvent(COMM42_DEBUG_EID, CFE_EVS_EventType_DEBUG,
                        "**** COMM42_SocketTask(%d) Waiting for semaphore: InitCycle=%d",
                        Comm42->ExecuteCycleCnt, Comm42->InitCycle);    
      
      CfeStatus = OS_BinSemTake(Comm42->WakeUpSemaphore); /* Pend until parent app gives semaphore */

      // During an interface shutdown ChildTaskRun is set to false and then the semaphore is given
      if (Comm42->ChildTaskRun)
      {

         /* Check connection for termination scenario */
         if (CfeStatus == CFE_SUCCESS) 
         {
            
            ++Comm42->ExecuteCycleCnt;
            if (COMM42_RecvSensorData(&(Comm42->SensorDataMsg)) > 0)
            {
               CFE_SB_TimeStampMsg(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader));
               CfeStatus = CFE_SB_TransmitMsg(CFE_MSG_PTR(Comm42->SensorDataMsg.TelemetryHeader), true);
               
               if (CfeStatus == CFE_SUCCESS)
               {
                  ++Comm42->SensorDataMsgCnt;
                  Comm42->ActuatorCmdMsgSent = false;
               }
               CFE_EVS_SendEvent(COMM42_SOCKET_TASK_EID, CFE_EVS_EventType_INFORMATION,
                     "Sent Sensor data message: cFEStatus=%d, InitCycle= %d, ExecuteCycleCnt=%d, Comm42->SensorDataMsgCnt=%d",
                     CfeStatus, Comm42->InitCycle, Comm42->ExecuteCycleCnt, Comm42->SensorDataMsgCnt);  
            }
            else
            {
               CFE_EVS_SendEvent(COMM42_SOCKET_TASK_EID, CFE_EVS_EventType_INFORMATION,
                                 "Closing socket after received data failure: InitCycle= %d, ExecuteCycleCnt=%d",
                                 Comm42->InitCycle, Comm42->ExecuteCycleCnt);    
               COMM42_Close();
            }
         
         } /* End if valid semaphore */
      } /* End if run child task */
   } /* End if socket connected */
   
   return Comm42->ChildTaskRun;

} /* End COMM42_SocketTask() */

