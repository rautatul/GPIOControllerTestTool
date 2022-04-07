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

#include <ntddk.h>
#include <wdf.h>
#include "interrupt.h"
#include "internal.h"
#include "new_delete.h"
#include "gpioioctl.h"
#include "device.h"

#include "trace.h"
#include "interrupt.tmh"

_Use_decl_annotations_
GPIO_INTERRUPT::GPIO_INTERRUPT(
    VOID
    )
    :
    InterruptsCount(0),
    Interrupts(NULL)
{
}

_Use_decl_annotations_
GPIO_INTERRUPT::~GPIO_INTERRUPT(
    VOID
    )
{
    FuncEntry(TRACE_FLAG_GPIO);

    if (Interrupts)
    {
        delete[] Interrupts;
    }

    FuncExit(TRACE_FLAG_GPIO);
}

_Use_decl_annotations_
NTSTATUS
GPIO_INTERRUPT::RetrieveInterruptContextForRequest(
    WDFREQUEST Request,
    PINTERRUPT_CONTEXT* InterruptContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID buffer;
    PGPIO_INTERRUPT_IN interruptIn;
    size_t Length;

    FuncEntry(TRACE_FLAG_GPIO);

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(GPIO_INTERRUPT_IN),
        &buffer,
        &Length);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get input buffer for request - %!STATUS!",
            status);
        goto exit;
    }
    if (Length != sizeof(GPIO_INTERRUPT_IN))
    {
        status = STATUS_INVALID_DEVICE_REQUEST;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get input buffer for request - expected %ld[B] but got %ld[B]",
            sizeof(GPIO_INTERRUPT_IN),
            static_cast<LONG>(Length));
        goto exit;
    }

    interruptIn = static_cast<PGPIO_INTERRUPT_IN>(buffer);

    if (interruptIn->InterruptIndex >= InterruptsCount)
    {
        status = STATUS_INVALID_DEVICE_REQUEST;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt index is too high - expected less than %ld but got %ld",
            InterruptsCount,
            interruptIn->InterruptIndex);
        goto exit;
    }

    *InterruptContext = GetInterruptContext(Interrupts [interruptIn->InterruptIndex]);

exit:

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_INTERRUPT::Initialize(
    WDFDEVICE Device,
    WDFCMRESLIST ResourcesTranslated,
    WDFCMRESLIST ResourcesRaw
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG resourcesCount;
    ULONG resourceIndex;
    ULONG interruptIndex;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;

    FuncEntry(TRACE_FLAG_GPIO);

    resourcesCount = WdfCmResourceListGetCount(ResourcesTranslated);
    for (resourceIndex = 0; resourceIndex < resourcesCount; resourceIndex++)
    {
        descriptor = WdfCmResourceListGetDescriptor(
            ResourcesTranslated,
            resourceIndex);
        if (descriptor->Type == CmResourceTypeInterrupt)
        {
            InterruptsCount++;
        }
    }

    if (InterruptsCount == 0)
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_GPIO_INTERRUPT,
            "There is no INTERRUPTS to initialize.");
        goto exit;
    }

    Interrupts = new WDFINTERRUPT[InterruptsCount];
    if (!Interrupts)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot alloacte memory for %d WDFINTERRUPTs",
            InterruptsCount);
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    interruptIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourcesCount; resourceIndex++)
    {
        descriptor = WdfCmResourceListGetDescriptor(
            ResourcesTranslated,
            resourceIndex);
        if (!(descriptor->Type == CmResourceTypeInterrupt))
        {
            continue;
        }

        WDF_OBJECT_ATTRIBUTES interruptAttributes;
        WDF_INTERRUPT_CONFIG interruptConfig;
        WDF_OBJECT_ATTRIBUTES spinLockAttributes;
        PINTERRUPT_CONTEXT interruptContext;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(
            &interruptAttributes,
            INTERRUPT_CONTEXT);
        WDF_INTERRUPT_CONFIG_INIT(
                        &interruptConfig,
                        OnInterrupt,
                        NULL);
        interruptConfig.PassiveHandling = TRUE;
        interruptConfig.InterruptTranslated = WdfCmResourceListGetDescriptor(
            ResourcesTranslated, 
            resourceIndex);
        interruptConfig.InterruptRaw = WdfCmResourceListGetDescriptor(
            ResourcesRaw, 
            resourceIndex);
        status = WdfInterruptCreate(
                        Device,
                        &interruptConfig,
                        &interruptAttributes,
                        &Interrupts [interruptIndex]);
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_GPIO_INTERRUPT,
                "Cannot create WDFINTERRUPT (%d out of %d)",
                interruptIndex,
                InterruptsCount);
            goto exit;
        }

        interruptContext = GetInterruptContext(Interrupts [interruptIndex]);
        interruptContext->Index = interruptIndex;
        interruptContext->State = NotDetected;
        interruptContext->WaitRequest = NULL;
        interruptContext->AcknowledgeRequest = NULL;
        KeInitializeEvent(
            &interruptContext->WaitEvent,
            SynchronizationEvent,
            FALSE);

        WDF_OBJECT_ATTRIBUTES_INIT(&spinLockAttributes);
        spinLockAttributes.ParentObject = Interrupts [interruptIndex];
        status = WdfSpinLockCreate(
            &spinLockAttributes,
            &interruptContext->Lock);
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_GPIO_INTERRUPT,
                "Cannot create WDFSPINLOCK for WDFINTERRUPT (%d out of %d)",
                interruptIndex,
                InterruptsCount);
            goto exit;
        }

        interruptIndex++;
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Initialized %d GPIO Interrupts",
        InterruptsCount);

exit:

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
VOID
GPIO_INTERRUPT::DeInitialize(
VOID
)
{
    FuncEntry(TRACE_FLAG_GPIO);
    InterruptsCount = 0;
    if (Interrupts)
    {
        delete[] Interrupts;
        Interrupts = NULL;
    }
    FuncExit(TRACE_FLAG_GPIO);
}

_Use_decl_annotations_
NTSTATUS
GPIO_INTERRUPT::WaitForInterrupt(
    WDFREQUEST Request
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PINTERRUPT_CONTEXT interruptContext = NULL;
    BOOLEAN releaseContext = FALSE;
    WDFREQUEST requestToComplete = Request;
    BOOLEAN releaseLock = FALSE;

    FuncEntry(TRACE_FLAG_GPIO);

    status = RetrieveInterruptContextForRequest(
        Request,
        &interruptContext);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get interrupt context - %!STATUS!",
            status);
        goto exit;
    }
    releaseContext = TRUE;

    WdfSpinLockAcquire(interruptContext->Lock);
    releaseLock = TRUE;

    if (interruptContext->WaitRequest)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] already has an awaiting request (%p) - completing request (%p)",
            interruptContext->Index,
            interruptContext->WaitRequest,
            Request);
        status = STATUS_INVALID_DEVICE_STATE;
        goto exit;
    }

    if (interruptContext->State == Detected)
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has already been detected - completing request (%p)",
            interruptContext->Index,
            Request);
        goto exit;
    }

    status = WdfRequestMarkCancelableEx(
        Request,
        OnWaitInterruptRequestCancel);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get mark wait request (%p) as cancelable for interrupt [%d] - %!STATUS!",
            Request,
            interruptContext->Index,
            status);
        goto exit;
    }

    requestToComplete = NULL;
    interruptContext->WaitRequest = Request;
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Interrupt [%d] has not been detected yet - waiting to complete request (%p)",
        interruptContext->Index,
        Request);

exit:

    if (releaseContext && releaseLock)
    {
        WdfSpinLockRelease(interruptContext->Lock);
    }

    if (requestToComplete)
    {
        WdfRequestComplete(
            requestToComplete,
            status);
    }

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_INTERRUPT::CancelWaitForInterrupt(
    WDFREQUEST Request
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PINTERRUPT_CONTEXT interruptContext = NULL;
    BOOLEAN releaseContext = FALSE;
    BOOLEAN releaseLock = FALSE;

    FuncEntry(TRACE_FLAG_GPIO);

    status = RetrieveInterruptContextForRequest(
        Request,
        &interruptContext);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get interrupt context - %!STATUS!",
            status);
        goto exit;
    }
    releaseContext = TRUE;

    WdfSpinLockAcquire(interruptContext->Lock);
    releaseLock = TRUE;

    if (interruptContext->WaitRequest != Request)
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] wait request (%p) has not been completed as canceling request (%p)",
            interruptContext->Index,
            interruptContext->WaitRequest,
            Request);
        status = STATUS_INVALID_DEVICE_STATE;
        goto exit;
    }

    interruptContext->WaitRequest = NULL;

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Interrupt [%d] waiting request (%p) has been cancelled",
        interruptContext->Index,
        Request);

exit:

    if (releaseContext && releaseLock)
    {
        WdfSpinLockRelease(interruptContext->Lock);
    }

    WdfRequestComplete(
        Request,
        STATUS_CANCELLED);

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
VOID
OnWaitInterruptRequestCancel(
    WDFREQUEST Request
    )
{
    PDEVICE_CONTEXT pDevice = GetDeviceContext(
        WdfIoQueueGetDevice(
        WdfRequestGetIoQueue(Request)));

    FuncEntry(TRACE_FLAG_GPIO);

    pDevice->GpioInterrupt->CancelWaitForInterrupt(Request);

    FuncExit(TRACE_FLAG_GPIO);
}

NTSTATUS
GPIO_INTERRUPT::AcknowledgeInterrupt(
    _In_ WDFREQUEST Request
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PINTERRUPT_CONTEXT interruptContext = NULL;
    BOOLEAN releaseContext = FALSE;
    BOOLEAN releaseLock = FALSE;

    FuncEntry(TRACE_FLAG_GPIO);

    status = RetrieveInterruptContextForRequest(
        Request,
        &interruptContext);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Cannot get interrupt context - %!STATUS!",
            status);
        goto exit;
    }
    releaseContext = TRUE;

    WdfSpinLockAcquire(interruptContext->Lock);
    releaseLock = TRUE;

    if (interruptContext->State != Detected)
    {
        status = STATUS_INVALID_DEVICE_STATE;
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has not been detected yet. Nothing to acknowledge. "
            "Complete request (%p) with status - %!STATUS!",
            interruptContext->Index,
            Request,
            status);
        goto exit;
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Interrupt [%d] is being marked to be acknowledged by request (%p)",
        interruptContext->Index,
        Request);

    interruptContext->AcknowledgeRequest = Request;
    KeSetEvent(
        &interruptContext->WaitEvent,
        IO_NO_INCREMENT,
        FALSE);
    interruptContext->State = NotDetected;


exit:

    if (releaseContext && releaseLock)
    {
        WdfSpinLockRelease(interruptContext->Lock);
    }

    WdfRequestComplete(
        Request,
        status);

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
BOOLEAN
OnInterrupt(
    WDFINTERRUPT Interrupt,
    ULONG /* MessageId */
    )
{
    PINTERRUPT_CONTEXT interruptContext = GetInterruptContext(Interrupt);
    WDFREQUEST requestToComplete = NULL;
    WDFREQUEST acknowledgeRequest = NULL;
    LARGE_INTEGER timeout;
    NTSTATUS status;

    FuncEntry(TRACE_FLAG_GPIO);

    WdfSpinLockAcquire(interruptContext->Lock);

    interruptContext->State = Detected;
    if (!interruptContext->WaitRequest)
    {
        goto wait;
    }

    requestToComplete = interruptContext->WaitRequest;
    interruptContext->WaitRequest = NULL;

    status = WdfRequestUnmarkCancelable(requestToComplete);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has been detected, but cannot complete wait request (%p) - %!STATUS!",
            interruptContext->Index,
            requestToComplete,
            status);
        requestToComplete = NULL;
        goto wait;
    }

wait:

    WdfSpinLockRelease(interruptContext->Lock);

    if (requestToComplete)
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has been detected, completing wait request (%p)",
            interruptContext->Index,
            requestToComplete);
        WdfRequestComplete(
            requestToComplete,
            STATUS_SUCCESS);
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Interrupt [%d] has been detected - waiting for acknowledge",
        interruptContext->Index);
    timeout.QuadPart = WDF_REL_TIMEOUT_IN_SEC(4 * 60);
    status = KeWaitForSingleObject(
        &interruptContext->WaitEvent,
        Executive,
        KernelMode,
        FALSE,
        &timeout);

    WdfSpinLockAcquire(interruptContext->Lock);
    interruptContext->State = NotDetected;
    acknowledgeRequest = interruptContext->AcknowledgeRequest;
    interruptContext->AcknowledgeRequest = NULL;
    WdfSpinLockRelease(interruptContext->Lock);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has been detected, but wait for acknowledge failed - %!STATUS!",
            interruptContext->Index,
            status);
        goto exit;
    }

    if (!acknowledgeRequest)
    {
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has been detected, but there was no acknowledge call!",
            interruptContext->Index);
        goto exit;
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Interrupt [%d] has been acknowledged by request (%p)",
        interruptContext->Index,
        acknowledgeRequest);

exit:
    FuncExit(TRACE_FLAG_GPIO);
    return TRUE;
}

_Use_decl_annotations_
VOID
GPIO_INTERRUPT::AcknowledgeAllInterrupts(
    VOID
    )
{
    FuncEntry(TRACE_FLAG_GPIO);

    for (ULONG interruptIndex = 0; interruptIndex < InterruptsCount; interruptIndex++)
    {
        PINTERRUPT_CONTEXT interruptContext = GetInterruptContext(
            Interrupts [interruptIndex]);
        NTSTATUS status = KeSetEvent(
            &interruptContext->WaitEvent,
            IO_NO_INCREMENT,
            FALSE);
        Trace(
            TRACE_LEVEL_VERBOSE,
            TRACE_FLAG_GPIO_INTERRUPT,
            "Interrupt [%d] has been acknowledged - %!STATUS!",
            interruptContext->Index,
            status);
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_INTERRUPT,
        "Acknowleddged %d interrupts",
        InterruptsCount);

    FuncExit(TRACE_FLAG_GPIO);
}