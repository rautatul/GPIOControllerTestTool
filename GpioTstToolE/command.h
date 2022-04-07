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
//#include "iaLPSS_GPIO_ext.h"

typedef struct CCommand CCommand, *PCCommand;

pair<ULONG, PBYTE>
ParseBuffer(
    _In_ list<string>           *Parameters,
    _In_ list<string>::iterator  Start
    );

VOID
PrintCommands();

struct CCommand 
{
public:

    long Index;
    string Type;
    list<string> *Parameters;

    //
    // File handle this command is being run against (if any).
    //

    HANDLE File;

    //
    // Overlapped structure for this command to use.
    //

    OVERLAPPED Overlapped;

    //
    // Handle to the thread for this command (if run asynchronously) 
    // The thread is signalled when the command is complete.
    //

    HANDLE Thread;

    //
    // Common parameters.
    //

    string Address;
    PBYTE  Buffer;

    static long s_Index;

    CCommand(
        _In_     string        Type,
        _In_     list<string> *Tokens
        ) : Type(Type),
            Thread(nullptr),
            Parameters(Tokens),
            Address(""),
            Buffer(nullptr),
            File(nullptr)
    {
        ZeroMemory(&Overlapped, sizeof(OVERLAPPED));
        Index = s_Index;
        s_Index += 1;
        return;
    }

    virtual
    ~CCommand(
        void
        )
    {
        delete[] Buffer;
        delete Parameters;
    }

    virtual
    bool
    Parse(
        void
        )
    {
        File = g_Peripheral;
        return true;
    }


    typedef 
    void
    (FN_PARSE)(
        _In_ list<string>::const_iterator &Iterator
        );

public:

    static
    PCCommand
    _ParseCommand(
        _In_ list<string> *Tokens
        );

public:

    VOID
    FakeCompletion(
        _In_    DWORD        Status,
        _In_    DWORD        Information
        )
    {
        Overlapped.Internal = Status == NO_ERROR ? Status : HRESULT_FROM_WIN32(Status);
        Overlapped.InternalHigh = Information;
        SetEvent(Overlapped.hEvent);
    }

    virtual
    bool
    Execute(
        VOID
        ) = 0;

    virtual
    void
    Complete(
        _In_ DWORD        Status,
        _In_ DWORD        Information
        )
    {
        printf("%s completed with status %d, information %d\n", 
               Type.c_str(),
               Status,
               Information);
    }

    virtual
    bool
    Cancel(
        VOID
        )
    {
        if (File != nullptr)
        {
            return CancelIoEx(File, &Overlapped) ? true : false;
        }
        else
        {
            return false;
        }
    }
};

class CInterruptCommand : public CCommand
{
protected:
    ULONG InterruptIndex;
    GPIO_INTERRUPT_IN InterruptIn;

public:

    CInterruptCommand(
        _In_ string Name,
        _In_ list<string> *Parameters,
        _In_opt_ string Tag
        ) : CCommand(Name, Parameters),
        InterruptIndex(0)
    {
        return;
    }

    bool
    Parse(
        void
        )
    {
        if (CCommand::Parse() == false)
        {
            return false;
        }
        
        if (PopNumberParameter(Parameters, 10, &InterruptIndex) == false)
        {
            printf("Interrupt Idx required\n");
            return false;
        }

        InterruptIn.InterruptIndex = InterruptIndex;

        return true;
    }
};

class CWaitOnInterruptCommand : public CInterruptCommand
{
public:
    CWaitOnInterruptCommand(
        _In_     list<string> *Parameters,
        _In_opt_ string        Tag
        ) : CInterruptCommand("intwait", Parameters, Tag)
    {
        return;
    }

    bool
    Execute(
        VOID
        );

    void
    Complete(
        _In_ DWORD        Status,
        _In_ DWORD        Information
        );
};

class CAcknowledgeInterruptCommand : public CInterruptCommand
{
public:
    CAcknowledgeInterruptCommand(
        _In_     list<string> *Parameters,
        _In_opt_ string        Tag
        ) : CInterruptCommand("intack", Parameters, Tag)
    {
        return;
    }

    bool
    Execute(
        VOID
        );

    void
    Complete(
        _In_ DWORD        Status,
        _In_ DWORD        Information
        );
};

class CIoCommand : public CCommand
{
protected:
    ULONG  GpioIndex;

public:
    CIoCommand(
        _In_    string Type,
        _In_     list<string> *Parameters,
        _In_opt_ string        Tag
        ) : CCommand(Type, Parameters),
        GpioIndex(0)
    {
        return;
    }

    bool
    Parse(
        void
        )
    {
        if (CCommand::Parse() == false)
        {
            return false;
        }
        
        if (PopNumberParameter(Parameters, 10, &GpioIndex) == false)
        {
            printf("GPIO Idx required\n");
            return false;
        }

        return true;
    }
};

class CIoReadCommand : public CIoCommand
{
protected:
    GPIO_READ_REQUEST_IN InBuffer;
    GPIO_READ_REQUEST_OUT OutBuffer;

public:
    CIoReadCommand(
        _In_     list<string> *Parameters,
        _In_opt_ string        Tag
        ) : CIoCommand("ioread", Parameters, Tag)
    {
        InBuffer.GpioIndex = 0;
        OutBuffer.Value = 0;
        return;
    }

    bool
    Parse(
        void
        )
    {
        if (__super::Parse() == false)
        {
            return false;
        }
        InBuffer.GpioIndex = GpioIndex;
        return true;
    }

    bool
        Execute(
        void
        );

    void
        Complete(
        _In_ DWORD        Status,
        _In_ DWORD        Information
        );
};

class CIoWriteCommand : public CIoCommand
{
protected:
    GPIO_WRITE_REQUEST_IN InBuffer;
    ULONG  GpioValue;

public:
    CIoWriteCommand(
        _In_     list<string> *Parameters,
        _In_opt_ string        Tag
        ) : CIoCommand("iowrite", Parameters, Tag),
        GpioValue(0)
    {
        InBuffer.GpioIndex = 0;
        return;
    }

    bool
    Parse(
        void
        )
    {
        if (__super::Parse() == false)
        {
            return false;
        }

        if (PopNumberParameter(Parameters, 10, &GpioValue) == false)
        {
            printf("GPIO value required\n");
            return false;
        }

        InBuffer.GpioIndex = GpioIndex;
        InBuffer.Value = GpioValue;

        return true;
    }

    bool
    Execute(
        void
        );

    void
    Complete(
        _In_ DWORD        Status,
        _In_ DWORD        Information
        );
};
