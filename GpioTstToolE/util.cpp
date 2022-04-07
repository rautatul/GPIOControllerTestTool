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
published, uploaded, posted, transmitted, distributed, or disclosed in any way without Intel�s
prior express written permission.

No license under any patent, copyright, trade secret or other intellectual property right
is granted to or conferred upon you by disclosure or delivery of the Materials, either expressly,
by implication, inducement, estoppel or otherwise. Any license under such intellectual property
rights must be express and approved by Intel in writing.

*************************************************************************************************/

#include "internal.h"

_Success_(return)
bool
PopStringParameter(
    _Inout_   list<string> *Parameters,
    _Out_     string       *Value,
    _Out_opt_ bool         *Present
    )
{
    if (Parameters->empty())
    {
        if (Present != nullptr)
        {
            *Present = false;
            *Value = string("");
            return true;
        }
        else
        {
            printf("Missing required parameter\n");
            return false;
        }
    }

    if (Present != nullptr)
    {
        *Present = true;
    }

    *Value = Parameters->front();
    Parameters->pop_front();
    return true;
}

_Success_(return)
bool
ParseNumber(
    _In_     const string &String,
    _In_     ULONG         Radix,
    _Out_    ULONG        *Value,
    _In_opt_ bounds        Bounds
    )
{
    PSTR end;
    
#pragma prefast(suppress:__WARNING_MISSING_ZERO_TERMINATION2 ,"zero-termination is checked below")
    *Value = strtoul(String.c_str(), &end, Radix);

    //
    // Make sure the entire string parsed.
    //

    if (*end != '\0')
    {
        printf("Value %s is not a number\n", String.c_str());
        return false;
    }

    //
    // See if we should do a bounds check.
    //

    if ((Bounds.first != 0) || (Bounds.second != 0))
    {
        if ((*Value < Bounds.first) || 
            (*Value > Bounds.second))
        {
            printf("Value %s is out of bounds\n", String.c_str());
            false;
        }
    }

    return true;
}

_Success_(return)
bool
ParseDouble(
    _In_     const string         &String,
    _Out_    double               *Value,
    _In_opt_ pair<double, double>  Bounds
    )
{
    PSTR end;
    
#pragma prefast(suppress:__WARNING_MISSING_ZERO_TERMINATION2 ,"zero-termination is checked below")
    *Value = strtod(String.c_str(), &end);

    //
    // Make sure the entire string parsed.
    //

    if (*end != '\0')
    {
        printf("Value %s is not a number\n", String.c_str());
        return false;
    }

    //
    // See if we should do a bounds check.
    //

    if ((Bounds.first != 0) || (Bounds.second != 0))
    {
        if ((*Value < Bounds.first) || 
            (*Value > Bounds.second))
        {
            printf("Value %s is out of bounds\n", String.c_str());
            false;
        }
    }

    return true;
}

_Success_(return)
bool
PopNumberParameter(
    _Inout_   list<string> *Parameters,
    _In_      ULONG         Radix,
    _Out_     ULONG        *Value,
    _In_opt_  bounds        Bounds,
    _Out_opt_ bool         *Present
    )
{
    string s;

    bool found = PopStringParameter(Parameters, &s, Present);
 
    if ((Present != nullptr) && (*Present == false))
    {
        *Value = Bounds.first;
        return true;
    }
    else if (found == false)
    {
        return false;
    }

    return ParseNumber(s, Radix, Value, Bounds);
}

_Success_(return)
bool
PopDoubleParameter(
    _Inout_   list<string>        *Parameters,
    _Out_     double              *Value,
    _In_opt_  pair<double,double>  Bounds,
    _Out_opt_ bool                *Present
    )
{
    string s;

    bool found = PopStringParameter(Parameters, &s, Present);
 
    if ((Present != nullptr) && (*Present == false))
    {
        *Value = Bounds.first;
        return true;
    }
    else if (found == false)
    {
        return false;
    }

    return ParseDouble(s, Value, Bounds);
}

_Success_(return)
bool
PopBufferParameter(
    _Inout_ list<string>       *Parameters,
    _Out_   pair<ULONG, PBYTE> *Value
    )
{
    ULONG length = 0;
    PBYTE buffer = nullptr;

    //
    // Check for an explict length
    //

    if (Parameters->front() != "{")
    {
        if (PopNumberParameter(Parameters, 10, &length) == false)
        {
            printf("Length expected\n");
            return false;
        }

        //
        // Consume any leading {
        //

        if ((Parameters->empty() == false) && 
            (Parameters->front() == "{"))
        {
            Parameters->pop_front();
        }
    } 
    else 
    {
        string tmp;
        if (PopStringParameter(Parameters, &tmp) == false)
        {
            return false;
        }
        else if (tmp != "{")
        {
            printf("output buffer must start with {\n");
            return false;
        }

        //
        // Count values until the trailing } - assume one byte per value
        //

        list<string>::iterator i;

        for(i = Parameters->begin(); 
            ((i != Parameters->end()) && (*i != "}")); 
            i++)
        {
            length += 1;
        }

        if (*i != "}")
        {
            printf("output buffer must end with }\n");
            return false;
        }
    }
    
    if (length != 0)
    {
        ULONG b = 0;
        bool bufferEnd = false;

        //
        // Allocate the buffer.
        //

        buffer = new BYTE[length];

        for(b = 0; b < length; b += 1)
        {
            ULONG value = 0;

            if (bufferEnd == false) 
            {
                string nextElement;
               
                PopStringParameter(Parameters, &nextElement);

                if (nextElement == "}")
                {
                    value = 0;
                    bufferEnd = true;
                }
                else if (ParseNumber(nextElement, 16, &value, bounds(0, 0xff)) == false)
                {
                    printf("invalid byte value %s\n", nextElement.c_str());
                    return false;
                }
            }

            buffer[b] = (BYTE) value;
        }

        if (bufferEnd == false)
        {
            string end;
           
            if (PopStringParameter(Parameters, &end) == false)
            {
                printf("unclosed buffer\n");
                return false;
            }
            else if (end != "}")
            {
                printf("buffer has too many initializers\n");
                return false;
            }
        }
    }

    Value->first = length;
    Value->second = buffer;

    return true;
}

void wait ( int seconds )
{
	clock_t endwait;
	endwait = clock () + seconds * CLOCKS_PER_SEC ;
	while (clock() < endwait) {}
}
