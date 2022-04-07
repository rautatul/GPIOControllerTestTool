/*++

INTEL CONFIDENTIAL
Copyright 2019 Intel Corporation All Rights Reserved.

The source code contained or described herein and all documents
related to the source code ("Material") are owned by Intel Corporation
or its suppliers or licensors. Title to the Material remains with
Intel Corporation or its suppliers and licensors. The Material
contains trade secrets and proprietary and confidential information of
Intel or its suppliers and licensors. The Material is protected by
worldwide copyright and trade secret laws and treaty provisions. No
part of the Material may be used, copied, reproduced, modified,
published, uploaded, posted, transmitted, distributed, or disclosed in
any way without Intel's prior express written permission.

No license under any patent, copyright, trade secret or other
intellectual property right is granted to or conferred upon you by
disclosure or delivery of the Materials, either expressly, by
implication, inducement, estoppel or otherwise. Any license under such
intellectual property rights must be express and approved by Intel in
writing.

--*/

#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#pragma warning(push)
#pragma warning(disable:4512)
#pragma warning(disable:4480)

#define GPIO_POOL_TAG ((ULONG) 'OIPG')

/////////////////////////////////////////////////
//
// Common includes.
//
/////////////////////////////////////////////////

#include <ntddk.h>
#include <wdm.h>
#include <wdf.h>
#include <ntstrsafe.h>

#include "spb.h"
#include "gpioioctl.h"

#include "trace.h"

//
// Forward Declarations
//

typedef struct _DEVICE_CONTEXT DEVICE_CONTEXT,  *PDEVICE_CONTEXT;
typedef struct _INTERRUPT_CONTEXT INTERRUPT_CONTEXT,  *PINTERRUPT_CONTEXT;
typedef enum _INTERRUPT_STATE INTERRUPT_STATE,  *PINTERRUPT_STATE;

class GPIO_INTERRUPT;
class GPIO_IO;

// @todo delete
#define MAX_INTERRUPT_IDS 10

struct _DEVICE_CONTEXT 
{
    WDFDEVICE FxDevice;

    WDFQUEUE IoctlQueue;

    GPIO_IO* GpioIo;

    GPIO_INTERRUPT* GpioInterrupt;

    BOOLEAN ConnectInterrupt;
};


struct _INTERRUPT_CONTEXT
{
    ULONG Index;

    INTERRUPT_STATE State;

    WDFSPINLOCK Lock;

    KEVENT WaitEvent;

    WDFREQUEST WaitRequest;

    WDFREQUEST AcknowledgeRequest;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, GetDeviceContext);
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(INTERRUPT_CONTEXT, GetInterruptContext);

#pragma warning(pop)

#endif // _INTERNAL_H_
