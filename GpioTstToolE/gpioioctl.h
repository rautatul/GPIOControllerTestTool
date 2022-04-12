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

#ifndef _CVF_GPIO_H_
#define _CVF_GPIO_H_

//
// Device path names
//
#define CVS_GPIO_NAME L"CVFGPIO"

#define CVF_GPIO_SYMBOLIC_NAME L"\\DosDevices\\" CVS_GPIO_NAME
#define CVF_GPIO_USERMODE_PATH L"\\\\.\\" CVS_GPIO_NAME
#define CVF_GPIO_USERMODE_PATH_SIZE sizeof(CVF_GPIO_USERMODE_PATH)

//
// Priavte GPIOTestTool IOCTLs
//

#define FILE_DEVICE_GPIO_PERIPHERAL 65501U
#define IOCTL_CVS_GPIO_IO_READ                CTL_CODE(FILE_DEVICE_GPIO_PERIPHERAL, 0x700, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CVS_GPIO_IO_WRITE               CTL_CODE(FILE_DEVICE_GPIO_PERIPHERAL, 0x701, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CVS_GPIO_WAIT_ON_INTERRUPT      CTL_CODE(FILE_DEVICE_GPIO_PERIPHERAL, 0x702, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CVS_GPIO_ACKNOWLEDGE_INTERRUPT  CTL_CODE(FILE_DEVICE_GPIO_PERIPHERAL, 0x703, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef ULONG GPIO_READ_REQUEST_VALUE;
typedef ULONG GPIO_WRITE_REQUEST_VALUE;

typedef struct _GPIO_REQUEST_IN
{
    ULONG GpioIndex;
}
GPIO_REQUEST_IN, *PGPIO_REQUEST_IN;

typedef struct _GPIO_READ_REQUEST_IN
    : public GPIO_REQUEST_IN
{
}
GPIO_READ_REQUEST_IN, *PGPIO_READ_REQUEST_IN;

typedef struct _GPIO_READ_REQUEST_OUT
{
    GPIO_READ_REQUEST_VALUE Value;
}
GPIO_READ_REQUEST_OUT, *PGPIO_READ_REQUEST_OUT;

typedef struct _GPIO_WRITE_REQUEST_IN
    : public GPIO_REQUEST_IN
{
    GPIO_WRITE_REQUEST_VALUE Value;
}
GPIO_WRITE_REQUEST_IN, *PGPIO_WRITE_REQUEST_IN;

typedef struct _GPIO_INTERRUPT_IN
{
    ULONG InterruptIndex;
}
GPIO_INTERRUPT_IN, *PGPIO_INTERRUPT_IN;

#endif _CVF_GPIO_H_
