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

#include "new_delete.h"
#include "internal.h"
#include "device.h"
#include "io.h"
#include "interrupt.h"

#include "trace.h"
#include "device.tmh"

_Use_decl_annotations_
NTSTATUS
OnPrepareHardware(
    WDFDEVICE FxDevice,
    WDFCMRESLIST FxResourcesRaw,
    WDFCMRESLIST FxResourcesTranslated
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PDEVICE_CONTEXT pDevice = GetDeviceContext(FxDevice);
    NTSTATUS status = STATUS_SUCCESS;


    status = pDevice->GpioIo->Initialize(
        FxDevice,
        FxResourcesTranslated);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "Cannot initialize GPIO IO resources! - %!STATUS!",
            status);
        goto exit;
    }
    else {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "[AR] Initialize GPIO IO resources! - %!STATUS!",
            status);
    }

    if (!pDevice->ConnectInterrupt)
    {
        Trace(
            TRACE_LEVEL_INFORMATION,
            TRACE_FLAG_WDFLOADING,
            "No interrupt has been connected (default). Set 'ConnectInterrupt' value to dword:1 in Windows Registry");
        goto exit;
    }

    status = pDevice->GpioInterrupt->Initialize(
        FxDevice,
        FxResourcesTranslated,
        FxResourcesRaw);
    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "Cannot initialize GPIO Interrupt resources! - %!STATUS!",
            status);
        goto exit;
    }
    else {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_WDFLOADING,
            "[AR] Initialize GPIO Interrupt resources! - %!STATUS!",
            status);
    }

exit:

    FuncExit(TRACE_FLAG_WDFLOADING);
    return status;
}

_Use_decl_annotations_
NTSTATUS
OnReleaseHardware(
    WDFDEVICE Device,
    WDFCMRESLIST /* Resources */
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);
    PDEVICE_CONTEXT pDevice = GetDeviceContext(Device);

    if (pDevice->GpioInterrupt)
    {
        pDevice->GpioInterrupt->DeInitialize();
        delete pDevice->GpioInterrupt;
        pDevice->GpioInterrupt = NULL;
    }

    if (pDevice->GpioIo)
    {
        pDevice->GpioIo->DeInitialize();
        delete pDevice->GpioIo;
        pDevice->GpioIo = NULL;
    }

    FuncExit(TRACE_FLAG_WDFLOADING);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
VOID
OnFileCleanup(
    WDFFILEOBJECT FileObject
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PDEVICE_CONTEXT pDevice;

    pDevice = GetDeviceContext(
        WdfFileObjectGetDevice(FileObject));

    pDevice->GpioInterrupt->AcknowledgeAllInterrupts();

    FuncExit(TRACE_FLAG_WDFLOADING);
}

_Use_decl_annotations_
VOID
OnTopLevelIoDefault(
    WDFQUEUE    FxQueue,
    WDFREQUEST  FxRequest
    )
{
    FuncEntry(TRACE_FLAG_GPIO);
    
    PDEVICE_CONTEXT pDevice;
    WDF_REQUEST_PARAMETERS params;
    NTSTATUS status;

    pDevice = GetDeviceContext(
        WdfIoQueueGetDevice(FxQueue));

    WDF_REQUEST_PARAMETERS_INIT(&params);

    WdfRequestGetParameters(FxRequest, &params);

    if ((params.Type == WdfRequestTypeDeviceControl) &&
        (params.Parameters.DeviceIoControl.IoControlCode == 
            IOCTL_CVS_GPIO_WAIT_ON_INTERRUPT))
    {
        status = pDevice->GpioInterrupt->WaitForInterrupt(FxRequest);
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_GPIO,
                "Error occurecd when handling wait for interrupt request (%p) - %!STATUS!",
                FxRequest,
                status);
        }
        goto exit;
    }

    status = WdfRequestForwardToIoQueue(FxRequest, pDevice->IoctlQueue);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR,
            TRACE_FLAG_GPIO,
            "Failed to forward WDFREQUEST %p to SPB queue %p - %!STATUS!",
            FxRequest,
            pDevice->IoctlQueue,
            status);

        WdfRequestComplete(FxRequest, status);
        goto exit;
    }

exit:

    FuncExit(TRACE_FLAG_GPIO);
}

_Use_decl_annotations_
VOID
OnIoDeviceControl(
    WDFQUEUE FxQueue,
    WDFREQUEST FxRequest,
    size_t /* OutputBufferLength */,
    size_t /* InputBufferLength */,
    ULONG IoControlCode
    )
{
    FuncEntry(TRACE_FLAG_GPIO);

    PDEVICE_CONTEXT pDevice;

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_FLAG_GPIO,
        "DeviceIoControl request (%p) received with IOCTL=%lu",
        FxRequest,
        IoControlCode);
    
    pDevice = GetDeviceContext(
        WdfIoQueueGetDevice(FxQueue));

    switch (IoControlCode)
    {
    case IOCTL_CVS_GPIO_IO_READ:
        pDevice->GpioIo->Read(FxRequest);
        break;

    case IOCTL_CVS_GPIO_IO_WRITE:
        pDevice->GpioIo->Write(FxRequest);
        break;

    case IOCTL_CVS_GPIO_ACKNOWLEDGE_INTERRUPT:
        pDevice->GpioInterrupt->AcknowledgeInterrupt(FxRequest);
        break;

    default:
        Trace(
            TRACE_LEVEL_WARNING,
            TRACE_FLAG_GPIO,
            "[AR] Request %p received with unexpected IOCTL=%lu",
            FxRequest,
            IoControlCode);
        WdfRequestComplete(FxRequest, STATUS_INVALID_DEVICE_REQUEST);
    }

    FuncExit(TRACE_FLAG_GPIO);
}
