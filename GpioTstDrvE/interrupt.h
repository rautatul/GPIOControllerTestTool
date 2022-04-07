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

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <ntddk.h>
#include <wdf.h>
#include "new_delete.h"

#define _GPIO_INTERRUPT__POOL_TAG 'tnIG'
#define _GPIO_INTERRUPT_INTERRUPTS__POOL_TAG 'nInI'

typedef struct _DEVICE_CONTEXT DEVICE_CONTEXT, *PDEVICE_CONTEXT;
typedef struct _INTERRUPT_CONTEXT INTERRUPT_CONTEXT, *PINTERRUPT_CONTEXT;

typedef enum _INTERRUPT_STATE
{
    NotDetected,
    Detected
} INTERRUPT_STATE, *PINTERRUPT_STATE;

class GPIO_INTERRUPT : public BaseAllocator
{
private:

    _Field_size_full_opt_(InterruptsCount)
    WDFINTERRUPT* Interrupts;
    _Field_range_(0, MAXULONG)
    ULONG InterruptsCount;

public:

    GPIO_INTERRUPT(
        VOID
        );

    ~GPIO_INTERRUPT(
        VOID
        );

protected:

    NTSTATUS
    RetrieveInterruptContextForRequest(
        _In_ WDFREQUEST Request,
        _Outptr_ PINTERRUPT_CONTEXT* InterruptContext
        );

public:
    NTSTATUS
    Initialize(
        _In_ WDFDEVICE Device,
        _In_ WDFCMRESLIST ResourcesTranslated,
        _In_ WDFCMRESLIST ResourcesRaw
        );

    VOID
    DeInitialize(
        VOID
        );

    NTSTATUS
    WaitForInterrupt(
        _In_ WDFREQUEST Request
        );

    NTSTATUS
    CancelWaitForInterrupt(
        _In_ WDFREQUEST Request
        );

    NTSTATUS
    AcknowledgeInterrupt(
        _In_ WDFREQUEST Request
        );

    VOID
    AcknowledgeAllInterrupts(
        VOID
        );

private:

    GPIO_INTERRUPT(
        _In_ const GPIO_INTERRUPT&
        );

    GPIO_INTERRUPT&
    operator = (
        _In_ const GPIO_INTERRUPT&
        );

};

EVT_WDF_REQUEST_CANCEL OnWaitInterruptRequestCancel;
EVT_WDF_INTERRUPT_ISR OnInterrupt;

#endif // _INTERRUPT_H_
