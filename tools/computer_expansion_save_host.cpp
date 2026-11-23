#include "core/SaveCodec.h"
#include "core/SaveManager.h"
#include "game/StorageCapacity.h"
#include "game/MarketShopService.h"
#include "game/MarketTransaction.h"
#include "game/MonsterFactory.h"
#include "platform/desktop/DesktopPlatform.h"
#include "platform/common/PosixUserFiles.h"
#include <cassert>
#include <fstream>
#include <vector>
#include <cstring>
#include <new>
#include "save_schema1_fabricator.h"
using namespace Game;
static size_t blockedAllocation=0;
void* operator new(size_t n,const std::nothrow_t&)noexcept {if(n==blockedAllocation)return nullptr;try{return ::operator new(n);}catch(...){return nullptr;}}
void* operator new[](size_t n,const std::nothrow_t&)noexcept {if(n==blockedAllocation)return nullptr;try{return ::operator new[](n);}catch(...){return nullptr;}}
struct Blobs:Platform::IBlobStore{Platform::IBlobStore& b;bool failMarker=false,failClear=false,failLookup=false;explicit Blobs(Platform::IBlobStore& x):b(x){}bool initialize()override{return b.initialize();}bool hasAuthoritativeLookup()const override{return true;}size_t blobSize(const char*n,const char*k)override{return b.blobSize(n,k);}Platform::BlobLookup lookupBlob(const char*n,const char*k,size_t&l)override{if(failLookup){l=0;return Platform::BlobLookup::FAILED;}return b.lookupBlob(n,k,l);}bool readBlob(const char*n,const char*k,void*p,size_t s)override{return b.readBlob(n,k,p,s);}bool writeBlob(const char*n,const char*k,const void*p,size_t s)override{return s<=4096&&!(failMarker&&std::strcmp(k,"save_archive")==0)&&b.writeBlob(n,k,p,s);}bool removeBlob(const char*n,const char*k)override{return b.removeBlob(n,k);}bool clearNamespace(const char*n)override{return !failClear&&b.clearNamespace(n);}};

struct Files:Platform::PosixUserFiles{using PosixUserFiles::PosixUserFiles;bool failWrite=false,failBegin=false;bool begin()override{return !failBegin&&PosixUserFiles::begin();}bool writeNew(const char*n,const void*p,size_t s,Platform::FileYield y=nullptr)override{return !failWrite&&PosixUserFiles::writeNew(n,p,s,y);}};
static std::vector<uint8_t> fixture(const char*name){std::ifstream f(std::string("tools/fixtures/")+name,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){assert(argc==2);DesktopPlatform desktop("data");auto base=desktop.serviceBundle();Blobs blobs(base.blobs);std::string dir=std::string(argv[1])+"/archive.XXXXXX";assert(mkdtemp(dir.data()));Files files(dir);Platform::Services services{base.lifecycle,base.clock,base.logger,base.input,base.display,base.audio,base.imu,base.power,blobs,base.resources,base.memory,base.peers,&files};Platform::bind(services);assert(desktop.begin());SaveManager saves;assert(saves.begin());
    auto portable=fixture("computer-expansion-schema10.bin");SaveCodec::Snapshot legacy;assert(SaveCodec::decode(portable.data(),portable.size(),legacy));assert(legacy.state.storageCount==20&&legacy.state.storage[19].metAt==119&&legacy.state.coins==7000&&legacy.state.bag.potion==7&&legacy.state.careRewards[21].budget==73&&StorageCapacity::capacity(legacy.state)==20);
    auto raw=fixture("computer-expansion-raw11-pity.bin");assert(Platform::blobs().writeBlob("stickmon","state",raw.data(),raw.size()));GameState state;MainSceneViewState view;assert(saves.load(state,view));assert(state.friendshipPity.entries[0].speciesId==151&&state.friendshipPity.entries[0].failures==3);assert(state.storageCount==20&&state.storage[19].metAt==119&&state.coins==7000&&state.marketSupply.pendingAdvice==3&&state.careRewards[21].budget==73&&state.storageUpgrades==0);
    state.storageUpgrades=9;state.storageCount=200;state.storageExpansionPeriod=28;state.coins=1337;
    for(unsigned i=0;i<200;++i){state.storage[i]=MonsterFactory::create(25,5);state.storage[i].ivPacked=i+1;state.storage[i].metAt=100+i;}
    for(unsigned i=0;i<CARE_REWARD_RECORD_CAP;++i){state.careRewards[i].valid=true;state.careRewards[i].metAt=i+1;state.careRewards[i].budget=100;state.careRewards[i].awarded[0]=i%50;}
    std::vector<uint8_t> encoded(SaveCodec::MAX_ENCODED_BYTES);size_t length=0;assert(SaveCodec::encode(state,view,99,encoded.data(),encoded.size(),length));assert(length>4096&&length<=encoded.size());SaveCodec::Snapshot decoded;assert(SaveCodec::decode(encoded.data(),length,decoded));assert(decoded.state.storage[199].metAt==299&&decoded.state.careRewards[201].metAt==202&&decoded.state.storageUpgrades==9&&decoded.state.storageExpansionPeriod==28);
    assert(saves.saveSnapshot(state,view));assert(files.exists("save-a.bin")||files.exists("save-b.bin"));
    GameState loaded;assert(saves.load(loaded,view));assert(loaded.storageCount==200&&loaded.storage[199].metAt==299&&loaded.coins==1337&&loaded.storageUpgrades==9);
    for(const auto* missing:{"save-a.bin","save-b.bin"}){
        state.coins=1337;assert(saves.saveSnapshot(state,view));assert(files.remove(missing));
        files.failWrite=true;state.coins=42;assert(!saves.saveSnapshot(state,view));files.failWrite=false;
        assert(saves.load(loaded,view)&&loaded.coins==1337&&loaded.storageCount==200);
    }
    files.failWrite=true;state.coins=42;assert(!saves.saveSnapshot(state,view));files.failWrite=false;assert(saves.load(loaded,view)&&loaded.coins==1337);
    state.coins=1337;assert(saves.saveSnapshot(state,view));assert(saves.snapshotHealthy());
    for(auto size:{sizeof(SaveCodec::Snapshot),files.size("save-a.bin")}){blockedAllocation=size;assert(!saves.load(loaded,view));assert(!saves.saveSnapshot(state,view));blockedAllocation=0;assert(saves.load(loaded,view)&&loaded.coins==1337);}
    blobs.failLookup=true;assert(!saves.load(loaded,view));assert(!saves.saveSnapshot(state,view));blobs.failLookup=false;
    auto corrupt=std::vector<uint8_t>(encoded.begin(),encoded.begin()+length);corrupt[4]=12;
    assert(files.remove("save-b.bin"));assert(files.writeNew("save-b.bin",corrupt.data(),corrupt.size()));assert(saves.load(loaded,view)&&loaded.coins==1337);
    auto future=corrupt;future.resize(SaveCodec::MAX_ENCODED_BYTES+256,0);const unsigned payload=future.size()-16;future[12]=payload&255;future[13]=payload>>8;auto crc=SaveTestUtil::blobCrc16(future.data(),future.size());future[14]=crc&255;future[15]=crc>>8;
    assert(files.remove("save-b.bin"));assert(files.writeNew("save-b.bin",future.data(),future.size()));SaveManager::LoadStatus status{};assert(!saves.load(loaded,view,nullptr,&status)&&status==SaveManager::LoadStatus::NEWER_VERSION);assert(!saves.snapshotHealthy()&&!saves.saveSnapshot(state,view));assert(files.remove("save-b.bin"));assert(saves.saveSnapshot(state,view));
    GameState empty{};std::vector<uint8_t> malformed(SaveCodec::MAX_ENCODED_BYTES);size_t malformedSize=0;assert(SaveCodec::encode(empty,{},1,malformed.data(),malformed.size(),malformedSize));malformed[malformedSize-84-1]=181;crc=SaveTestUtil::blobCrc16(malformed.data(),malformedSize);malformed[14]=crc&255;malformed[15]=crc>>8;assert(!SaveCodec::decode(malformed.data(),malformedSize,decoded));
    blobs.failClear=true;assert(!saves.clearAll());blobs.failClear=false;assert(saves.load(loaded,view)&&loaded.storageCount==200&&loaded.coins==1337);
    files.failBegin=true;assert(!saves.load(loaded,view));assert(!saves.saveSnapshot(state,view));files.failBegin=false;
    assert(files.writeNew("keep.photo","x",1));assert(saves.clearAll());assert(!files.exists("save-a.bin")&&!files.exists("save-b.bin")&&files.exists("keep.photo"));
    auto first=legacy.state;first.gameMinutesTotal=0;first.marketSeason.periodDay=0;assert(saves.saveSnapshot(first,view));const auto firstBefore=first;blobs.failMarker=true;
    auto tx=MarketTransaction::commit(first,[&](auto&x){return MarketShop::buy(x,MarketShop::EXPANSION,0);},[&]{return saves.saveSnapshot(first,view);},[](auto){});
    assert(tx.status==MarketTransaction::Status::SAVE_FAILED&&std::memcmp(&first,&firstBefore,sizeof(first))==0);assert(saves.load(loaded,view)&&loaded.storageUpgrades==0&&loaded.coins==7000);blobs.failMarker=false;
    assert(MarketShop::buy(first,MarketShop::EXPANSION,0)==MarketShop::Result::OK&&saves.saveSnapshot(first,view));assert(files.remove("save-a.bin")&&files.remove("save-b.bin"));assert(!saves.load(loaded,view)&&!saves.snapshotHealthy()&&!saves.saveSnapshot(first,view));assert(saves.clearAll());
    state.storageUpgrades=10;assert(!SaveCodec::encode(state,view,1,encoded.data(),encoded.size(),length));
    std::printf("computer-expansion-save: golden legacy formats, 200 companions, 202 care records, archive/rollback/mount failure/reset OK (%zu bytes)\n",encoded.size());
}
