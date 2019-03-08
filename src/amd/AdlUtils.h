


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

typedef struct _CoolingContext {
	int SleepFactor = 0;
	int LastTemp = 0;
	int CurrentTemp = 0;
	int CurrentFan = 0;
	bool NeedsCooling = false;
	bool FanIsAutomatic = false;
	bool IsFanControlEnabled = false;
	int pciBus = -1;
	int Card = -1;
#ifdef __linux__
	std::ifstream ifsTemp;
	std::ifstream ifsFan;
#else
    ADL_CONTEXT_HANDLE context;
    int MaxFanSpeed;
#endif
	//uv_mutex_t m_mutex;
} CoolingContext;

class AdlUtils
{
public:
	
	static bool InitADL(CoolingContext *cool);
	static bool ReleaseADL(CoolingContext *cool);
    static bool Get_DeviceID_by_PCI(CoolingContext *cool, OclThread * thread);
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

};


#endif // __ADLUTILS_H__
