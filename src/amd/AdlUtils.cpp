/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018      SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <uv.h>
#include <cmath>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#ifdef __linux__
#include <dlfcn.h>
#include <unistd.h>
#endif
#include <sys/types.h>

#include <CL/cl_ext.h>

#include "amd/OclCache.h"
#include "amd/OclError.h"
#include "amd/OclLib.h"
#include "amd/AdlUtils.h"

//#include "amd/OclGPU.h"
#include "common/log/Log.h"
#include "workers/Workers.h"
#include "3rdparty/ADL/adl_sdk.h"
#include "3rdparty/ADL/adl_defines.h"
#include "3rdparty/ADL/adl_structures.h"

 // Definitions of the used function pointers.Add more if you use other ADL APIs.
typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int(*ADL2_OVERDRIVEN_TEMPERATURE_GET) (ADL_CONTEXT_HANDLE, int, int, int*);
typedef int(*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int(*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE context, LPAdapterInfo lpInfo, int iInputSize);
typedef int(*ADL2_OVERDRIVEN_FANCONTROL_GET) (ADL_CONTEXT_HANDLE, int, ADLODNFanControl*);
typedef int(*ADL2_OVERDRIVEN_FANCONTROL_SET) (ADL_CONTEXT_HANDLE, int, ADLODNFanControl*);
typedef int(*ADL2_OVERDRIVE6_FANSPEED_GET) (ADL_CONTEXT_HANDLE, int, ADLOD6FanSpeedInfo *);
typedef int(*ADL2_OVERDRIVE6_FANSPEED_SET) (ADL_CONTEXT_HANDLE, int, ADLOD6FanSpeedValue *);
typedef int(*ADL2_OVERDRIVEN_CAPABILITIES_GET)	(ADL_CONTEXT_HANDLE, int, ADLODNCapabilities*);

static uv_mutex_t m_mutex;


ADL2_MAIN_CONTROL_CREATE                        ADL2_Main_Control_Create = nullptr;
ADL2_MAIN_CONTROL_DESTROY                       ADL2_Main_Control_Destroy = nullptr;
ADL2_OVERDRIVEN_TEMPERATURE_GET					ADL2_OverdriveN_Temperature_Get = nullptr;
ADL2_ADAPTER_NUMBEROFADAPTERS_GET				ADL2_Adapter_NumberOfAdapters_Get = nullptr;
ADL2_ADAPTER_ADAPTERINFO_GET					ADL2_Adapter_AdapterInfo_Get = nullptr;
ADL2_OVERDRIVEN_FANCONTROL_GET                  ADL2_OverdriveN_FanControl_Get = nullptr;
ADL2_OVERDRIVEN_FANCONTROL_SET                  ADL2_OverdriveN_FanControl_Set = nullptr;
ADL2_OVERDRIVE6_FANSPEED_SET                    ADL2_Overdrive6_FanSpeed_Set = nullptr;
ADL2_OVERDRIVE6_FANSPEED_GET                    ADL2_Overdrive6_FanSpeed_Get = nullptr;
ADL2_OVERDRIVEN_CAPABILITIES_GET                ADL2_OverdriveN_Capabilities_Get = nullptr;

// Memory allocation function
void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}

// Optional Memory de-allocation function
void __stdcall ADL_Main_Memory_Free(void* lpBuffer)
{
	if (NULL != lpBuffer)
	{
		free(lpBuffer);
		lpBuffer = NULL;
	}
}

#if defined (__linux__)
// equivalent functions in linux
void * GetProcAddress( void * pLibrary, const char * name)
{
    return dlsym( pLibrary, name);
}


const char *getUserName()
{
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw)
  {
    return pw->pw_name;
  }

  return "";
}

#endif

#ifdef __linux__
void *hDLL = NULL;      // Handle to .so library
#else
HINSTANCE hDLL;         // Handle to DLL
#endif

bool AdlUtils::InitADL(CoolingContext *cool)
{

#ifdef __linux__
    // Loop over hwmon to find the correct card id
    char filenameBuf[100];
    int iCardIndex;

    for (iCardIndex = 0; iCardIndex <= 16; iCardIndex++)
    {
        snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/temp1_input", cool->pciBus, iCardIndex);
        //LOG_INFO("OPEN std::ifstream %s", filenameBuf);
        cool->ifsTemp.open(filenameBuf, std::ifstream::in);
        if (cool->ifsTemp.is_open()) {
            cool->Card = iCardIndex;
            break;
        }
    }
    if (!cool->ifsTemp.is_open()) {
        LOG_ERR("Failed to open %s", filenameBuf);
        return false;
    }

    snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1", cool->pciBus, cool->Card);
    //LOG_INFO("OPEN std::ifstream %s", filenameBuf);
    cool->ifsFan.open(filenameBuf, std::ifstream::in);
    if (!cool->ifsFan.is_open()) {
        LOG_ERR("Failed to open %s", filenameBuf);

        // Check for root
        uid_t uid = geteuid();
        if (uid != 0) {
            LOG_ERR("UserID %i has no priviledge to open fan control, needs to be run as root, fan control disabled!", uid);
            cool->IsFanControlEnabled = false;
        }


    }
    cool->IsFanControlEnabled = true;
    return true;

#else	
    if (hDLL == NULL) {
        hDLL = LoadLibraryA("atiadlxx.dll");
        if (hDLL == NULL)
            // A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
            // Try to load the 32 bit library (atiadlxy.dll) instead
            hDLL = LoadLibraryA("atiadlxy.dll");
    }
#endif

#ifndef __linux__
    if (NULL == hDLL)
    {
        LOG_ERR("ADL library not found! Please install atiadlxx.dll");
        return false;
    }

    ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");
    ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
    ADL2_OverdriveN_Temperature_Get = (ADL2_OVERDRIVEN_TEMPERATURE_GET)GetProcAddress(hDLL, "ADL2_OverdriveN_Temperature_Get");
    ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(hDLL, "ADL2_Adapter_NumberOfAdapters_Get");
    ADL2_Adapter_AdapterInfo_Get = (ADL2_ADAPTER_ADAPTERINFO_GET)GetProcAddress(hDLL, "ADL2_Adapter_AdapterInfo_Get");
    ADL2_OverdriveN_FanControl_Get = (ADL2_OVERDRIVEN_FANCONTROL_GET)GetProcAddress(hDLL, "ADL2_OverdriveN_FanControl_Get");
    ADL2_OverdriveN_FanControl_Set = (ADL2_OVERDRIVEN_FANCONTROL_SET)GetProcAddress(hDLL, "ADL2_OverdriveN_FanControl_Set");
    ADL2_OverdriveN_Capabilities_Get = (ADL2_OVERDRIVEN_CAPABILITIES_GET)GetProcAddress(hDLL, "ADL2_OverdriveN_Capabilities_Get");

    ADL2_Overdrive6_FanSpeed_Get = (ADL2_OVERDRIVE6_FANSPEED_GET)GetProcAddress(hDLL, "ADL2_Overdrive6_FanSpeed_Get");
    ADL2_Overdrive6_FanSpeed_Set = (ADL2_OVERDRIVE6_FANSPEED_SET)GetProcAddress(hDLL, "ADL2_Overdrive6_FanSpeed_Set");

    if (NULL == ADL2_Main_Control_Create ||
        NULL == ADL2_Main_Control_Destroy ||
        NULL == ADL2_OverdriveN_Temperature_Get ||
        NULL == ADL2_Adapter_NumberOfAdapters_Get ||
        NULL == ADL2_Adapter_AdapterInfo_Get ||
        NULL == ADL2_OverdriveN_FanControl_Get ||
        NULL == ADL2_OverdriveN_FanControl_Set ||
        NULL == ADL2_Overdrive6_FanSpeed_Get ||
        NULL == ADL2_OverdriveN_Capabilities_Get ||
        NULL == ADL2_Overdrive6_FanSpeed_Set)
	{
		LOG_ERR("ADL APIs are missing!");
		return false;
	}

    if (ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &cool->context) == ADL_OK) {
        cool->IsFanControlEnabled = true;
        return true;
    }
    return false;
#else
   return ADL_OK;
#endif
}

bool  AdlUtils::ReleaseADL(CoolingContext *cool, bool bReset)
{
#ifdef __linux__
	if ((!cool->FanIsAutomatic) && bReset) {
		// Set back to automatic fan control
		cool->CurrentFan = 0;
		AdlUtils::SetFanPercent( cool, cool->CurrentFan);
	}
	if (cool->ifsTemp.is_open()) {
		cool->ifsTemp.close();
	}
	if (cool->ifsFan.is_open()) {
		cool->ifsFan.close();
	}

#else	
    int ret = ADL2_Main_Control_Destroy(cool->context);
    if (hDLL != NULL) {
        FreeLibrary(hDLL);
        hDLL = NULL;
    }
    return ret == ADL_OK ? true : false;
#endif	
}

bool AdlUtils::Get_DeviceID_by_PCI(CoolingContext *cool, xmrig::OclThread * thread)
{
#ifdef __linux__
    return Get_DeviceID_by_PCI_Linux(cool, thread);
#else
    return Get_DeviceID_by_PCI_Windows(cool, thread);
#endif
}

bool AdlUtils::Get_DeviceID_by_PCI_Linux(CoolingContext *cool, xmrig::OclThread * thread)
{
    //LOG_INFO("thread->pciBusID() = %i", thread->pciBusID());
    return true;
}

bool AdlUtils::Get_DeviceID_by_PCI_Windows(CoolingContext *cool, xmrig::OclThread * thread)
{
#ifndef __linux__
    int iNumberAdapters = 0;
    bool found = false;

    if (ADL_OK == ADL2_Adapter_NumberOfAdapters_Get(cool->context, &iNumberAdapters)) {

        AdapterInfo* infos = new AdapterInfo[iNumberAdapters];
        if (ADL_OK == ADL2_Adapter_AdapterInfo_Get(cool->context, infos, sizeof(AdapterInfo)*iNumberAdapters)) {

            int iCandidateIndex;
            int iCandidateCount = 0;            

            for (int i = 0; i < iNumberAdapters; i++) {
                //LOG_INFO("%i " YELLOW("PCI:%04x:%02x:%02x") " UID %s AdapterID %i present %i exists %i", i, infos[i].iFunctionNumber, infos[i].iBusNumber, infos[i].iDeviceNumber, infos[i].strDriverPath, infos[i].iAdapterIndex, infos[i].iPresent, infos[i].iExist);
                if (thread->pciBusID() == infos[i].iBusNumber) {
                    iCandidateIndex = i;
                    cool->Card = i;
                    
                    AdlUtils::GetMaxFanRpm(cool);

                    if (TemperatureWindows(cool)) {
                        LOG_INFO("Card "  YELLOW("PCI:%04x:%02x:%02x") " Temp %i Thread %i ADL Adapter %i", thread->pciDomainID(), thread->pciBusID(), thread->pciDeviceID(), cool->CurrentTemp, thread->threadId(), cool->Card);
                        found = true;
                        break;
                    }
                    else {
                        LOG_ERR("Failed to get Temperature for Display Adapter %i", cool->Card);
                    }
                }

                
            }
        }
    }
    return found;

#endif
}

bool AdlUtils::GetMaxFanRpm(CoolingContext *cool)
{
#ifndef __linux__
    ADLODNCapabilities overdriveCapabilities;
    memset(&overdriveCapabilities, 0, sizeof(ADLODNCapabilities));

    if (ADL_OK != ADL2_OverdriveN_Capabilities_Get(cool->context, cool->Card, &overdriveCapabilities))
    {
        LOG_ERR("ADL2_OverdriveN_Capabilities_Get failed\n");
        cool->MaxFanSpeed = -1;
        return false;
    }
    else {
        cool->MaxFanSpeed = overdriveCapabilities.fanSpeed.iMax;
    }
    return true;
#endif    
}

bool AdlUtils::GetFanPercent(CoolingContext *cool, int *percent)
{
#ifdef __linux__
    return GetFanPercentLinux(cool, percent);
#else
    return GetFanPercentWindows(cool, percent);
#endif
}

bool AdlUtils::GetFanPercentLinux(CoolingContext *cool, int *percent)
{
	int result;
	char filenameBuf[150];
	std::ifstream ifsFanSpeed;

    //LOG_INFO("DEBUG GetFanPercentLinux 1");

    snprintf(filenameBuf, 150, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1", cool->pciBus, cool->Card );
		
    //LOG_INFO("OPEN std::ifstream %s", filenameBuf);
    ifsFanSpeed.open (filenameBuf);
    if (!ifsFanSpeed.is_open()) {
        LOG_ERR("Failed to open %s", filenameBuf);
        LOG_INFO("DEBUG GetFanPercentLinux Exit 1");
        return false;
    }

    int speed;
    ifsFanSpeed >> speed;
    ifsFanSpeed.close();  
    
    //LOG_INFO("DEBUG GetFanPercentLinux Speed %i", speed);

    cool->CurrentFan = (speed * 100) / 255;
    if (percent != NULL)
    {
        *percent = cool->CurrentFan;
    }
    //LOG_INFO("DEBUG GetFanPercentLinux Percent %i", cool->CurrentFan);

    //LOG_INFO("DEBUG GetFanPercentLinux Exit 2");
    
    return true;
}

bool AdlUtils::GetFanPercentWindows(CoolingContext *cool, int *percent)
{
   /*
    ADLOD6FanSpeedInfo odNFanSpeed;
    memset(&odNFanSpeed, 0, sizeof(ADLOD6FanSpeedInfo));
    int ret = ADL2_Overdrive6_FanSpeed_Get(cool->context, cool->Card, &odNFanSpeed);
    if (ADL_OK == ret)
    {
        cool->CurrentFan = odNFanSpeed.iFanSpeedPercent;
        if (percent != NULL)
        {
            *percent = cool->CurrentFan;
        }
        return true;
    }
    return false;
    */

    //AdlUtils::SetFanPercentWindows(cool, 100);
    //AdlUtils::SetFanPercentWindows(cool, 2400);

#ifndef __linux__
    ADLODNFanControl odNFanControl;
    memset(&odNFanControl, 0, sizeof(ADLODNFanControl));

    odNFanControl.iCurrentFanSpeedMode = ADL_DL_FANCTRL_SPEED_TYPE_PERCENT;
    odNFanControl.iMode = ADLODNControlType::ODNControlType_Manual;
    
    if (ADL_OK == ADL2_OverdriveN_FanControl_Get(cool->context, cool->Card, &odNFanControl))
    {
        cool->CurrentFan = (odNFanControl.iCurrentFanSpeed * 100) / cool->MaxFanSpeed;
        if (percent != NULL)
        {
           *percent = cool->CurrentFan;
        }
        return true;
    }
    return false;
#endif    
}

bool AdlUtils::SetFanPercent(CoolingContext *cool, int percent)
{
	
	if (!cool->IsFanControlEnabled) {
		return false;
	}
	
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;

#ifdef __linux__
	return SetFanPercentLinux( cool, percent);
#else
	return SetFanPercentWindows( cool, percent);
#endif	
}

bool AdlUtils::SetFanPercentWindows(CoolingContext *cool, int percent)
{
#ifdef __linux__
    return ADL_ERR;
#else
    ADLODNFanControl odNFanControl;
    memset(&odNFanControl, 0, sizeof(ADLODNFanControl));

    if (percent == 0) {
        odNFanControl.iMinFanLimit = 500;
        odNFanControl.iMode = ADLODNControlType::ODNControlType_Auto;
    }
    else {
        odNFanControl.iMinFanLimit = (percent * cool->MaxFanSpeed) / 100;
        odNFanControl.iMode = ADLODNControlType::ODNControlType_Manual;
    }

    int ret = ADL2_OverdriveN_FanControl_Set(cool->context, cool->Card, &odNFanControl);
    if (ADL_OK == ret)
    {
        return true;
    }
    return false;
#endif
}

bool AdlUtils::SetFanPercentLinux(CoolingContext *cool, int percent)
{
#ifdef __linux__
	int result;
	char filenameBuf[100];
	std::ofstream ifsFanControl;
	std::ofstream ifsFanSpeed;

	snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1_enable", cool->pciBus, cool->Card );
	//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
	ifsFanControl.open (filenameBuf);
	if (!ifsFanControl.is_open()) {
		LOG_ERR("Failed to open %s", filenameBuf);

		// Check for root
		uid_t uid = geteuid();
		if (uid != 0) {
			LOG_ERR("UserID %i has no priviledge to open fan control, needs to be run as root, fan control disabled!", uid);
			cool->IsFanControlEnabled = false;
		}
		return false;
	}
	cool->IsFanControlEnabled = true;

    //LOG_INFO("DEBUG SetFanPercentLinux percent %i", percent);

	if (percent == 0) {
		char *automatic = "2";
		ifsFanControl << automatic;
		ifsFanControl.close();
		cool->FanIsAutomatic = true;
		result = ADL_OK;
	}
	else {
		char *manual = "1";
		ifsFanControl << manual;
		ifsFanControl.close();
		cool->FanIsAutomatic = false;

		snprintf(filenameBuf, 100, "/sys/module/amdgpu/drivers/pci:amdgpu/0000:%02x:00.0/hwmon/hwmon%i/pwm1", cool->pciBus, cool->Card );
		//LOG_INFO("OPEN std::ifstream %s", filenameBuf);
		ifsFanSpeed.open (filenameBuf);
		if (!ifsFanSpeed.is_open()) {
			LOG_ERR("Failed to open %s", filenameBuf);
			return false;
		}

		int speed = (percent * 255) / 100;
		ifsFanSpeed << speed;
		ifsFanSpeed.close();

        //LOG_INFO("DEBUG SetFanPercentLinux Speed %i", speed);

		result = ADL_OK;
	}

	return true;
#else
    return false;
#endif
}


bool AdlUtils::Temperature(CoolingContext *cool)
{
#ifdef __linux__
    return TemperatureLinux(cool);
#else
    return TemperatureWindows(cool);
#endif
}

bool AdlUtils::TemperatureLinux(CoolingContext *cool)
{
#ifdef __linux__
	int result;

	
	int temp = 0;
	int fan = 0;
	try
	{
		if (cool->ifsTemp.is_open()) {
			//LOG_INFO("Filestream is open.");
			cool->ifsTemp.seekg (0, std::ifstream::beg);
			cool->ifsTemp >> temp;
			result = ADL_OK;
		}
		//LOG_INFO("Temp is %i", temp);
		if (cool->ifsFan.is_open()) {
			//LOG_INFO("Filestream is open.");
			cool->ifsFan.seekg (0, std::ifstream::beg);
			cool->ifsFan >> fan;
			result = ADL_OK;
		}

	}
	catch(const std::exception& ex) {
		LOG_ERR("Exception reading temp: %s", ex.what());
		result = ADL_ERR;
	}

	cool->CurrentTemp = temp / 1000;
	//cool->CurrentFan = (fan * 100) / 255;

	//LOG_INFO("FINAL std::ifstream temp %i", temp);
			

				//if (topology.pcie.bus == infos[i].iBusNumber) {
			
				//LOG_DEBUG("***** Card %u temp %u adapterindex %u, adlbus %u oclbus %u", i, temp, infos[i].iAdapterIndex, infos[i].iBusNumber, topology.pcie.bus);
			
	
	return true;
#else
    return false;
#endif
}


bool AdlUtils::TemperatureWindows(CoolingContext *cool)
{	
#ifndef __linux__    
	if (ADL_OK != ADL2_OverdriveN_Temperature_Get(cool->context, cool->Card, 1, &cool->CurrentTemp)) {
		LOG_ERR("Failed to get ADL2_OverdriveN_Temperature_Get");
        return false;
	}
    cool->CurrentTemp = cool->CurrentTemp / 1000;
	return true;
#endif
}

#ifdef __linux__
int AdlUtils::GetTickCount(void) 
{
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now))
    return 0;
  return now.tv_sec * 1000.0 + now.tv_nsec / 1000000.0;
}
#endif

bool  AdlUtils::DoCooling(cl_device_id DeviceID, int deviceIdx, int ThreadID, CoolingContext *cool)
{
	const int StartSleepFactor = 500;
    const float IncreaseSleepFactor = 1.5;
	const int FanFactor = 1;
    const int FanAutoDefault = 50;
    const int TickDiff = GetTickCount() - cool->LastTick;
	
    if (AdlUtils::Temperature(cool) != true) {
		return false;
	}
	
    if (TickDiff < 1000) {
		return true;
	}
	cool->LastTick = GetTickCount();

    if (Workers::fanlevel() > 0)
    {
        SetFanPercent(cool, Workers::fanlevel());
    }

    //LOG_INFO("DEBUG DoCooling 2");
    if (cool->CurrentFan <= 0) {
        AdlUtils::GetFanPercent(cool, NULL);
    }
    //LOG_INFO("DEBUG DoCooling 3");
    

	if (cool->CurrentTemp > Workers::maxtemp()) {
		if (!cool->NeedsCooling) {
			cool->SleepFactor = StartSleepFactor;
			LOG_INFO( YELLOW("Card %u Thread %i Temperature %u is over %i, reduced mining, Sleeptime %i"), deviceIdx, ThreadID, cool->CurrentTemp, Workers::maxtemp(), cool->SleepFactor);
		}
		cool->NeedsCooling = true;
        cool->CurrentFan = 100;
        AdlUtils::SetFanPercent(cool, cool->CurrentFan);
	}

	if (cool->NeedsCooling) {
		if (cool->CurrentTemp < Workers::maxtemp() - Workers::falloff()) {
			LOG_INFO( YELLOW("Card %u Thread %i Temperature %i is below %i, do full mining, Sleeptime was %u"), deviceIdx, ThreadID, cool->CurrentTemp, Workers::maxtemp() - Workers::falloff(), cool->SleepFactor);
			//cool->LastTemp = cool->CurrentTemp;
			cool->NeedsCooling = false;
			cool->SleepFactor = StartSleepFactor;

            /*if (Workers::fanlevel() == 0)
            {
                // Decrease fan speed
                if (cool->CurrentFan > 0)
                    cool->CurrentFan = cool->CurrentFan - FanFactor;
                AdlUtils::SetFanPercent(cool, cool->CurrentFan);
            }*/

		}
		if (cool->LastTemp < cool->CurrentTemp) {
			cool->SleepFactor = cool->SleepFactor * IncreaseSleepFactor;
			if (cool->SleepFactor > 1500) {
				cool->SleepFactor = 1500;
			}
			//LOG_INFO("Card %u Temperature %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);
		}
		
	}

    //LOG_INFO("DEBUG DoCooling 4");

	if (cool->NeedsCooling) {
		int iReduceMining = 5;

        if (Workers::fanlevel() == 0)
        {
            // Increase fan speed
            if (cool->CurrentFan < 100)
                cool->CurrentFan = cool->CurrentFan + (FanFactor * 3);
            AdlUtils::SetFanPercent(cool, cool->CurrentFan);
        }
		//LOG_INFO("Card %u Temperature %i iReduceMining %i iSleepFactor %i LastTemp %i NeedCooling %i ", deviceIdx, temp, iReduceMining, cool->SleepFactor, cool->LastTemp, cool->NeedCooling);

		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(cool->SleepFactor));
			iReduceMining = iReduceMining - 1;
		} while ((iReduceMining > 0) && (Workers::sequence() > 0));
	}
	else {
        if (Workers::fanlevel() == 0)
        {
            // Decrease fan speed if temp keeps dropping
            if (cool->LastTemp > cool->CurrentTemp) {
                if (!cool->FanIsAutomatic) {
                    if (cool->CurrentFan > FanAutoDefault) {
                        cool->CurrentFan = cool->CurrentFan - FanFactor;
                        //LOG_INFO("DEBUG AdlUtils::SetFanPercent %i", cool->CurrentFan);
                        AdlUtils::SetFanPercent(cool, cool->CurrentFan);
                    }
                    else {
                        if (cool->CurrentFan < FanAutoDefault) {
                            // Set back to automatic fan control
                            cool->CurrentFan = 0;
                            AdlUtils::SetFanPercent(cool, cool->CurrentFan);
                        }
                    }
                }
            }
            else {
                // Increase fan speed if temp keeps raising
                if (!cool->FanIsAutomatic) {
                    cool->CurrentFan = cool->CurrentFan + FanFactor;
                    if (cool->CurrentFan > 100)
                        cool->CurrentFan = 100;
                    AdlUtils::SetFanPercent(cool, cool->CurrentFan);                    
                }
            }
        }
	}
	cool->LastTemp = cool->CurrentTemp;
	
    //LOG_INFO("DEBUG DoCooling Exit");

    return true;
}