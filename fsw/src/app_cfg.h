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
**    Define application configurations for the 42 Interface application
**
**  Notes:
**    1. This is part of prototype effort to port a 42 simulator FSW controller
**       component into a cFS-based application 
**    2. These macros can only be built with the application and can't
**       have a platform scope because the same app_cfg.h file name is used for
**       all applications following the object-based application design.
*/

#ifndef _app_cfg_
#define _app_cfg_

/*
** Includes
*/

#include "app_c_fw.h"
#include "bc42_intf_platform_cfg.h"
#include "bc42_intf_eds_typedefs.h"


/******************************************************************************
** Versions
**
** 1.0 - Initial Basecamp release created from OpenSatKit and upgraded to the
**       latest 42 version
** 1.1 - Change from 2 Hz to 1 Hz scheduler message
** 2.0 - Updated 42 versions on 9/5/25. 42's standalone AcApp had a design
**       overhaul that greatly simplifies this app.
** 3.0 - Compatible with Basecamp 4.0 and cFS 7.0
**
*/

#define  BC42_INTF_MAJOR_VER   3
#define  BC42_INTF_MINOR_VER   0


/******************************************************************************
** JSON init file definitions/declarations.
** - See app_c_demo::app_cfg.h for how to define configuration macros 
*/

#define CFG_APP_CFE_NAME  APP_CFE_NAME
#define CFG_APP_PERF_ID   APP_PERF_ID
     
#define CFG_APP_CMD_PIPE_DEPTH  APP_CMD_PIPE_DEPTH
#define CFG_APP_CMD_PIPE_NAME   APP_CMD_PIPE_NAME

#define CFG_BC42_INTF_CMD_TOPICID              BC42_INTF_CMD_TOPICID
#define CFG_BC42_INTF_STATUS_TLM_TOPICID       BC42_INTF_STATUS_TLM_TOPICID
#define CFG_BC42_INTF_SENSOR_DATA_MSG_TOPICID  BC42_INTF_SENSOR_DATA_MSG_TOPICID
#define CFG_BC42_INTF_ACTUATOR_CMD_MSG_TOPICID BC42_INTF_ACTUATOR_CMD_MSG_TOPICID
#define CFG_BC42_INTF_STATUS_TOPICID           BC_SCH_4_SEC_TOPICID    // Use different CFG_ name instead of BC_SCH_*_TOPICID to localize impact if rate changes

#define CFG_CHILD_NAME                  CHILD_NAME
#define CFG_CHILD_STACK_SIZE            CHILD_STACK_SIZE
#define CFG_CHILD_PRIORITY              CHILD_PRIORITY
#define CFG_CHILD_SEM_NAME              CHILD_SEM_NAME
#define CFG_CHILD_PERF_ID               CHILD_PERF_ID
#define CFG_CHILD_PIPE_DEPTH            CHILD_PIPE_DEPTH
#define CFG_CHILD_PIPE_NAME             CHILD_PIPE_NAME
#define CFG_CHILD_PIPE_ACTUATOR_MSG_LIM CHILD_PIPE_ACTUATOR_MSG_LIM
#define CFG_CHILD_ACTUATOR_MSG_TIMEOUT  CHILD_ACTUATOR_MSG_TIMEOUT
#define CFG_CHILD_UNCLOSED_CYCLE_LIM    CHILD_UNCLOSED_CYCLE_LIM

#define CFG_BC42_SOCKET_PORT  BC42_SOCKET_PORT


#define APP_CONFIG(XX) \
   XX(APP_CFE_NAME,char*) \
   XX(APP_PERF_ID,uint32) \
   XX(APP_CMD_PIPE_DEPTH,uint32) \
   XX(APP_CMD_PIPE_NAME,char*) \
   XX(BC42_INTF_CMD_TOPICID,uint32) \
   XX(BC42_INTF_STATUS_TLM_TOPICID,uint32) \
   XX(BC42_INTF_SENSOR_DATA_MSG_TOPICID,uint32) \
   XX(BC42_INTF_ACTUATOR_CMD_MSG_TOPICID,uint32) \
   XX(BC_SCH_4_SEC_TOPICID,uint32) \
   XX(CHILD_NAME,char*) \
   XX(CHILD_STACK_SIZE,uint32) \
   XX(CHILD_PRIORITY,uint32) \
   XX(CHILD_SEM_NAME,char*) \
   XX(CHILD_PERF_ID,uint32) \
   XX(CHILD_PIPE_DEPTH,uint32) \
   XX(CHILD_PIPE_NAME,char*) \
   XX(CHILD_PIPE_ACTUATOR_MSG_LIM,uint32) \
   XX(CHILD_ACTUATOR_MSG_TIMEOUT,uint32) \
   XX(CHILD_UNCLOSED_CYCLE_LIM,uint32) \
   XX(BC42_SOCKET_PORT,uint32) \

DECLARE_ENUM(Config,APP_CONFIG)

/******************************************************************************
** App level definitions that don't need to be in the ini file
**
*/

#define BC42_INTF_UNDEF_TLM_STR "Undefined"


/******************************************************************************
** Event Macros
**
** Define the base event message IDs used by each object/component used by the
** application. There are no automated checks to ensure an ID range is not
** exceeded so it is the developer's responsibility to verify the ranges. 
*/

#define BC42_INTF_BASE_EID  (APP_C_FW_APP_BASE_EID +  0)
#define COMM42_BASE_EID     (APP_C_FW_APP_BASE_EID + 20)

/*
** One event ID is used for all initialization debug messages. Uncomment one of
** the BC42_INTF_INIT_EVS_TYPE definitions. Set it to INFORMATION if you want to
** see the events during initialization. This is opposite to what you'd expect 
** because INFORMATION messages are enabled by default when an app is loaded.
*/

#define BC42_INTF_INIT_DEBUG_EID 999
#define BC42_INTF_INIT_EVS_TYPE CFE_EVS_DEBUG
//#define BC42_INTF_INIT_EVS_TYPE CFE_EVS_INFORMATION

/******************************************************************************
** BC42_INTF Macros
*/


#endif /* _app_cfg_ */
