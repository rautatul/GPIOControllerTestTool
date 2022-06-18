#pragma once

//#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
#include <winioctl.h>
#include <tchar.h>
#include <wchar.h>

#include <stdio.h>
#include <stdlib.h>
//#include <devioctl.h>
#include <strsafe.h>
#include <sys/timeb.h>
#include <time.h>

#include <initguid.h>
#include <shlwapi.h>
//#include <winnt.h>
#include <Cfgmgr32.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <map>

//#include "Exceptions.h"
// This is in DataStructs.h to limit scope
//#include "ProviderBase.h"   // for nlohmann Json

//
//using namespace std;

#if 0
#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(a)   (sizeof(a)/sizeof((a)[0]))
#endif
#else
// For Klocwork parser
#if !defined(ARRAY_SIZE)
//#define ARRAY_SIZE(a)   (_ARRAYSIZE(a))
#define ARRAY_SIZE(a)   (ARRAYSIZE(a))
#endif
#endif
