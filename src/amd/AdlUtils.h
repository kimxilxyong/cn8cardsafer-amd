


#ifndef __ADLUTILS_H__
#define __ADLUTILS_H__

#include <iostream>
#include <fstream>
#include <uv.h>

#include "3rdparty/ADL/adl_structures.h"
#include "workers/OclThread.h"

// Memory allocation function
//void* __stdcall ADL_Main_Memory_Alloc(int iSize);
// Optional Memory de-allocation function
//void __stdcall ADL_Main_Memory_Free(void* lpBuffer);



class AdlUtils
{
public:
	
	static bool InitADL(CoolingContext *cool);
	static bool ReleaseADL(CoolingContext *cool, bool bReset);
    static bool Get_DeviceID_by_PCI(CoolingContext *cool, const xmrig::OclThread * thread);
	static bool Get_DeviceID_by_PCI_Linux(CoolingContext *cool, const xmrig::OclThread * thread);
	static bool Get_DeviceID_by_PCI_Windows(CoolingContext *cool, const xmrig::OclThread * thread);
	static bool Get_GPU_Busy(CoolingContext *cool, const xmrig::OclThread * thread);
	static bool Get_GPU_Power(CoolingContext *cool, const xmrig::OclThread * thread);
	static bool Temperature(CoolingContext *cool);
	static bool TemperatureLinux(CoolingContext *cool);
	static bool TemperatureWindows(CoolingContext *cool);
    static bool GetFanPercent(CoolingContext *cool, int *percent);
    static bool GetFanPercentLinux(CoolingContext *cool, int *percent);
    static bool GetFanPercentWindows(CoolingContext *cool, int *percent);
	static bool SetFanPercent(CoolingContext *cool, int percent);
	static bool SetFanPercentLinux(CoolingContext *cool, int percent);
	static bool SetFanPercentWindows(CoolingContext *cool, int percent);
	static bool DoCooling(cl_device_id DeviceID, int deviceIdx, int ThreadID, CoolingContext *cool);

    static bool GetMaxFanRpm(CoolingContext *cool);
	
	#ifdef __linux__
    static ulong GetTickCount(void);
    #endif
};


#endif // __ADLUTILS_H__
