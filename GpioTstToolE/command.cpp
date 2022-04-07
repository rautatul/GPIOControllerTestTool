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

#include "internal.h"

extern set<ULONG> g_EnabledInterrupts;
long CCommand::s_Index = 1;

VOID
PrintCommands()
{
    printf("\n");
    printf("Commands:\n");
    printf("  ioread <idx>             reads GPIO resource value\n");
    printf("  iowrite <idx> <value>    writes GPIO resource value\n");
    printf("  intwait <idx>            waits for interrupt to happen\n");
    printf("  intack <idx>             acknowledges the interrupt\n");
    printf("  help                     print command list\n");
    printf("  wait                     wait one second\n");
    printf("\n");
}

PCCommand
CCommand::_ParseCommand(
    _In_ list<string> *Parameters
    )
{
    string tag;
    string name;
    
    PCCommand command = nullptr;

    if (Parameters->front()[0] == L'@')
    {
        if (PopStringParameter(Parameters, &tag) == false)
        {
            printf("Error - could not pop tag\n");
            return nullptr;
        }
    }

    if (PopStringParameter(Parameters, &name) == false)
    {
        return nullptr;
    }

    if(_stricmp(name.c_str(), "ioread") == 0)
    {
        command = new CIoReadCommand(Parameters, tag);
    }
    else if(_stricmp(name.c_str(), "iowrite") == 0)
    {
        command = new CIoWriteCommand(Parameters, tag);
    }
    else if(_stricmp(name.c_str(), "intwait") == 0)
    {
        command = new CWaitOnInterruptCommand(Parameters, tag);
    }
    else if(_stricmp(name.c_str(), "intack") == 0)
    {
        command = new CAcknowledgeInterruptCommand(Parameters, tag);
    }
    else if(_stricmp(name.c_str(), "help") == 0)
    {
        PrintCommands();
        return nullptr;
    }
    else if (_stricmp(name.c_str(), "wait") == 0)
    {
        //PrintCommands();
        printf(".............................................\n");
        wait(1);
        return nullptr;
    }
    else if (_stricmp(name.c_str(), "prt") == 0)
    {
        printf(".....\n");
        return nullptr;
    }
    else
    {
        printf("unrecognized command %s\n", name.c_str());
        PrintCommands();
        return nullptr;
    }

    if (command->Parse() == false)
    {
        delete command;
        return nullptr;
    }

    return command;
}

bool
CWaitOnInterruptCommand::Execute(
    VOID
    )
{
    ULONG bytesReturned;

    if (File == nullptr)
    {
        return false;
    }
    
    if ((DeviceIoControl(
        File, 
        IOCTL_CVS_GPIO_WAIT_ON_INTERRUPT,
        &InterruptIn,
        sizeof(GPIO_INTERRUPT_IN),
        NULL,
        0,
        &bytesReturned,
        &Overlapped) == TRUE) || 
        (GetLastError() != ERROR_IO_PENDING))
    {
        FakeCompletion(GetLastError(), bytesReturned);
    }

    return true;
}

void
CWaitOnInterruptCommand::Complete(
    _In_ DWORD        Status,
    _In_ DWORD        /* Information */
    )
{
    if (Status != NO_ERROR)
    {
        printf("Error while waiting for interupt %d - status %d\n", InterruptIndex, GetLastError());
    }
    else
    {
        printf("Interrupt %d wait has finished. Remember to acknowledge it!\n", InterruptIn.InterruptIndex);
    }
}

bool
CAcknowledgeInterruptCommand::Execute(
    VOID
    )
{
    ULONG bytesReturned;

    if (File == nullptr)
    {
        return false;
    }
    
    if ((DeviceIoControl(
        File, 
        IOCTL_CVS_GPIO_ACKNOWLEDGE_INTERRUPT,
        &InterruptIn,
        sizeof(GPIO_INTERRUPT_IN),
        NULL,
        0,
        &bytesReturned,
        &Overlapped) == TRUE) || 
        (GetLastError() != ERROR_IO_PENDING))
    {
        FakeCompletion(GetLastError(), bytesReturned);
    }

    return true;
}

void
CAcknowledgeInterruptCommand::Complete(
    _In_ DWORD        Status,
    _In_ DWORD        /* Information */
    )
{
    if (Status != NO_ERROR)
    {
        printf("Error while acknowledging %d - status %d\n", InterruptIndex, GetLastError());
    }
    else
    {
        printf("Interrupt %d wait has been acknowledged\n", InterruptIn.InterruptIndex);
    }
}

bool
CIoReadCommand::Execute(
    VOID
    )
{
    ULONG bytesReturned;

    if (File == nullptr)
    {
        return false;
    }
    
    if ((DeviceIoControl(
        File, 
        IOCTL_CVS_GPIO_IO_READ,
        &InBuffer,
        sizeof(GPIO_READ_REQUEST_IN),
        &OutBuffer,
        sizeof(GPIO_READ_REQUEST_OUT),
        &bytesReturned,
        &Overlapped) == TRUE) || 
        (GetLastError() != ERROR_IO_PENDING))
    {
        FakeCompletion(GetLastError(), bytesReturned);
    }

    return true;
}

void
CIoReadCommand::Complete(
    _In_ DWORD        Status,
    _In_ DWORD        /* Information */
    )
{
    if (Status != NO_ERROR)
    {
        printf("Error reading from GPIO peripheral %d - status %d\n", GpioIndex, GetLastError());
    }
    else
    {
        printf("GPIO preipheral %d line read state = %d\n", InBuffer.GpioIndex, OutBuffer.Value);
    }
}

bool
CIoWriteCommand::Execute(
    VOID
    )
{
    ULONG bytesReturned;

    if (File == nullptr)
    {
        return false;
    }
    
    if ((DeviceIoControl(
        File, 
        IOCTL_CVS_GPIO_IO_WRITE,
        &InBuffer,
        sizeof(GPIO_WRITE_REQUEST_IN),
        nullptr,
        0,
        &bytesReturned,
        &Overlapped) == TRUE) || 
        (GetLastError() != ERROR_IO_PENDING))
    {
        FakeCompletion(GetLastError(), bytesReturned);
    }

    return true;
}

void
CIoWriteCommand::Complete(
    _In_ DWORD        Status,
    _In_ DWORD        /* Information */
    )
{
    if (Status != NO_ERROR)
    {
        printf("Error writing to GPIO peripheral %d - status %d\n", GpioIndex, GetLastError());
    }
    else
    {
        printf("GPIO preipheral %d line changed to = %d\n", InBuffer.GpioIndex, InBuffer.Value);
    }
}
