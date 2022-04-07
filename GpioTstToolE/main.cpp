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

bool
ReadCommandFromStream(
    _In_    FILE*         InputStream,
    _Inout_ list<string> *Tokens
    );

VOID
PrintUsage(
    _In_ PCSTR exeName
    );

DWORD
RunCommand(
    _In_ PCCommand Command
    );


HANDLE g_Peripheral = nullptr;
HANDLE g_Event = nullptr;
PCCommand g_CurrentCommand = nullptr;

BOOL
#pragma prefast(suppress: __WARNING_UNMATCHED_DEFN, "WIN8:497586 false positive for prefast 28251")
WINAPI
OnControlKey(
    _In_ DWORD ControlType
    );

void 
__cdecl 
main(
    _In_                     ULONG ArgumentsCe,
    _In_reads_(ArgumentsCe) PCSTR Arguments[]
    )
{
    FILE* inputStream = stdin;
    bool prompt = true;

    PCSTR peripheralPath = nullptr;
    PCSTR inputPath = nullptr;

    //
    // Parse the command line arguments.
    //

    ULONG arg = 1;

    while (arg < ArgumentsCe)
    {
        if ((Arguments[arg][0] != '/') &&
            (Arguments[arg][0] != '-'))
        {
            PrintUsage(Arguments[0]);
            goto exit;
        }
        else
        {
            if (tolower(Arguments[arg][1]) == 'p')
            {
                arg++;
                if (arg == ArgumentsCe)
                {
                    PrintUsage(Arguments[0]);
                    goto exit;
                }
                peripheralPath = Arguments[arg];
            }
            else if (tolower(Arguments[arg][1]) == 'i')
            {
                arg++;
                if (arg == ArgumentsCe)
                {
                    PrintUsage(Arguments[0]);
                    goto exit;
                }
                inputPath = Arguments[arg];
            }
            else
            {
                PrintUsage(Arguments[0]);
                goto exit;
            }
        }

        arg++;
    }

    //
    // Open the input file if specified
    //

    if (inputPath != nullptr)
    {
        errno_t error;

        printf("Opening %s as command input file\n", inputPath);
        error = fopen_s(&inputStream, inputPath, "r");

        if (error != 0)
        {
            printf("Error opening input file %s - %d\n", inputPath, error);
            goto exit;
        }
        prompt = false;
    }

    //
    // Open peripheral driver
    //

    if (peripheralPath == nullptr)
    {
        g_Peripheral = CreateFileW(
            CVF_GPIO_USERMODE_PATH,
            (GENERIC_READ | GENERIC_WRITE),
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);
    }
    else
    {
        printf("Opening %s as peripheral driver path \n", peripheralPath);
        g_Peripheral = CreateFileA(
            peripheralPath,
            (GENERIC_READ | GENERIC_WRITE),
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            nullptr);
    }

    if (g_Peripheral == INVALID_HANDLE_VALUE)
    {
        printf("Error opening peripheral driver - %d\n", GetLastError());
        goto exit;
    }

    setvbuf(inputStream, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    //
    // Setup a control-C handler.
    //

    if (SetConsoleCtrlHandler(OnControlKey, true) == FALSE)
    {
        printf("Error setting ctrl-C handler - %d\n", GetLastError());
        goto exit;
    }

    //
    // Setup an overlapped structure to use with each I/O.
    //

    g_Event = CreateEvent(nullptr, true, false, nullptr); 
    
    if (g_Event == nullptr)
    {
        printf("error creating I/O event - %d\n", GetLastError());
        goto exit;
    }

    //
    // Loop reading commands off the command line and parsing them.
    // EOF causes an exit.
    //

    do 
    {
        list<string> *tokens = new list<string>();
        PCCommand command;

        if (prompt)
        {
            printf("> ");
            fflush(stdout);
        }

        if (ReadCommandFromStream(inputStream, tokens) == false)
        {
            delete tokens;
            break;
        }

        if (tokens->empty())
        {
            delete tokens;
            continue;
        }

        command = CCommand::_ParseCommand(tokens);

        if (command == nullptr) 
        {
            continue;
        }

        g_CurrentCommand = command;
            
        RunCommand(command);

        g_CurrentCommand = nullptr;
            
        delete command;

    }
    while (feof(inputStream) == 0);

exit:

    if (prompt == false)
    {
        fclose(inputStream);
    }

    CloseHandle(g_Peripheral);
    CloseHandle(g_Event);

    return;
}

VOID
PrintUsage(
    _In_ PCSTR exeName
    )
{
    printf("Usage: %s [/p <driver_path>] [/i <script_name>]\n",
        exeName);
}

DWORD
RunCommand(
    _In_ PCCommand Command
    )
{
    HANDLE h = nullptr;

    DWORD status;
    DWORD bytesTransferred;

    Command->Overlapped.hEvent = g_Event;
    
    if (Command->Execute() == true)
    {
        if (GetOverlappedResult(Command->File, 
                                &(Command->Overlapped),
                                &bytesTransferred,
                                true) == FALSE)
        {
            status = GetLastError();
        }
        else
        {
            status = NO_ERROR;
        }

        Command->Complete(status, bytesTransferred);
    }
    else
    {
        status = GetLastError();
    }

    if (h != nullptr)
    {
        CloseHandle(h);
    }

    return status;
}


bool
ReadLine(
    _In_                    FILE* InputStream,
    _In_                    ULONG BufferCch,
    _Out_writes_(BufferCch) CHAR  Buffer[]
    )
{
    ULONG i;

    for(i = 0; i < BufferCch - 1; i += 1)
    {
        if (fread(&(Buffer[i]), sizeof(CHAR), 1, InputStream) == 0)
        {
           return false;
        } 

        fputc(Buffer[i], stdout);

        if (Buffer[i] == '\n')
        {
            Buffer[i] = '\0';
            return true;
        }
        else if (Buffer[i] == '\b')
        {
            Buffer[i] = '\0';
            i -= 2;
        }
    }

    return false;
}

bool
ReadCommandFromStream(
    _In_    FILE*         InputStream,
    _Inout_ list<string> *Tokens
    )
{   
    CHAR buffer[20 * 1024];

    if (ReadLine(InputStream, 
                 sizeof(buffer) - 1,
                 buffer) == false)
    {
        return false;
    }

    string currentLine(buffer);
    string token;

    list<string> tokens;

    Tokens->clear();

    string::size_type i;
    long start;

    for(i = 0, start = -1; 
        i < currentLine.length() + 1; 
        i += 1)
    {
        wchar_t c;
        
        if (i < currentLine.length()) 
        {
            c = currentLine[i];
        }
        else
        {
            c = L'\0';
        }

        if (iswspace(c) || (c == L'\0'))
        {
            if (start == -1)
            {
                //
                // not tracking a token - whitespace is skipped.
                //
            }
            else
            {
                //
                // tracking a token - whitespace or NUL terminates it.
                // if the last character was a } then split the token.
                //

                if (currentLine[i-1] == '}')
                {
                    token.assign(currentLine, start, i - start - 1);
                    Tokens->insert(Tokens->end(), token);
                    Tokens->insert(Tokens->end(), string("}"));
                }
                else
                {
                    token.assign(currentLine, start, i - start);
                    Tokens->insert(Tokens->end(), token);
                }

                start = -1;
            }
        }
        else if (start == -1)
        {
            //
            // first character of a token.  If it's { then split the token.
            //

            if (currentLine[i] == '{')
            {
                Tokens->insert(Tokens->end(), string("{"));
            }
            else if (currentLine[i] == '}')
            {
                Tokens->insert(Tokens->end(), string("}"));
            }
            else
            {
                start = (long) i;
            }
        }
        else 
        {
            //
            // tracking a token - non whitespace is included.
            //
        }
    }

    return true;
}

VOID
PrintBytes(
    _In_                  ULONG BufferCb,
    _In_reads_bytes_(BufferCb) BYTE  Buffer[]
    )
{
    ULONG index = 0;

    for(index = 0; index < BufferCb; index += 16)
    {
        printf("  ");
        for(ULONG i = index; i < (index + 16); i += 1)
        {
            if (i < BufferCb)
            {
                if ((i != index) && (i % 8 == 0))
                {
                    printf("- ");
                }

                printf("%02x ", Buffer[i]);
            }
            else 
            {
                if ((i != index) && (i % 8 == 0))
                {
                    printf("  ");
                }
                printf("   ");
            }
        }

        printf(" : ");

        for(ULONG i = index; i < (index + 16); i += 1)
        {
            if (i < BufferCb)
            {
                if ((i != index) && (i % 8 == 0))
                {
                    printf(" ");
                }

                if (isprint(Buffer[i]))
                {
                    printf("%c", Buffer[i]);
                }
                else
                {
                    printf(".");
                }
            }
            else 
            {
                printf(" ");
            }
        }

        printf("\n");
    }
}

BOOL
WINAPI
OnControlKey(
    _In_ DWORD ControlType
    )
{
    if (ControlType == CTRL_C_EVENT)
    {
        //
        // If there's a current command then attempt to cancel it.
        //

        if (g_CurrentCommand != nullptr)
        {
            g_CurrentCommand->Cancel();
            return TRUE;
        }

        CancelIo(g_Peripheral);
    }

    return FALSE;
}
