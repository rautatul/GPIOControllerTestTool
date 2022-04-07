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
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <gpio.h>

#include "io.h"
#include "new_delete.h"
#include "gpioioctl.h"

#include "trace.h"
#include "io.tmh"

_Use_decl_annotations_
NTSTATUS
GPIO_IO::Execute(
    WDFREQUEST Request,
    GPIO_IO_OPERATION* IoOperation
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PVOID InputBuffer = NULL;
    PGPIO_REQUEST_IN requestIn = NULL;
    PVOID OutputBuffer = NULL;

    FuncEntry(TRACE_FLAG_GPIO);

    if (!Connections)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] There is no IO connection to porcess request %p - %!STATUS!",
            IoOperation->GetName(),
            Request,
            status);
        goto exit;
    }

    status = IoOperation->RequestRetrieveInputBuffer(
        Request,
        &InputBuffer);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot get input buffer for request %p - %!STATUS!",
            IoOperation->GetName(),
            Request,
            status);
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

    requestIn = static_cast<PGPIO_REQUEST_IN>(InputBuffer);
    if (requestIn->GpioIndex >= ConnectionsCount)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Connection index is to high. Expected less than %d but got %d.",
            IoOperation->GetName(),
            ConnectionsCount,
            requestIn->GpioIndex);
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

    status = IoOperation->RequestRetrieveOutputBuffer(
        Request,
        &OutputBuffer);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot retrieve output buffer for request %p - %!STATUS!",
            IoOperation->GetName(),
            Request,
            status);
        goto exit;
    }

    status = Connections [requestIn->GpioIndex].ExecuteIoOperation(
        IoOperation,
        InputBuffer,
        OutputBuffer);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Executing on connection %d failed - %!STATUS!",
            IoOperation->GetName(),
            requestIn->GpioIndex,
            status);
        goto exit;
    }

    IoOperation->TraceSuccessCompletion(
        InputBuffer,
        OutputBuffer
        );

exit:

    IoOperation->RequestCompleteWithInformation(
        Request,
        status);

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
GPIO_IO::GPIO_IO(
    VOID
    )
    :
    ConnectionsCount(0),
    Connections(NULL),
    ReadIoOperation(NULL),
    WriteIoOperation(NULL)
{
}

_Use_decl_annotations_
GPIO_IO::~GPIO_IO(
    VOID
    )
{
    FuncEntry(TRACE_FLAG_GPIO);

    if (WriteIoOperation)
    {
        delete WriteIoOperation;
    }
    if (ReadIoOperation)
    {
        delete ReadIoOperation;
    }
    if (Connections)
    {
        delete[] Connections;
    }

    FuncExit(TRACE_FLAG_GPIO);
}

_Use_decl_annotations_
NTSTATUS
GPIO_IO::Initialize(
    WDFDEVICE Device,
    WDFCMRESLIST Resources
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG resourcesCount;
    ULONG resourceIndex;
    ULONG connectionIndex;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;

    FuncEntry(TRACE_FLAG_GPIO);

    resourcesCount = WdfCmResourceListGetCount(Resources);
    for (resourceIndex = 0; resourceIndex < resourcesCount; resourceIndex++)
    {
        descriptor = WdfCmResourceListGetDescriptor(
            Resources,
            resourceIndex);
        if (descriptor->Type == CmResourceTypeConnection
            && descriptor->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_GPIO
            && descriptor->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_GPIO_IO)
        {
            ConnectionsCount++;
        }
    }

    ReadIoOperation = new GPIO_READ_IO_OPERATION;
    if (!ReadIoOperation)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "Cannot allocate memory for READ_IO_OPERATION");
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    WriteIoOperation = new GPIO_WRITE_IO_OPERATION;
    if (!WriteIoOperation)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "Cannot allocate memory for WRITE_IO_OPERATION");
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (ConnectionsCount == 0)
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_GPIO_IO,
            "There is no IO_CONNECTION to initialize.");
        goto exit;
    }

    Connections = new GPIO_IO_CONNECTION[ConnectionsCount];
    if (!Connections)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "Cannot alloacte memory for %d IO_CONNECTIONs",
            ConnectionsCount);
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    connectionIndex = 0;
    for (resourceIndex = 0; resourceIndex < resourcesCount; resourceIndex++)
    {
        descriptor = WdfCmResourceListGetDescriptor(
            Resources,
            resourceIndex);
        if (!(descriptor->Type == CmResourceTypeConnection
            && descriptor->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_GPIO
            && descriptor->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_GPIO_IO))
        {
            continue;
        }
        status = Connections [connectionIndex].Initialize(
            Device,
            descriptor->u.Connection.IdLowPart,
            descriptor->u.Connection.IdHighPart);
        if (NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_VERBOSE,
                TRACE_FLAG_GPIO_IO,
                "Gpio Connection index %d, id (%d, %d)",
                connectionIndex,
                descriptor->u.Connection.IdLowPart,
                descriptor->u.Connection.IdHighPart
            );
        }
        else {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_GPIO_IO,
                "Cannot initialize IO_CONNECTION (%d out of %d)",
                connectionIndex,
                ConnectionsCount);
            goto exit;
        }
        connectionIndex++;
    }

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_IO,
        "Initialized %d GPIO IO connections",
        ConnectionsCount);

exit:

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
VOID
GPIO_IO::DeInitialize(
    VOID
    )
{
    FuncEntry(TRACE_FLAG_GPIO);
    if (WriteIoOperation)
    {
        delete WriteIoOperation;
        WriteIoOperation = NULL;
    }
    if (ReadIoOperation)
    {
        delete ReadIoOperation;
        ReadIoOperation = NULL;
    }
    ConnectionsCount = 0;
    if (Connections)
    {
        delete[] Connections;
        Connections = NULL;
    }
    FuncExit(TRACE_FLAG_GPIO);
}

_Use_decl_annotations_
NTSTATUS
GPIO_IO::Read(
    WDFREQUEST Request
    )
{
    return Execute(
        Request,
        ReadIoOperation);
}

_Use_decl_annotations_
NTSTATUS
GPIO_IO::Write(
    WDFREQUEST Request
    )
{
    return Execute(
        Request,
        WriteIoOperation);
}

_Use_decl_annotations_
GPIO_IO_CONNECTION::GPIO_IO_CONNECTION(
    VOID
    )
    :
    IdLowPart(0),
    IdHighPart(0),
    Device(0)
{
}

_Use_decl_annotations_
GPIO_IO_CONNECTION::~GPIO_IO_CONNECTION(
    VOID
    )
{
}

_Use_decl_annotations_
NTSTATUS
GPIO_IO_CONNECTION::Initialize(
    WDFDEVICE _Device,
    ULONG _IdLowPart,
    ULONG _IdHighPart
    )
{
    Device = _Device;
    IdLowPart = _IdLowPart;
    IdHighPart = _IdHighPart;
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
GPIO_IO_CONNECTION::ExecuteIoOperation(
    GPIO_IO_OPERATION* IoOperation,
    PVOID InputBuffer,
    PVOID OutputBuffer
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES gpioAttributes;
    WDF_IO_TARGET_STATE gpioTargetState;
    WDFIOTARGET gpioTarget = NULL;
    BOOLEAN releaseGpioTarget = FALSE;
    UNICODE_STRING gpioString;
    WCHAR gpioStringBuffer[100];
    WDF_IO_TARGET_OPEN_PARAMS gpioOpenParams;
    WDF_OBJECT_ATTRIBUTES gpioRequestAttributes;
    WDFREQUEST gpioRequest = NULL;
    BOOLEAN releaseGpioRequest = FALSE;
    WDF_OBJECT_ATTRIBUTES gpioMemoryAttributes;
    WDFMEMORY gpioMemory = NULL;
    BOOLEAN releaseGpioMemory = FALSE;
    WDF_REQUEST_SEND_OPTIONS gpioSendOptions;

    FuncEntry(TRACE_FLAG_GPIO);

    WDF_OBJECT_ATTRIBUTES_INIT(&gpioAttributes);
    gpioAttributes.ParentObject = Device;
    status = WdfIoTargetCreate(
        Device,
        &gpioAttributes,
        &gpioTarget);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot create IOTARGET - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }
    releaseGpioTarget = TRUE;

    RtlInitEmptyUnicodeString(
        &gpioString,
        gpioStringBuffer,
        sizeof(gpioStringBuffer));
    status = RESOURCE_HUB_CREATE_PATH_FROM_ID(
        &gpioString,
        IdLowPart,
        IdHighPart);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot initialize PATH - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }
    else {
        Trace(
            TRACE_LEVEL_VERBOSE,
            TRACE_FLAG_GPIO_IO,
            "[%s] resource id (%d, %d)",
            IoOperation->GetName(),
            IdLowPart,
            IdHighPart
        );
    }

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
        &gpioOpenParams,
        &gpioString,
        IoOperation->DesiredAccess());
    status = WdfIoTargetOpen(
        gpioTarget,
        &gpioOpenParams);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot open IOTARGET - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&gpioRequestAttributes);
    status = WdfRequestCreate(
        &gpioRequestAttributes,
        gpioTarget,
        &gpioRequest);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot create REQUEST - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }
    releaseGpioRequest = TRUE;

    WDF_OBJECT_ATTRIBUTES_INIT(&gpioMemoryAttributes);
    gpioMemoryAttributes.ParentObject = gpioRequest;
    status = IoOperation->MemoryCreatePreallocated(
        &gpioMemoryAttributes,
        InputBuffer,
        OutputBuffer,
        &gpioMemory);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot create MEMORY - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }
    releaseGpioMemory = TRUE;

    status = IoOperation->IoTargetFormatRequest(
        gpioTarget,
        gpioRequest,
        gpioMemory);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot format MEMORY - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }

    WDF_REQUEST_SEND_OPTIONS_INIT(
        &gpioSendOptions,
        WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
    if (!WdfRequestSend(
        gpioRequest,
        gpioTarget,
        &gpioSendOptions))
    {
        status = WdfRequestGetStatus(gpioRequest);
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Sending REQUEST failed - %!STATUS!",
            IoOperation->GetName(),
            status);
        goto exit;
    }

exit:

    if (releaseGpioTarget)
    {
        gpioTargetState = WdfIoTargetGetState(gpioTarget);
        if (gpioTargetState == WdfIoTargetStarted)
        {
            WdfIoTargetPurge(
                gpioTarget,
                WdfIoTargetPurgeIoAndWait);
        }
    }

    if (releaseGpioMemory)
    {
        WdfObjectDelete(gpioMemory);
    }

    if (releaseGpioRequest)
    {
        WdfObjectDelete(gpioRequest);
    }

    if (releaseGpioTarget)
    {
        gpioTargetState = WdfIoTargetGetState(gpioTarget);
        if (gpioTargetState != WdfIoTargetClosed)
        {
            WdfIoTargetClose(gpioTarget);
        }
        WdfObjectDelete(gpioTarget);
    }

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
GPIO_IO_OPERATION::GPIO_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
GPIO_IO_OPERATION::~GPIO_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
GPIO_READ_IO_OPERATION::GPIO_READ_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
GPIO_READ_IO_OPERATION::~GPIO_READ_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
NTSTATUS
GPIO_READ_IO_OPERATION::RequestRetrieveInputBuffer(
    WDFREQUEST Request,
    PVOID* Buffer
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    size_t length;

    FuncEntry(TRACE_FLAG_GPIO);

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(GPIO_READ_REQUEST_IN),
        Buffer,
        &length);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot get InputBuffer - %!STATUS!",
            GetName(),
            status);
        goto exit;
    }
    if (length != sizeof(GPIO_READ_REQUEST_IN))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Expected InputBuffer size of %d[B] but got %d[B]",
            GetName(),
            sizeof(GPIO_READ_REQUEST_IN),
            static_cast<int>(length));
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

exit:
    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_READ_IO_OPERATION::RequestRetrieveOutputBuffer(
    WDFREQUEST Request,
    PVOID* Buffer
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    size_t length;

    FuncEntry(TRACE_FLAG_GPIO);

    status = WdfRequestRetrieveOutputBuffer(
        Request,
        sizeof(GPIO_READ_REQUEST_OUT),
        Buffer,
        &length);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot get OutputBuffer - %!STATUS!",
            GetName(),
            status);
        goto exit;
    }
    if (length != sizeof(GPIO_READ_REQUEST_OUT))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Expected OutputBuffer size of %d[B] but got %d[B]",
            GetName(),
            sizeof(GPIO_READ_REQUEST_OUT),
            static_cast<int>(length));
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

exit:
    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
ACCESS_MASK
GPIO_READ_IO_OPERATION::DesiredAccess(
    VOID
    )
{
    return FILE_GENERIC_READ;
}

_Use_decl_annotations_
NTSTATUS
GPIO_READ_IO_OPERATION::MemoryCreatePreallocated(
    PWDF_OBJECT_ATTRIBUTES Attributes,
    PVOID /* InputBuffer */,
    PVOID OutputBuffer,
    WDFMEMORY* Memory
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    FuncEntry(TRACE_FLAG_GPIO);

    if (!OutputBuffer)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Output buffer is null!",
            GetName());
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

    status = WdfMemoryCreatePreallocated(
        Attributes,
        static_cast<PVOID>(
        &(static_cast<PGPIO_READ_REQUEST_OUT>(OutputBuffer)->Value)),
        sizeof(GPIO_READ_REQUEST_VALUE),
        Memory);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot create WDFMEMORY",
            GetName());
        goto exit;
    }
exit:

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_READ_IO_OPERATION::IoTargetFormatRequest(
    WDFIOTARGET IoTarget,
    WDFREQUEST Request,
    WDFMEMORY Memory
    )
{
    return WdfIoTargetFormatRequestForIoctl(
        IoTarget,
        Request,
        IOCTL_GPIO_READ_PINS,
        NULL,
        0,
        Memory,
        0);
}

_Use_decl_annotations_
VOID
GPIO_READ_IO_OPERATION::RequestCompleteWithInformation(
    WDFREQUEST Request,
    NTSTATUS Status
    )
{
    WdfRequestCompleteWithInformation(
        Request,
        Status,
        static_cast<ULONG_PTR>(
        NT_SUCCESS(Status) 
        ? sizeof(GPIO_READ_REQUEST_OUT)
        : 0));
}

_Use_decl_annotations_
VOID
GPIO_READ_IO_OPERATION::TraceSuccessCompletion(
    _In_ PVOID InputBuffer,
    _In_opt_ PVOID OutputBuffer
    )
{
    PGPIO_READ_REQUEST_IN requestIn = static_cast<PGPIO_READ_REQUEST_IN>(InputBuffer);
    PGPIO_READ_REQUEST_OUT requestOut = static_cast<PGPIO_READ_REQUEST_OUT>(OutputBuffer);
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_IO,
        "[%s] Successfully read value of 0x%08lx from GPIO IO index = %d",
        GetName(),
        requestOut->Value,
        requestIn->GpioIndex);
}

_Use_decl_annotations_
PCHAR
GPIO_READ_IO_OPERATION::GetName(
    VOID
)
{
    return "ReadIoOperation";
}

_Use_decl_annotations_
GPIO_WRITE_IO_OPERATION::GPIO_WRITE_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
GPIO_WRITE_IO_OPERATION::~GPIO_WRITE_IO_OPERATION(
    VOID
    )
{
}

_Use_decl_annotations_
NTSTATUS
GPIO_WRITE_IO_OPERATION::RequestRetrieveInputBuffer(
    WDFREQUEST Request,
    PVOID* Buffer
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    size_t length;

    FuncEntry(TRACE_FLAG_GPIO);

    status = WdfRequestRetrieveInputBuffer(
        Request,
        sizeof(GPIO_WRITE_REQUEST_IN),
        Buffer,
        &length);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot get InputBuffer - %!STATUS!",
            GetName(),
            status);
        goto exit;
    }
    if (length != sizeof(GPIO_WRITE_REQUEST_IN))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Expected InputBuffer size of %d[B] but got %d[B]",
            GetName(),
            sizeof(GPIO_WRITE_REQUEST_IN),
            static_cast<int>(length));
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

exit:
    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_WRITE_IO_OPERATION::RequestRetrieveOutputBuffer(
    WDFREQUEST /* Request */,
    PVOID* Buffer
)
{
    *Buffer = NULL;
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
ACCESS_MASK
GPIO_WRITE_IO_OPERATION::DesiredAccess(
    VOID
)
{
    return FILE_GENERIC_WRITE;
}

_Use_decl_annotations_
NTSTATUS
GPIO_WRITE_IO_OPERATION::MemoryCreatePreallocated(
    PWDF_OBJECT_ATTRIBUTES Attributes,
    PVOID InputBuffer,
    PVOID /* OutputBuffer */,
    WDFMEMORY* Memory
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    FuncEntry(TRACE_FLAG_GPIO);

    if (!InputBuffer)
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Input buffer is null!",
            GetName());
        status = STATUS_INVALID_DEVICE_REQUEST;
        goto exit;
    }

    status = WdfMemoryCreatePreallocated(
        Attributes,
        static_cast<PVOID>(
        &(static_cast<PGPIO_WRITE_REQUEST_IN>(InputBuffer)->Value)),
        sizeof(GPIO_WRITE_REQUEST_VALUE),
        Memory);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO_IO,
            "[%s] Cannot create WDFMEMORY",
            GetName());
        goto exit;
    }
exit:

    FuncExit(TRACE_FLAG_GPIO);
    return status;
}

_Use_decl_annotations_
NTSTATUS
GPIO_WRITE_IO_OPERATION::IoTargetFormatRequest(
    WDFIOTARGET IoTarget,
    WDFREQUEST Request,
    WDFMEMORY Memory
    )
{
    return WdfIoTargetFormatRequestForIoctl(
        IoTarget,
        Request,
        IOCTL_GPIO_WRITE_PINS,
        Memory,
        0,
        Memory,
        0);
}

_Use_decl_annotations_
VOID
GPIO_WRITE_IO_OPERATION::RequestCompleteWithInformation(
    WDFREQUEST Request,
    NTSTATUS Status
    )
{
    WdfRequestCompleteWithInformation(
        Request,
        Status,
        0);
}

_Use_decl_annotations_
VOID
GPIO_WRITE_IO_OPERATION::TraceSuccessCompletion(
    _In_ PVOID InputBuffer,
    _In_opt_ PVOID /* OutputBuffer */
    )
{
    PGPIO_WRITE_REQUEST_IN requestIn = static_cast<PGPIO_WRITE_REQUEST_IN>(InputBuffer);
    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO_IO,
        "[%s] Successfully written value of 0x%08lx to GPIO IO index = %d",
        GetName(),
        requestIn->Value,
        requestIn->GpioIndex);
}

_Use_decl_annotations_
PCHAR
GPIO_WRITE_IO_OPERATION::GetName(
    VOID
    )
{
    return "WriteIoOperation";
}
