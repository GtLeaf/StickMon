#pragma once
#include "platform/api/PlatformServices.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace Platform {
// One application thread owns this directory. No caller-supplied paths.
class PosixUserFiles : public IUserFiles {
public:
    explicit PosixUserFiles(std::string directory):directory_(std::move(directory)){}
    bool begin() override {
        if(::mkdir(directory_.c_str(),0700)!=0&&errno!=EEXIST)return false;
        struct stat s{};return ::stat(directory_.c_str(),&s)==0&&S_ISDIR(s.st_mode);
    }
    bool exists(const char* name) override {
        if(!valid(name))return true;
        struct stat s{};
        // Treat errors other than absence as occupied, never overwrite them.
        return ::stat(path(name).c_str(),&s)==0||errno!=ENOENT;
    }
    size_t size(const char* name) override {
        if(!valid(name))return 0;
        struct stat s{};return ::stat(path(name).c_str(),&s)==0&&S_ISREG(s.st_mode)?size_t(s.st_size):0;
    }
    bool read(const char* name,size_t offset,void* output,size_t length) override {
        if(!valid(name)||!output||offset>size(name)||length>size(name)-offset)return false;
        FILE* f=std::fopen(path(name).c_str(),"rb");if(!f)return false;
        const bool ok=std::fseek(f,long(offset),SEEK_SET)==0&&std::fread(output,1,length,f)==length;
        return std::fclose(f)==0&&ok;
    }
    bool writeNew(const char* name,const void* data,size_t length,FileYield yield=nullptr) override {
        if(!valid(name)||!data||!length||exists(name))return false;
        const auto destination=path(name),temporary=destination+".tmp";
        FILE* f=std::fopen(temporary.c_str(),"wb");if(!f)return false;
        bool ok=true;const auto* bytes=static_cast<const uint8_t*>(data);
        for(size_t done=0;done<length&&ok;){
            const size_t n=length-done<4096?length-done:4096;
            ok=std::fwrite(bytes+done,1,n,f)==n;done+=n;if(yield)yield();
        }
        if(ok)ok=std::fflush(f)==0&&::fsync(::fileno(f))==0;
        if(std::fclose(f)!=0)ok=false;
        // FAT rename is publish-only, so never unlink an existing destination.
        if(ok&&!exists(name))ok=::rename(temporary.c_str(),destination.c_str())==0;
        else ok=false;
        if(!ok)::unlink(temporary.c_str());
        return ok;
    }
    bool remove(const char* name) override {return valid(name)&&::unlink(path(name).c_str())==0;}
protected:
    const std::string& directory()const{return directory_;}
private:
    std::string directory_;
    static bool valid(const char* s){
        if(!s||!*s||std::strlen(s)>32||s[0]=='.')return false;
        for(const char* p=s;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return false;
        return std::strstr(s,"..")==nullptr;
    }
    std::string path(const char* name)const{return directory_+"/"+name;}
};
}
