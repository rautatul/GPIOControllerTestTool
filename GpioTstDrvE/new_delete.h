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

#pragma once

#include <ntddk.h>

#define GPIO_TAG 'oipg'

FORCEINLINE
void*
__cdecl
operator new (size_t Size)
{
    PVOID result = ExAllocatePoolWithTag(
        NonPagedPool,
        Size,
        GPIO_TAG);

    if (result)
    {
        RtlZeroMemory(
            result,
            Size);
    }

    return result;
}

FORCEINLINE
void*
__cdecl
operator new [] (size_t Size)
{
    PVOID result = ExAllocatePoolWithTag(
        NonPagedPool,
        Size,
        GPIO_TAG
    );

    if (result)
    {
        RtlZeroMemory(
            result,
            Size);
    }

    return result;
}

FORCEINLINE
VOID
__cdecl
operator delete (void* ptr)
{
    if (ptr != NULL)
    {
        ExFreePoolWithTag(ptr, GPIO_TAG);
    }
}

FORCEINLINE
void
__cdecl
operator delete [] (void* ptr)
{
    if (ptr != NULL)
    {
        ExFreePoolWithTag(ptr, GPIO_TAG);
    }
}

class BaseAllocator
{
public:
    void* operator new(size_t size)
    {
        PVOID result = ExAllocatePoolWithTag(
            NonPagedPool,
            size,
            GPIO_TAG);

        if (result)
        {
            RtlZeroMemory(
                result,
                size);
        }

        return result;
    }

    void* operator new[] (size_t size)
    {
        PVOID result = ExAllocatePoolWithTag(
            NonPagedPool,
            size,
            GPIO_TAG
        );

        if (result)
        {
            RtlZeroMemory(
                result,
                size);
        }

        return result;
    }

    void operator delete (void* ptr)
    {
        if (ptr != NULL)
        {
            ExFreePoolWithTag(ptr, GPIO_TAG);
        }
    }

    void operator delete [] (void* ptr)
    {
        if (ptr != NULL)
        {
            ExFreePoolWithTag(ptr, GPIO_TAG);
        }
    }
};