
#ifndef __COOLINGCONTEXT_H__
#define __COOLINGCONTEXT_H__

#include <iostream>
#include <fstream>
#include <uv.h>

typedef struct _CoolingContext {
	int SleepFactor = 0;
	int LastTemp = 0;
	ulong LastTick = 0;
	int CurrentTemp = 0;
	int CurrentFanLevel = 0;
	bool NeedsCooling = false;
	bool FanIsAutomatic = false;
	bool IsFanControlEnabled = false;
	int PciBus = -1;
	int GPUIndex = -1;
	int Card = -1;
	int Busy = -1;
	int Power = -1;
#ifdef __linux__
	std::ifstream ifsTemp;
	std::ifstream ifsFan;
#else
    ADL_CONTEXT_HANDLE context;
    int MaxFanSpeed;
#endif
	//uv_mutex_t m_mutex;
} CoolingContext;
#endif