#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>
#include "Rgb565Transfer.h"

struct Pending { const uint16_t* pixels; std::vector<uint16_t> expected; };
struct FakeSemaphore {
    std::deque<Pending> pending;
    int completed = 0;
    bool timeout = false;
};
int xSemaphoreTake(SemaphoreHandle_t handle, unsigned) {
    auto& sem = *static_cast<FakeSemaphore*>(handle);
    if (sem.timeout) return 0;
    assert(!sem.pending.empty());
    auto work = sem.pending.front(); sem.pending.pop_front();
    assert(std::equal(work.expected.begin(), work.expected.end(), work.pixels));
    ++sem.completed;
    return 1;
}
struct Context {
    FakeSemaphore sem;
    const uint16_t* source;
    int calls=0, failAt=-1;
};
int submit(void* value,int x0,int y0,int x1,int y1,const uint16_t* pixels) {
    auto& ctx=*static_cast<Context*>(value);
    if (ctx.calls++==ctx.failAt) return ESP_FAIL;
    for (const auto& work : ctx.sem.pending) assert(work.pixels!=pixels);
    Pending work{pixels,{}};
    for(int y=y0;y<y1;++y)
        for(int x=x0;x<x1;++x) work.expected.push_back(ctx.source[y*9+x]);
    assert(std::equal(work.expected.begin(),work.expected.end(),pixels));
    ctx.sem.pending.push_back(work);
    return ESP_OK;
}
int main() {
    uint16_t source[9*11]; for(int i=0;i<99;++i) source[i]=uint16_t(i+1);
    uint16_t a[9*3+2], b[9*3+2];
    std::fill_n(a,29,0xa55a);std::fill_n(b,29,0xa55a);
    uint16_t* buffers[]={a+1,b+1};
    Context ctx{{},source};
    assert(Stickmon::submitRgb565Region(source,9,2,1,8,11,buffers,2,3,&ctx.sem,submit,&ctx)==ESP_OK);
    assert(ctx.calls==4 && ctx.sem.completed==4 && ctx.sem.pending.empty());
    assert(a[0]==0xa55a && a[28]==0xa55a && b[0]==0xa55a && b[28]==0xa55a);
    ctx.calls=0;ctx.failAt=1;
    assert(Stickmon::submitRgb565Region(source,9,1,1,8,11,buffers,2,3,&ctx.sem,submit,&ctx)==ESP_FAIL);
    assert(ctx.sem.pending.empty());
    ctx.calls=0;ctx.failAt=-1;ctx.sem.timeout=true;
    assert(Stickmon::submitRgb565Region(source,9,1,1,8,11,buffers,2,3,&ctx.sem,submit,&ctx)==ESP_ERR_TIMEOUT);
    assert(ctx.calls==2);
}
