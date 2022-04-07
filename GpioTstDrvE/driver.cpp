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
#include "driver.h"
#include "device.h"
#include "io.h"
#include "interrupt.h"
#include <ntstrsafe.h>

#include "driver.tmh"

_Use_decl_annotations_
NTSTATUS
#pragma prefast(suppress:__WARNING_DRIVER_FUNCTION_TYPE, "thanks, i know this already")
DriverEntry(
    PDRIVER_OBJECT  DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG driverConfig;
    WDF_OBJECT_ATTRIBUTES driverAttributes;

    WDFDRIVER fxDriver;

    NTSTATUS status;

    WPP_INIT_TRACING(DriverObject, RegistryPath);

    FuncEntry(TRACE_FLAG_WDFLOADING);

    WDF_DRIVER_CONFIG_INIT(&driverConfig, OnDeviceAdd);
    driverConfig.DriverPoolTag = GPIO_POOL_TAG;

    WDF_OBJECT_ATTRIBUTES_INIT(&driverAttributes);
    driverAttributes.EvtCleanupCallback = OnDriverCleanup;
    driverAttributes.EvtDestroyCallback = OnDriverDestroy;

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        &driverAttributes,
        &driverConfig,
        &fxDriver);

    if (!NT_SUCCESS(status))
    {
        Trace(
            TRACE_LEVEL_ERROR, 
            TRACE_FLAG_WDFLOADING,
            "Error creating WDF driver object - %!STATUS!", 
            status);

        goto exit;
    }

    Trace(
        TRACE_LEVEL_VERBOSE, 
        TRACE_FLAG_WDFLOADING,
        "Created WDF driver object");

exit:

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}

_Use_decl_annotations_
VOID
OnDriverCleanup(
     WDFOBJECT Object
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    UNREFERENCED_PARAMETER(Object);

    WPP_CLEANUP(nullptr);

    FuncExit(TRACE_FLAG_WDFLOADING);
}

_Use_decl_annotations_
VOID
OnDriverDestroy(
    WDFOBJECT Object
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);
    PDEVICE_CONTEXT pDevice = GetDeviceContext(Object);
    if (!pDevice)
    {
        goto exit;
    }

    if (pDevice->GpioInterrupt)
    {
        delete pDevice->GpioInterrupt;
        pDevice->GpioInterrupt = NULL;
    }

    if (pDevice->GpioIo)
    {
        delete pDevice->GpioIo;
        pDevice->GpioIo = NULL;
    }

exit:
    FuncExit(TRACE_FLAG_WDFLOADING);
}

_Use_decl_annotations_
NTSTATUS
OnDeviceAdd(
    WDFDRIVER /* FxDriver */,
    PWDFDEVICE_INIT FxDeviceInit
    )
{
    FuncEntry(TRACE_FLAG_WDFLOADING);

    PDEVICE_CONTEXT pDevice;
    NTSTATUS status;

    {
        WDF_PNPPOWER_EVENT_CALLBACKS pnpCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpCallbacks);
        pnpCallbacks.EvtDevicePrepareHardware = OnPrepareHardware;
        pnpCallbacks.EvtDeviceReleaseHardware = OnReleaseHardware;
        WdfDeviceInitSetPnpPowerEventCallbacks(FxDeviceInit, &pnpCallbacks);
    }

    {
        WDF_FILEOBJECT_CONFIG fileObjectConfig;

        WDF_FILEOBJECT_CONFIG_INIT(
            &fileObjectConfig, 
            nullptr, 
            nullptr,
            OnFileCleanup);

        WDF_OBJECT_ATTRIBUTES fileObjectAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT(&fileObjectAttributes);

        WdfDeviceInitSetFileObjectConfig(
            FxDeviceInit, 
            &fileObjectConfig,
            &fileObjectAttributes);
    }

    //
    // Create the device.
    //
    {
        WDFDEVICE fxDevice;
        WDF_OBJECT_ATTRIBUTES deviceAttributes;
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);

        status = WdfDeviceCreate(
            &FxDeviceInit, 
            &deviceAttributes,
            &fxDevice);

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Error creating WDFDEVICE - %!STATUS!", 
                status);

            goto exit;
        }

        pDevice = GetDeviceContext(fxDevice);
        NT_ASSERT(pDevice != nullptr);

        pDevice->FxDevice = fxDevice;
        pDevice->GpioIo = NULL;
    }

    //
    // Ensure device is disable-able
    //
    {
        WDF_DEVICE_STATE deviceState;
        WDF_DEVICE_STATE_INIT(&deviceState);
        
        deviceState.NotDisableable = WdfFalse;
        WdfDeviceSetDeviceState(pDevice->FxDevice, &deviceState);
    }

    //
    // Create queues to handle IO
    //
    {
        WDF_IO_QUEUE_CONFIG queueConfig;
        WDFQUEUE queue;

        //
        // Top-level queue
        //

        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
            &queueConfig, 
            WdfIoQueueDispatchParallel);

        queueConfig.EvtIoDefault = OnTopLevelIoDefault;

        status = WdfIoQueueCreate(
            pDevice->FxDevice,
            &queueConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &queue
            );

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Error creating top-level IO queue - %!STATUS!", 
                status);

            goto exit;
        }

        WDF_IO_QUEUE_CONFIG_INIT(
            &queueConfig, 
            WdfIoQueueDispatchSequential);
        queueConfig.EvtIoDeviceControl = OnIoDeviceControl;

        status = WdfIoQueueCreate(
            pDevice->FxDevice,
            &queueConfig,
            WDF_NO_OBJECT_ATTRIBUTES,
            &pDevice->IoctlQueue
            );

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Error creating IOCTL queue - %!STATUS!", 
                status);

            goto exit;
        }
    }

    //
    // Create a symbolic link.
    //
    {
        PDEVICE_OBJECT temp;
        WCHAR name[256];
        PWCHAR pName = name;
        ULONG propertySize;
        DEVICE_REGISTRY_PROPERTY registryProperty = DevicePropertyHardwareID;
        name[0] = NULL;

        temp = WdfDeviceWdmGetPhysicalDevice(
            pDevice->FxDevice);

        status = IoGetDeviceProperty(
            temp,
            registryProperty,
            sizeof(name), 
            name, 
            &propertySize);
        
        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Cannot get device property: 0x%x"
                "- %!STATUS!",
                registryProperty,
                status);

            goto exit;
        }
        DECLARE_UNICODE_STRING_SIZE(symbolicLinkName, 128);

        while (wcslen(pName) > 0)
        {
                Trace(
                TRACE_LEVEL_VERBOSE, 
                TRACE_FLAG_WDFLOADING,
                "HardwareID: %ws",
                pName);

                // drop "ACPI\VEN_*" and search for "ACPI\..."
                if ((wcsstr(pName, L"ACPI\\VEN_") == NULL) &&
                    (wcsstr(pName, L"ACPI\\") == pName))
                {
                    pName += wcslen(L"ACPI\\");
                    break;
                }
                pName += wcslen(pName) + 1;
        }

        if (wcslen(pName) == 0)
        {
            status = STATUS_INVALID_PARAMETER;
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Appropriate HardwareID not found");
            goto exit;
        } 
        else
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "HardwareID used: %ws",
                pName);
        }
        status = RtlUnicodeStringPrintf(
            &symbolicLinkName, L"%ws%ws",
            CVF_GPIO_SYMBOLIC_NAME, 
            pName);


        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Error creating symbolic link string for device %ws"
                "- %!STATUS!",
                pName,
                status);

            goto exit;
        }
        status = WdfDeviceCreateSymbolicLink(
            pDevice->FxDevice, 
            &symbolicLinkName);

        if (!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR, 
                TRACE_FLAG_WDFLOADING,
                "Error creating symbolic link for device "
                "- %!STATUS!", 
                status);

            goto exit;
        }
        else
        {
             Trace(
                TRACE_LEVEL_VERBOSE, 
                TRACE_FLAG_WDFLOADING,
                "device has symbolic link %!USTR!",
                &symbolicLinkName);
        }
    }

    //
    // Retrieve registry settings.
    //
    {
        WDFKEY key = NULL;
        WDFKEY subkey = NULL;
        ULONG connectInterrupt = 0;
        NTSTATUS settingStatus;
    
        DECLARE_CONST_UNICODE_STRING(subkeyName, L"Settings");
        DECLARE_CONST_UNICODE_STRING(connectInterruptName, L"ConnectInterrupt");

        settingStatus = WdfDeviceOpenRegistryKey(
            pDevice->FxDevice,
            PLUGPLAY_REGKEY_DEVICE,
            KEY_READ,
            WDF_NO_OBJECT_ATTRIBUTES,
            &key);

        if (!NT_SUCCESS(settingStatus))
        {
            Trace(
                TRACE_LEVEL_WARNING, 
                TRACE_FLAG_WDFLOADING,
                "Error opening device registry key - %!STATUS!",
                settingStatus);
            goto exit;
        }

        settingStatus = WdfRegistryOpenKey(
            key,
            &subkeyName,
            KEY_READ,
            WDF_NO_OBJECT_ATTRIBUTES,
            &subkey);

        if (!NT_SUCCESS(settingStatus))
        {
            Trace(
                TRACE_LEVEL_WARNING, 
                TRACE_FLAG_WDFLOADING,
                "Error opening registry subkey for 'Settings' - %!STATUS!", 
                settingStatus);
            goto exit;
        }

        settingStatus = WdfRegistryQueryULong(
            subkey,
            &connectInterruptName,
            &connectInterrupt);

        if (!NT_SUCCESS(settingStatus))
        {
            Trace(
                TRACE_LEVEL_WARNING, 
                TRACE_FLAG_WDFLOADING,
                "Error querying registry value for 'ConnectInterrupt' - %!STATUS!", 
                settingStatus);
            goto exit;
        }

        if (key != NULL)
        {
            WdfRegistryClose(key);
        }

        if (subkey != NULL)
        {
            WdfRegistryClose(subkey);
        }

        pDevice->ConnectInterrupt = (connectInterrupt == 1);
    }

    {
        pDevice->GpioIo = new GPIO_IO;
        if (!pDevice->GpioIo)
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_WDFLOADING,
                "Cannot allocate memory for IO!");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
    }


    {
        pDevice->GpioInterrupt = new GPIO_INTERRUPT;
        if (!pDevice->GpioInterrupt)
        {
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_WDFLOADING,
                "Cannot allocate memory for Interrupts!");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        }
    }
exit:

    FuncExit(TRACE_FLAG_WDFLOADING);

    return status;
}
