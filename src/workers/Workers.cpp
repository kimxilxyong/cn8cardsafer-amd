/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2018-2019 SChernykh   <https://github.com/SChernykh>
 * Copyright 2016-2019 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#include <cmath>
#include <thread>


#include "amd/OclGPU.h"
#include "amd/OclLib.h"
#include "api/Api.h"
#include "common/log/Log.h"
#include "core/Config.h"
#include "core/Controller.h"
#include "crypto/CryptoNight.h"
#include "interfaces/IJobResultListener.h"
#include "interfaces/IThread.h"
#include "rapidjson/document.h"
#include "workers/Handle.h"
#include "workers/Hashrate.h"
#include "workers/OclThread.h"
#include "workers/OclWorker.h"
#include "workers/Workers.h"
#include "Mem.h"

#include "amd/AdlUtils.h"


bool Workers::m_active = false;
bool Workers::m_enabled = true;

int Workers::m_maxtemp = 75;
int Workers::m_falloff = 5;
int Workers::m_fanlevel = 0;

cl_context Workers::m_opencl_ctx;

Hashrate *Workers::m_hashrate = nullptr;
size_t Workers::m_threadsCount = 0;
std::atomic<int> Workers::m_paused;
std::atomic<uint64_t> Workers::m_sequence;
std::list<xmrig::Job> Workers::m_queue;
std::vector<Handle*> Workers::m_workers;
uint64_t Workers::m_ticks = 0;
uv_async_t Workers::m_async;
uv_mutex_t Workers::m_mutex;
uv_rwlock_t Workers::m_rwlock;
uv_timer_t Workers::m_timer;
xmrig::Controller *Workers::m_controller = nullptr;
xmrig::IJobResultListener *Workers::m_listener = nullptr;
xmrig::Job Workers::m_job;


struct JobBaton
{
    uv_work_t request;
    std::vector<xmrig::Job> jobs;
    std::vector<xmrig::JobResult> results;
    int errors = 0;

    JobBaton() {
        request.data = this;
    }
};


static size_t threadsCountByGPU(size_t index, const std::vector<xmrig::IThread *> &threads)
{
    size_t count = 0;

    for (const xmrig::IThread *thread : threads) {
        if (thread->index() == index) {
            count++;
        }
    }

    return count;
}


xmrig::Job Workers::job()
{
    uv_rwlock_rdlock(&m_rwlock);
    xmrig::Job job = m_job;
    uv_rwlock_rdunlock(&m_rwlock);

    return job;
}


size_t Workers::hugePages()
{
    return 0;
}


size_t Workers::threads()
{
    return m_threadsCount;
}


void Workers::printHashrate(bool detail)
{
    assert(m_controller != nullptr);
    if (!m_controller) {
        return;
    }

    if (detail) {
        const bool isColors = m_controller->config()->isColors();
        char num1[8] = { 0 };
        char num2[8] = { 0 };
        char num3[8] = { 0 };

        LOG_INFO(" THREAD | GPU |     PCI    | 10s H/s | 60s H/s | 15m H/s | TEMP |  FAN |", isColors ? "\x1B[1;37m" : "");
        

        size_t i = 0;
        for (const xmrig::IThread *t : m_controller->config()->threads()) {
            auto thread = static_cast<const xmrig::OclThread *>(t);
            CoolingContext *cool = thread->cool();
                
                //LOG_DEBUG("Cool=%i", cool);

                AdlUtils::GetMaxFanRpm(cool);
                AdlUtils::Temperature(cool);                
                AdlUtils::GetFanPercent(cool, NULL);

                //LOG_INFO("DEBUG printHashrate Speed %i", percent);

                //Log::i()->text("| %6zu | %3zu | " YELLOW("%04x:%02x:%02x") " | %3u  | %7s | %7s | %7s | %3.1i%%%  |",
                LOG_INFO(" %6zu | %3zu | " YELLOW("%04x:%02x:%02x") " | %7s | %7s | %7s | %3u  | %3.li%% |",
                    i, thread->cardId(),
                    thread->pciDomainID(),
                    thread->pciBusID(),
                    thread->pciDeviceID(),                    
                    Hashrate::format(m_hashrate->calc(i, Hashrate::ShortInterval), num1, sizeof num1),
                    Hashrate::format(m_hashrate->calc(i, Hashrate::MediumInterval), num2, sizeof num2),
                    Hashrate::format(m_hashrate->calc(i, Hashrate::LargeInterval), num3, sizeof num3),
                    cool->CurrentTemp,
                    cool->CurrentFanLevel
                );

                i++;
            }
         
        }


    //m_hashrate->print();
}

void Workers::printHealth()
{
    
    uv_rwlock_rdlock(&m_rwlock);

    const bool isColors = m_controller->config()->isColors();
      
    const size_t platformIndex            = static_cast<size_t>( m_controller->config()->platformIndex());
    std::vector<cl_platform_id> platforms = OclLib::getPlatformIDs();
    std::vector<GpuContext> ctxVec;

    cl_uint num_devices = 0;
    OclLib::getDeviceIDs(platforms[platformIndex], CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices);
    if (num_devices == 0) {
        LOG_ERR("No devices found for platform %i", platforms[platformIndex]);
        return;
    }

    cl_device_id *device_list = new cl_device_id[num_devices];
    OclLib::getDeviceIDs(platforms[platformIndex], CL_DEVICE_TYPE_GPU, num_devices, device_list, nullptr);

    //LOG_INFO("GPU |     PCI    | 10s H/s | 60s H/s | 15m H/s | MHz |  MEM | TEMP |  FAN |", isColors ? "\x1B[1;37m" : "");

    int matchcount;
    for (cl_uint i = 0; i < num_devices; i++) {
        GpuContext ctx;
        ctx.deviceIdx    = i;
        ctx.platformIdx  = platformIndex;
        ctx.DeviceID     = device_list[i];
        ctx.computeUnits = OclLib::getDeviceMaxComputeUnits(ctx.DeviceID);
        ctx.vendor       = OclLib::getDeviceVendor(ctx.DeviceID);

        if (ctx.vendor == xmrig::OCL_VENDOR_UNKNOWN) {
            continue;
        }

        OclLib::getDeviceInfo(ctx.DeviceID, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(size_t), &ctx.freeMem);
        OclLib::getDeviceInfo(ctx.DeviceID, CL_DEVICE_GLOBAL_MEM_SIZE,    sizeof(size_t), &ctx.globalMem);
        // if environment variable GPU_SINGLE_ALLOC_PERCENT is not set we can not allocate the full memory
        ctx.freeMem = std::min(ctx.freeMem, ctx.globalMem);

        ctx.board = OclLib::getDeviceBoardName(ctx.DeviceID);
        ctx.name  = OclLib::getDeviceName(ctx.DeviceID);

        int CardID = i;
        if (OclCLI::getPCIInfo(&ctx, CardID) != CL_SUCCESS) {
            LOG_ERR("Cannot get PCI information for Card %i", CardID);
        }

        //thread->setPciBusID(contexts[i]->device_pciBusID);
        //thread->setPciDeviceID(contexts[i]->device_pciDeviceID);
        //thread->setPciDomainID(contexts[i]->device_pciDomainID);



        size_t it = 0;
        matchcount = 0;
        CoolingContext coollocal;
        for (xmrig::IThread *t : m_controller->config()->threads()) {
            auto thread = static_cast<const xmrig::OclThread *>(t);
            CoolingContext *cool = thread->cool();
                
                //LOG_DEBUG("Cool=%i", cool);

                // Check if this thread belongs to this card
                if (ctx.device_pciBusID == cool->PciBus)
                {
                    matchcount++;

                    AdlUtils::GetMaxFanRpm(cool);
                    AdlUtils::Temperature(cool);                
                    AdlUtils::GetFanPercent(cool, NULL);
                    AdlUtils::Get_GPU_Power(cool, thread);
                    AdlUtils::Get_GPU_Busy(cool, thread);

                    if (matchcount == 1)
                    {
                        coollocal.GPUIndex = cool->GPUIndex;
                        //LOG_INFO("GPUIndex=%i", coollocal.GPUIndex);
                        coollocal.Busy = cool->Busy;
                        coollocal.Power = cool->Busy;
                    }
                    else
                    {
                        coollocal.GPUIndex = cool->GPUIndex;
                        //LOG_INFO("GPUIndex=%i", coollocal.GPUIndex);
                        coollocal.Busy = (coollocal.Busy + cool->Busy) / 2;
                        coollocal.Power = (coollocal.Power + cool->Power) / 2;
                    }
                    it++;
                }    
                
            }
         
            if (matchcount > 0)
            {
                //LOG_INFO("DEBUG printHashrate Speed %i", percent);

                cl_uint max_clock_freq;
                if (OclLib::getDeviceInfo(ctx.DeviceID, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(max_clock_freq), &max_clock_freq) != CL_SUCCESS) {
                    LOG_ERR("Cannot get MAX_CLOCK_FREQUENCY information for Card %i", CardID);
                }
    

                LOG_INFO(isColors ? MAGENTA("GPU #%i: |") " " YELLOW("PCI:%04x:%02x:%02x |") " " MAGENTA_BOLD("%i MHz | %i PWR | %i%% BUSY")
                                                            : "GPU #%i: | PCI:%04x:%02x:%02x | %u MHz | %i PWR | %i%% BUSY ",
                        ctx.deviceIdx,  //CardID,
                        ctx.device_pciDomainID, ctx.device_pciBusID, ctx.device_pciDeviceID,
                        max_clock_freq, coollocal.Power, coollocal.Busy
                        );
            }         
        
                   

        ctxVec.push_back(ctx);
    }


    delete [] device_list;

    
    uv_rwlock_rdunlock(&m_rwlock);
}

/*
void Workers::printHashrate(bool detail)
{
    assert(m_controller != nullptr);
    if (!m_controller) {
        return;
    }

    if (detail) {
        const bool isColors = m_controller->config()->isColors();
        char num1[8] = { 0 };
        char num2[8] = { 0 };
        char num3[8] = { 0 };

        Log::i()->text("%s| THREAD | GPU | 10s H/s | 60s H/s | 15m H/s |", isColors ? "\x1B[1;37m" : "");

        size_t i = 0;
        for (const xmrig::IThread *thread : m_controller->config()->threads()) {
            Log::i()->text("| %6zu | %3zu | %7s | %7s | %7s |",
                i, thread->index(),
                Hashrate::format(m_hashrate->calc(i, Hashrate::ShortInterval), num1, sizeof num1),
                Hashrate::format(m_hashrate->calc(i, Hashrate::MediumInterval), num2, sizeof num2),
                Hashrate::format(m_hashrate->calc(i, Hashrate::LargeInterval), num3, sizeof num3)
            );

            i++;
        }
    }

    m_hashrate->print();
}
*/

void Workers::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    if (!m_active) {
        return;
    }

    m_paused = enabled ? 0 : 1;
    m_sequence++;
}

void Workers::setMaxtemp(int maxtemp)
{
    m_maxtemp = maxtemp;
}

void Workers::setFalloff(int falloff)
{
    m_falloff = falloff;
}

void Workers::setFanlevel(int fanlevel)
{
    m_fanlevel = fanlevel;
}

void Workers::setJob(const xmrig::Job &job, bool donate)
{
    uv_rwlock_wrlock(&m_rwlock);
    m_job = job;

    if (donate) {
        m_job.setPoolId(-1);
    }
    uv_rwlock_wrunlock(&m_rwlock);

    m_active = true;
    if (!m_enabled) {
        return;
    }

    m_sequence++;
    m_paused = 0;
}


bool Workers::start(xmrig::Controller *controller)
{
#   ifdef APP_DEBUG
    LOG_NOTICE("THREADS ------------------------------------------------------------------");
    for (const xmrig::IThread *thread : controller->config()->threads()) {
        thread->print();
    }
    LOG_NOTICE("--------------------------------------------------------------------------");
#   endif

    m_controller = controller;
    const std::vector<xmrig::IThread *> &threads = controller->config()->threads();
    size_t ways = 0;

    for (const xmrig::IThread *thread : threads) {
       ways += thread->multiway();
    }

    m_threadsCount = threads.size();
    m_hashrate = new Hashrate(m_threadsCount, controller);

    uv_mutex_init(&m_mutex);
    uv_rwlock_init(&m_rwlock);

    m_sequence = 1;
    m_paused   = 1;

    uv_async_init(uv_default_loop(), &m_async, Workers::onResult);

    std::vector<GpuContext *> contexts(m_threadsCount);

    const bool isCNv2 = controller->config()->isCNv2();

    for (size_t i = 0; i < m_threadsCount; ++i) {
        xmrig::OclThread *thread = static_cast<xmrig::OclThread *>(threads[i]);
        if (isCNv2 && thread->stridedIndex() == 1) {
            LOG_WARN("%sTHREAD #%zu: \"strided_index\":1 is not compatible with CryptoNight variant 2",
                     controller->config()->isColors() ? "\x1B[1;33m" : "", i);
        }

        thread->setThreadsCountByGPU(threadsCountByGPU(thread->index(), threads));

        contexts[i] = thread->ctx();
    }

    if (InitOpenCL(contexts, controller->config(), &m_opencl_ctx) != 0) {
        return false;
    }

    uv_timer_init(uv_default_loop(), &m_timer);
    uv_timer_start(&m_timer, Workers::onTick, 500, 500);

    uint32_t offset = 0;

    size_t i = 0;
    for (xmrig::IThread *t: threads) {
        Handle *handle = new Handle(i, t, contexts[i], offset, ways);
        //Handle *handle = new Handle(i, t, &contexts[i], offset, ways);
        offset += t->multiway();

        xmrig::OclThread *thread = static_cast<xmrig::OclThread *>(t);

        int CardID = t->index();
        if (OclCLI::getPCIInfo(contexts[i], CardID) != CL_SUCCESS) {
            LOG_ERR("Cannot get PCI information for Card %i", CardID);
        }

        thread->setPciBusID(contexts[i]->device_pciBusID);
        thread->setPciDeviceID(contexts[i]->device_pciDeviceID);
        thread->setPciDomainID(contexts[i]->device_pciDomainID);

        i++;

        m_workers.push_back(handle);
        handle->start(Workers::onReady);
    }

    controller->save();

    return true;
}


void Workers::stop()
{
    uv_timer_stop(&m_timer);
    m_hashrate->stop();

    uv_close(reinterpret_cast<uv_handle_t*>(&m_async), nullptr);
    m_paused   = 0;
    m_sequence = 0;

    for (size_t i = 0; i < m_workers.size(); ++i) {
        m_workers[i]->join();
        ReleaseOpenCl(m_workers[i]->ctx());
    }

    ReleaseOpenClContext(m_opencl_ctx);
}


void Workers::submit(const xmrig::Job &result)
{
    uv_mutex_lock(&m_mutex);
    m_queue.push_back(result);
    uv_mutex_unlock(&m_mutex);

    uv_async_send(&m_async);
}


#ifndef XMRIG_NO_API
void Workers::threadsSummary(rapidjson::Document &doc)
{
//    uv_mutex_lock(&m_mutex);
//    const uint64_t pages[2] = { m_status.hugePages, m_status.pages };
//    const uint64_t memory   = m_status.ways * xmrig::cn_select_memory(m_status.algo);
//    uv_mutex_unlock(&m_mutex);

//    auto &allocator = doc.GetAllocator();

//    rapidjson::Value hugepages(rapidjson::kArrayType);
//    hugepages.PushBack(pages[0], allocator);
//    hugepages.PushBack(pages[1], allocator);

//    doc.AddMember("hugepages", hugepages, allocator);
//    doc.AddMember("memory", memory, allocator);
}
#endif


void Workers::onReady(void *arg)
{
    auto handle = static_cast<Handle*>(arg);

    IWorker *worker = new OclWorker(handle);
    handle->setWorker(worker);

    start(worker);
}


void Workers::onResult(uv_async_t *handle)
{
    JobBaton *baton = new JobBaton();

    uv_mutex_lock(&m_mutex);
    while (!m_queue.empty()) {
        baton->jobs.push_back(std::move(m_queue.front()));
        m_queue.pop_front();
    }
    uv_mutex_unlock(&m_mutex);

    uv_queue_work(uv_default_loop(), &baton->request,
        [](uv_work_t* req) {
            JobBaton *baton = static_cast<JobBaton*>(req->data);
            if (baton->jobs.empty()) {
                return;
            }

            cryptonight_ctx *ctx;
            MemInfo info = Mem::create(&ctx, baton->jobs[0].algorithm().algo(), 1);

            for (const xmrig::Job &job : baton->jobs) {
                xmrig::JobResult result(job);

                if (CryptoNight::hash(job, result, ctx)) {
                    baton->results.push_back(result);
                }
                else {
                    baton->errors++;
                }
            }

            Mem::release(&ctx, 1, info);
        },
        [](uv_work_t* req, int status) {
            JobBaton *baton = static_cast<JobBaton*>(req->data);

            for (const xmrig::JobResult &result : baton->results) {
                m_listener->onJobResult(result);
            }

            if (baton->errors > 0 && !baton->jobs.empty()) {
                LOG_ERR("THREAD #%d COMPUTE ERROR", baton->jobs[0].threadId());
            }

            delete baton;
        }
    );
}


void Workers::onTick(uv_timer_t *handle)
{
    for (Handle *handle : m_workers) {
        if (!handle->worker()) {
            return;
        }

        m_hashrate->add(handle->threadId(), handle->worker()->hashCount(), handle->worker()->timestamp());
    }

    if ((m_ticks++ & 0xF) == 0)  {
        m_hashrate->updateHighest();
    }
}


void Workers::start(IWorker *worker)
{
    worker->start();
}
