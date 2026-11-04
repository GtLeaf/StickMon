// SPDX-License-Identifier: Apache-2.0
#include <cstdlib>
#include <limits>
#include "sdgoods_driver_fakes.h"
#include "Audio.h"
#include "PeerRadio.h"
#include "Battery.h"
#include "hardware/EspNowRadioConfig.h"

namespace Platform {
void logf(const char*, ...) {}
void logLine(const char*) {}
}
class Allocator final : public Platform::IMemoryAllocator {
    void* allocate(size_t size,bool) override { return std::malloc(size); }
    void release(void* data) override { std::free(data); }
    size_t externalFree() const override { return 1000000; }
};
int main() {
    {
        Sdgoods::Battery battery;
        Fake::adcFailUnit=true;assert(!battery.begin() && battery.level(0)==-1);
        Fake::adcFailUnit=false;Fake::adcFailChannel=true;assert(!battery.begin());
        assert(Fake::adcCreated==1 && Fake::adcDeleted==1);
        Fake::adcFailChannel=false;Fake::adcFailCalibration=true;assert(!battery.begin());
        assert(battery.level(2000)==-1);
        Fake::adcFailCalibration=false;assert(battery.begin() && battery.begin());
        assert(Fake::adcCreated==2 && Fake::calCreated==1);
        Fake::adcMv=1200;assert(battery.level(2010)==47);
        int reads=Fake::adcReads;
        assert(battery.level(4009)==47 && Fake::adcReads==reads);
        battery.poll(4010); assert(Fake::adcReads==reads+8);
        assert(!battery.chargingEstimate(4010) && battery.level(4010)==47 && Fake::adcReads==reads+8);
        Fake::adcSamples={900,1500,1200,1200,1200,1200,1200,1200};
        assert(battery.level(12010)==47); // trimmed spikes
        Fake::adcMv=1400;assert(battery.level(22010)==56); // smooth to 3720mV
        Fake::adcFailRead=true;assert(battery.level(32010)==-1);
        reads=Fake::adcReads;assert(battery.level(34009)==-1 && Fake::adcReads==reads);
        Fake::adcFailRead=false;Fake::adcMv=1080;assert(battery.level(34010)==19); // no stale smoothing
        Fake::adcSamples={0,4095,1200,1200,1200,1200,1200,1200};
        assert(battery.level(44010)>=19); // tolerate two failed samples
        Fake::adcSamples={0,4095,0,1200,1200,1200,1200,1200};assert(battery.level(54010)==-1);
        Fake::adcFailConvert=true;assert(battery.level(56010)==-1);
        Fake::adcFailConvert=false;Fake::adcMv=1501;assert(battery.level(58010)==-1);
        Fake::adcMv=1452;assert(battery.level(60010)==100);
    }
    assert(Fake::adcCreated==Fake::adcDeleted && Fake::calCreated==Fake::calDeleted);
    {
        Sdgoods::Battery battery;assert(battery.begin());
        assert(battery.level(0xfffffc00u)==100);
        const int reads=Fake::adcReads;
        assert(battery.level(0x3cfu)==100 && Fake::adcReads==reads);
        battery.level(0x3d0u);assert(Fake::adcReads==reads+8);
    }
    {
        Sdgoods::Battery battery; assert(battery.begin());
        const int reads=Fake::adcReads;
        for(unsigned i=0;i<20;++i) {
            Fake::adcMv=1200+int(i);
            battery.poll(i*2000);
            assert(battery.chargingEstimate(i*2000)==(i==19));
            if(i<5) assert(battery.level(i*2000)==47); // percent stays cached for 10s
        }
        assert(Fake::adcReads==reads+160); // menu polling and HUD share fresh samples
        battery.resetChargingEstimate(); assert(!battery.chargingEstimate(38000));
        for(unsigned i=20;i<40;++i) {
            Fake::adcMv=1200+int(i); battery.poll(i*2000);
        }
        assert(battery.chargingEstimate(78000));
        Fake::adcFailRead=true;
        assert(battery.level(80000)==-1 && !battery.chargingEstimate(80000));
        Fake::adcFailRead=false; Fake::adcMv=1200;
        assert(battery.level(82000)==47 && !battery.chargingEstimate(82000));
        for(unsigned i=42;i<62;++i) {
            Fake::adcMv=1200+int(i); battery.poll(i*2000);
        }
        assert(battery.chargingEstimate(122000));
        battery.poll(130000); assert(!battery.chargingEstimate(130000));
    }
    std::puts("PASS real Battery driver: ADC/calibration retry and cleanup, idempotence, cache/wraparound, spikes, smoothing, unknown and recovery");
    std::puts("PASS real Battery charging estimate: independent 2s polling, HUD query deduplication, 10s level cache, load reset, ADC failure and sampling gap");
    Allocator allocator; Sdgoods::Audio audio(allocator);
    assert(audio.begin() && !Fake::pa && audio.volume()==0 && Fake::stereoRate==16000);
    std::vector<uint8_t> samples(1024,255);
    assert(audio.playPcmU8Channel(samples.data(),samples.size(),16000,1,true));
    Fake::runAudio();
    assert(!Fake::pa && Fake::i2sWrites==0 && !fakeChannels[0].enabled);
    assert(audio.queuedPcm(1)==1); // one block partially retired at normal pace
    audio.setVolume(50); Fake::runAudio();
    assert(Fake::pa && Fake::i2sWrites==1 && fakeChannels[0].enabled);
    for(auto s:Fake::lastWrite)assert(s>0 && s<32767);
    audio.setVolume(0);assert(!Fake::pa);Fake::runAudio();
    assert(Fake::i2sWrites==1 && !fakeChannels[0].enabled && audio.volume()==0);
    audio.suspend(true);assert(!audio.playing() && !Fake::pa);
    assert(!audio.playPcmU8(samples.data(),samples.size(),16000));
    audio.suspend(false);assert(audio.volume()==0);
    assert(audio.playPcmU8(samples.data(),samples.size(),16000));
    audio.setVolume(20);Fake::failWrite=true;Fake::runAudio();
    assert(!Fake::pa && !audio.playing());Fake::failWrite=false;
    audio.setVolume(0);
    assert(audio.beginMicrophone() && audio.microphoneActive());
    std::vector<int16_t> recorded(600);
    assert(audio.recordMicrophone(recorded.data(),recorded.size(),22050));
    assert(recorded[0]==32767 && recorded[1]==-32768 && !audio.microphoneRecording());
    Fake::failRead=true;assert(!audio.recordMicrophone(recorded.data(),10,16000));
    assert(!audio.microphoneRecording() && !Fake::pa && audio.volume()==0);
    audio.endMicrophone();assert(!audio.microphoneActive() && !fakeChannels[1].enabled);
    std::puts("PASS real Audio driver: muted startup/PCM/PA, queue retirement, lock/resume, I2S failure and microphone clipping/failure");
    // Every initialization step may fail; teardown must leave no live radio.
    for(int failure=1;failure<=11;++failure) {
        Sdgoods::PeerRadio radio;
        Fake::failAt=failure;Fake::step=0;
        assert(!radio.enable() && !radio.active() && !Fake::wifi && !Fake::now);
        radio.end();
    }
    Fake::failAt=0;
    Sdgoods::PeerRadio radio;assert(radio.enable() && radio.enable());
    assert(!Fake::nvsEnabled && Fake::storage==WIFI_STORAGE_RAM && Fake::channel==EspNowRadioConfig::CHANNEL);
    uint8_t mac[]{1,2,3,4,5,6}, bytes[]{9,8,7};
    assert(radio.send(mac,bytes,3) && Fake::peers.size()==1);
    assert(!radio.send(mac,bytes,251) && !radio.send(nullptr,bytes,3));
    Platform::PeerPacket packet;
    esp_now_recv_info_t info{mac};
    fakeReceive(&info,bytes,3);assert(radio.receive(packet));
    assert(packet.length==3 && std::memcmp(packet.source,mac,6)==0 && packet.payload[0]==9);
    fakeReceive(&info,bytes,251);assert(!radio.receive(packet));
    for(int i=0;i<12;++i)fakeReceive(&info,bytes,3);
    int count=0;while(radio.receive(packet))++count;assert(count==8);
    const auto stale=fakeReceive;
    radio.end();stale(&info,bytes,3);
    assert(!radio.active() && !Fake::wifi && !Fake::now && !radio.receive(packet));
    assert(radio.enable() && !radio.receive(packet));radio.end();
    std::puts("PASS real PeerRadio driver: init failures clean up, RAM settings, bounded receive queue, teardown and reopen");
}
