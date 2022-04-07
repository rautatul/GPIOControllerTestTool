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

#ifndef _IO_H_
#define _IO_H_

#include <ntddk.h>
#include <wdf.h>
#include "new_delete.h"

typedef struct _DEVICE_CONTEXT DEVICE_CONTEXT, *PDEVICE_CONTEXT;

class GPIO_IO_CONNECTION;
class GPIO_IO_OPERATION;

class GPIO_IO : public BaseAllocator
{
private:

    _Field_size_full_opt_(ConnectionsCount)
    GPIO_IO_CONNECTION* Connections;
    _Field_range_(0, MAXULONG)
    ULONG ConnectionsCount;
    _Field_size_full_opt_(1)
    GPIO_IO_OPERATION* ReadIoOperation;
    _Field_size_full_opt_(1)
    GPIO_IO_OPERATION* WriteIoOperation;

protected:

    NTSTATUS
    Execute(
        _In_ WDFREQUEST Request,
        _In_ GPIO_IO_OPERATION* IoOperation
        );

public:

    GPIO_IO(
        VOID
        );

    ~GPIO_IO(
        VOID
        );

    NTSTATUS
    Initialize(
        _In_ WDFDEVICE Device,
        _In_ WDFCMRESLIST Resources
        );

    VOID
    DeInitialize(
        VOID
        );

    NTSTATUS
    Read(
        _In_ WDFREQUEST Request
        );

    NTSTATUS
    Write(
        _In_ WDFREQUEST Request
        );

private:

    GPIO_IO(
        _In_ const GPIO_IO&
        );

    GPIO_IO&
    operator = (
        _In_ const GPIO_IO&
        );

};

class GPIO_IO_CONNECTION : public BaseAllocator
{
private:

    ULONG IdLowPart;
    ULONG IdHighPart;
    WDFDEVICE Device;

public:

    GPIO_IO_CONNECTION(
        VOID
        );

    ~GPIO_IO_CONNECTION(
        VOID
        );

    NTSTATUS
    Initialize(
        _In_ WDFDEVICE Device,
        _In_ ULONG IdLowPart,
        _In_ ULONG IdHighPart
        );

    NTSTATUS
    ExecuteIoOperation(
        _In_ GPIO_IO_OPERATION* IoOperation,
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer
        );

private:

    GPIO_IO_CONNECTION(
        _In_ const GPIO_IO_CONNECTION&
        );

    GPIO_IO_CONNECTION&
    operator = (
        _In_ const GPIO_IO_CONNECTION&
        );
};

class GPIO_IO_OPERATION : public BaseAllocator
{
public:

    GPIO_IO_OPERATION(
        VOID
        );

    virtual
    ~GPIO_IO_OPERATION(
        VOID
        );

    virtual
    NTSTATUS
    RequestRetrieveInputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        ) = 0;

    virtual
    NTSTATUS
    RequestRetrieveOutputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        ) = 0;

    virtual
    ACCESS_MASK
    DesiredAccess(
        VOID
        ) = 0;

    virtual
    NTSTATUS
    MemoryCreatePreallocated(
        _In_ PWDF_OBJECT_ATTRIBUTES Attributes,
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer,
        _Out_ WDFMEMORY* Memory
        ) = 0;

    virtual
    NTSTATUS
    IoTargetFormatRequest(
        _In_ WDFIOTARGET IoTarget,
        _In_ WDFREQUEST Request,
        _In_ WDFMEMORY Memory
        ) = 0;

    virtual
    VOID
    RequestCompleteWithInformation(
        _In_ WDFREQUEST Request,
        _In_ NTSTATUS Status
        ) = 0;

    virtual
    VOID
    TraceSuccessCompletion(
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer
        ) = 0;

    virtual
    PCHAR
    GetName(
        VOID
        ) = 0;

private:

    GPIO_IO_OPERATION(
        _In_ const GPIO_IO_OPERATION&
        );

    GPIO_IO_OPERATION&
    operator = (
        _In_ const GPIO_IO_OPERATION&
        );
};

class GPIO_READ_IO_OPERATION
    :
    public GPIO_IO_OPERATION
{
public:

    GPIO_READ_IO_OPERATION(
        VOID
        );

    virtual
    ~GPIO_READ_IO_OPERATION(
        VOID
        );

    virtual
    NTSTATUS
    RequestRetrieveInputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        );

    virtual
    NTSTATUS
    RequestRetrieveOutputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        );

    virtual
    ACCESS_MASK
    DesiredAccess(
        VOID
        );

    virtual
    NTSTATUS
    MemoryCreatePreallocated(
        _In_ PWDF_OBJECT_ATTRIBUTES Attributes,
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer,
        _Out_ WDFMEMORY* Memory
        );

    virtual
    NTSTATUS
    IoTargetFormatRequest(
        _In_ WDFIOTARGET IoTarget,
        _In_ WDFREQUEST Request,
        _In_ WDFMEMORY Memory
        );

    virtual
    VOID
    RequestCompleteWithInformation(
        _In_ WDFREQUEST Request,
        _In_ NTSTATUS Status
        );

    virtual
    VOID
    TraceSuccessCompletion(
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer
        );

    virtual
    PCHAR
    GetName(
        VOID
        );

private:

    GPIO_READ_IO_OPERATION(
        _In_ const GPIO_READ_IO_OPERATION&
        );

    GPIO_READ_IO_OPERATION&
    operator = (
        _In_ const GPIO_READ_IO_OPERATION&
        );
};

class GPIO_WRITE_IO_OPERATION : public GPIO_IO_OPERATION
{
public:

    GPIO_WRITE_IO_OPERATION(
        VOID
        );

    virtual
    ~GPIO_WRITE_IO_OPERATION(
        VOID
        );

    virtual
    NTSTATUS
    RequestRetrieveInputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        );

    virtual
    NTSTATUS
    RequestRetrieveOutputBuffer(
        _In_ WDFREQUEST Request,
        _Outptr_ PVOID* Buffer
        );

    virtual
    ACCESS_MASK
    DesiredAccess(
        VOID
        );

    virtual
    NTSTATUS
    MemoryCreatePreallocated(
        _In_ PWDF_OBJECT_ATTRIBUTES Attributes,
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer,
        _Out_ WDFMEMORY* Memory
        );

    virtual
    NTSTATUS
    IoTargetFormatRequest(
        _In_ WDFIOTARGET IoTarget,
        _In_ WDFREQUEST Request,
        _In_ WDFMEMORY Memory
        );

    virtual
    VOID
    RequestCompleteWithInformation(
        _In_ WDFREQUEST Request,
        _In_ NTSTATUS Status
        );

    virtual
    VOID
    TraceSuccessCompletion(
        _In_ PVOID InputBuffer,
        _In_opt_ PVOID OutputBuffer
        );

    virtual
    PCHAR
    GetName(
        VOID
        );

private:

    GPIO_WRITE_IO_OPERATION(
        _In_ const GPIO_WRITE_IO_OPERATION&
        );

    GPIO_WRITE_IO_OPERATION&
    operator = (
        _In_ const GPIO_WRITE_IO_OPERATION&
        );
};

#endif // _IO_H_
