/*************************************************************************************************

INTEL CONFIDENTIAL

Copyright 2014 Intel Corporation
All Rights Reserved. 

The source code contained or described herein and all documents related to the source
code ("Material") are owned by Intel Corporation or its suppliers or licensors. Title to
the Material remains with Intel Corporation or its suppliers and licensors. The Material
contains trade secrets and proprietary and confidential information of Intel or its suppliers
and licensors. The Material is protected by worldwide copyright and trade secret laws and
treaty provisions. No part of the Material may be used, copied, reproduced, modified,
published, uploaded, posted, transmitted, distributed, or disclosed in any way without Intel’s
prior express written permission.

No license under any patent, copyright, trade secret or other intellectual property right
is granted to or conferred upon you by disclosure or delivery of the Materials, either expressly,
by implication, inducement, estoppel or otherwise. Any license under such intellectual property
rights must be express and approved by Intel in writing.

*************************************************************************************************/
#include <string>
#include <map>
#include <list>
#include <set>
#include <functional>

#include <iomanip>

#include <cstdlib>

#include <stdexcept>

#include <math.h>

#include <wchar.h>
#include <windows.h>
#include <winioctl.h>
#include <specstrings.h>
#include <Cfgmgr32.h>

#include <spb.h>

#include "gpioioctl.h"

using namespace std;

#if !defined( _GUID_DEVINTERFACE_GPIO_DEFINED )
#define _GUID_DEVINTERFACE_GPIO_DEFINED
DEFINE_GUID(GUID_DEVINTERFACE_GPIO,
    0xde937575, 0x7a87, 0x4182, 0xbf, 0xff, 0x98, 0x12, 0xb3, 0x8c, 0xce, 0x5f);
#endif
//
// Global Variables
//
extern HCMNOTIFICATION m_hNotification_Interface;

#define _USE_DEVICE_PATH_ASCII

#ifndef _MAX_PATH
#define _MAX_PATH   260
#endif

extern HANDLE g_Peripheral;

typedef pair<ULONG, PBYTE> BUFPAIR;
typedef list<BUFPAIR>      BUFLIST;

void wait ( int seconds );

VOID
PrintBytes(
    _In_                  ULONG BufferCb,
    _In_reads_bytes_(BufferCb) BYTE  Buffer[]
    );

_Success_(return)
bool
PopStringParameter(
    _Inout_   list<string> *Parameters,
    _Out_     string       *Value,
    _Out_opt_ bool         *Present = nullptr
    );

typedef pair<ULONG,ULONG> bounds;

_Success_(return)
bool
PopNumberParameter(
    _Inout_   list<string>      *Parameters,
    _In_      ULONG              Radix,
    _Out_     ULONG             *Value,
    _In_opt_  pair<ULONG,ULONG>  Bounds = bounds(0,0),
    _Out_opt_ bool              *Present = nullptr
    );

_Success_(return)
bool
PopDoubleParameter(
    _Inout_   list<string>        *Parameters,
    _Out_     double              *Value,
    _In_opt_  pair<double,double>  Bounds = bounds(0,0),
    _Out_opt_ bool                *Present = nullptr
    );

_Success_(return)
bool
ParseNumber(
    _In_     const string &String,
    _In_     ULONG         Radix,
    _Out_    ULONG        *Value,
    _In_opt_ bounds        Bounds = bounds(0,0)
    );

_Success_(return)
bool
ParseDouble(
    _In_     const string         &String,
    _Out_    double               *Value,
    _In_opt_ pair<double, double>  Bounds
    );

_Success_(return)
bool
PopBufferParameter(
    _Inout_ list<string>       *Parameters,
    _Out_   pair<ULONG, PBYTE> *Value
    );


#define countof(x) (sizeof(x) / sizeof(x[0]))
    
#include "command.h"
