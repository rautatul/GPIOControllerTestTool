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

#ifndef _TRACE_H_
#define _TRACE_H_

extern "C" 
{
//
// Tracing Definitions:
//
// Control GUID: 
// [Guid("43840FCA-7695-4481-871D-1BC05C426AA7")]
//

#define WPP_CONTROL_GUIDS                           \
    WPP_DEFINE_CONTROL_GUID(                        \
        cvfGpioTraceGuid,                       \
        (43840FCA,7695,4481,871D,1BC05C426AA7),     \
        WPP_DEFINE_BIT(TRACE_FLAG_WDFLOADING)       \
        WPP_DEFINE_BIT(TRACE_FLAG_GPIO)             \
        WPP_DEFINE_BIT(TRACE_FLAG_GPIO_IO)          \
        WPP_DEFINE_BIT(TRACE_FLAG_GPIO_INTERRUPT)   \
        )
}

#define WPP_LEVEL_FLAGS_LOGGER(level,flags) WPP_LEVEL_LOGGER(flags)
#define WPP_LEVEL_FLAGS_ENABLED(level, flags) (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= level)

// begin_wpp config
// FUNC FuncEntry{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// FUNC FuncExit{LEVEL=TRACE_LEVEL_VERBOSE}(FLAGS);
// USEPREFIX(FuncEntry, "%!STDPREFIX! [%!FUNC!] --> entry");
// USEPREFIX(FuncExit, "%!STDPREFIX! [%!FUNC!] <--");
// FUNC Trace(LEVEL, FLAGS, MSG, ...);
// end_wpp

#endif _TRACE_H_
