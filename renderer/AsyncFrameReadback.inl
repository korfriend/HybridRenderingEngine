// Private ExecuteCustomModule contract RenderReadbackV1. See docs/async-readback.md.
#include <wrl/client.h>
#include <deque>
#include <memory>
#include <atomic>
#include <chrono>

namespace presentation_telemetry {
// Process-lifetime counters: snapshots never consume or reset another caller's data.
static std::atomic<uint64_t> calls{0}, wait_ns{0}, polls{0};
static void Record(std::chrono::steady_clock::time_point start,uint64_t spins) {
    wait_ns.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-start).count()),std::memory_order_relaxed);
    polls.fetch_add(spins,std::memory_order_relaxed);calls.fetch_add(1,std::memory_order_relaxed);
}
}

// ExecuteCustomModule: version:int=1, action:string="snapshot". No resources retained.
// status:int 0=success, 3=unavailable, 4=invalid typed request; durations are CPU wall time.
__vmstatic bool PresentationTelemetryV1(vmobjects::VmParamMap<std::string,std::any>&,
    vmobjects::VmParamMap<std::string,std::any>&,vmobjects::VmParamMap<std::string,std::any>& p)
{
    const int* version=nullptr;const std::string* action=nullptr;
    for(auto& entry:p){if(entry.first=="version")version=std::any_cast<int>(&entry.second);
        else if(entry.first=="action")action=std::any_cast<std::string>(&entry.second);}
    const bool valid=version&&*version==1&&action&&*action=="snapshot";
    p.SetParam("contract_version",1);p.SetParam("status",!valid?4:!g_moduleInitialized?3:0);
    if(valid&&g_moduleInitialized){
        p.SetParam("wait_ms",double(presentation_telemetry::wait_ns.load(std::memory_order_relaxed))/1e6);
        p.SetParam("calls",presentation_telemetry::calls.load(std::memory_order_relaxed));
        p.SetParam("polls",presentation_telemetry::polls.load(std::memory_order_relaxed));
    }
    return true;
}

namespace async_readback {
using Microsoft::WRL::ComPtr;
struct Source { ComPtr<ID3D11Texture2D> texture; bool empty=false; };
struct Slot { ComPtr<ID3D11Texture2D> texture; uint64_t token=0; bool empty=false; };
struct Queue { std::vector<Slot> slots; std::deque<size_t> pending; int width=0,height=0; };
static std::map<int,Source> sources;
static std::map<int,Queue> queues;
static void Release(int id) { queues.erase(id); sources.erase(id); }
static void Clear() { queues.clear(); sources.clear(); }
static void Finalize(VmIObject* image,ID3D11Texture2D* texture,bool empty) {
#ifndef DX10_0
    sources[image->GetObjectID()]={texture,empty};
#endif
}
template<class T> const T* Read(vmobjects::VmParamMap<std::string,std::any>& map,const char* name) {
    for(auto& entry:map)if(entry.first==name)return std::any_cast<T>(&entry.second);
    return nullptr;
}
}

__vmstatic bool RenderReadbackV1(vmobjects::VmParamMap<std::string,std::any>&,
    vmobjects::VmParamMap<std::string,std::any>& actors,
    vmobjects::VmParamMap<std::string,std::any>& p)
{
    using namespace async_readback;
    p.SetParam("contract_version",1);
    auto result=[&](int status){p.SetParam("status",status);return true;};
    const auto* version=Read<int>(p,"version");const auto* action=Read<std::string>(p,"action");
    if(!version||*version!=1||!action)return result(4);
#ifdef DX10_0
    return result(3);
#else
    if(!g_moduleInitialized||!g_psoManager.dx11Device)return result(3);
    EnterCriticalSection(&cs);
    struct Unlock { ~Unlock(){LeaveCriticalSection(&cs);} } unlock;
    try {
        if(*action=="probe")return result(0);
        const auto* target=Read<VmIObject*>(actors,"camera");
        if(!target||!*target)return result(4);
        const int id=(*target)->GetObjectID();
        if(*action=="reset"){queues.erase(id);return result(0);}
        auto* context=g_psoManager.dx11DeviceImmContext;
        if(*action=="poll") {
            auto found=queues.find(id);
            if(found==queues.end()||found->second.pending.empty())return result(1);
            auto& q=found->second;auto& slot=q.slots[q.pending.front()];
            std::vector<uint8_t> rgba;
            if(!slot.empty) {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                const HRESULT hr=context->Map(slot.texture.Get(),0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
                if(hr==DXGI_ERROR_WAS_STILL_DRAWING)return result(1);
                if(FAILED(hr))return result(5);
                try {rgba.resize(size_t(q.width)*q.height*4);}
                catch(...){context->Unmap(slot.texture.Get(),0);return result(5);}
                for(int y=0;y<q.height;++y)std::memcpy(rgba.data()+size_t(y)*q.width*4,
                    static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(q.width)*4);
                context->Unmap(slot.texture.Get(),0);
            }else rgba.resize(size_t(q.width)*q.height*4);
            p.SetParam("rgba",std::make_shared<std::vector<uint8_t>>(std::move(rgba)));p.SetParam("token",slot.token);
            p.SetParam("width",q.width);p.SetParam("height",q.height);
            slot.token=0;q.pending.pop_front();return result(0);
        }
        if(*action!="enqueue")return result(4);
        const auto* token=Read<uint64_t>(p,"frame_token");
        const auto* w=Read<int>(p,"width");const auto* h=Read<int>(p,"height");
        auto source=sources.find(id);vmint2 size;(*target)->GetFrameBufferInfo(&size);
        if(!token||!*token||!w||!h||*w<2||*h<2||*w>3840||*h>3840||
            size.x!=*w||size.y!=*h||source==sources.end()||!source->second.texture)return result(4);
        D3D11_TEXTURE2D_DESC desc{};source->second.texture->GetDesc(&desc);
        if(desc.Width!=*w||desc.Height!=*h||desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM||desc.SampleDesc.Count!=1)return result(4);
        auto& q=queues[id];
        if(q.width!=*w||q.height!=*h){if(!q.pending.empty())return result(4);q=Queue{};q.width=*w;q.height=*h;}
        const size_t capacity=std::max(size_t(1),std::min(size_t(3),size_t(128)*1024*1024/(size_t(*w)*(*h)*4)));
        if(q.pending.size()>=capacity)return result(2);
        size_t index=0;for(;index<q.slots.size()&&q.slots[index].token;++index){}
        if(index==q.slots.size()){
            Slot slot;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
            if(FAILED(g_psoManager.dx11Device->CreateTexture2D(&desc,nullptr,&slot.texture)))return result(5);
            q.slots.push_back(std::move(slot));
        }
        auto& slot=q.slots[index];slot.token=*token;slot.empty=source->second.empty;
        if(!slot.empty)context->CopyResource(slot.texture.Get(),source->second.texture.Get());
        q.pending.push_back(index);
        // Submit without waiting: standalone/minimized callers have no Present to flush work.
        context->Flush();p.SetParam("capacity",int(capacity));return result(0);
    }catch(...){return result(5);}
#endif
}
