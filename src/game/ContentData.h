// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#ifndef STICKMON_EXTERNAL_CONTENT
#define STICKMON_EXTERNAL_CONTENT 0
#endif
#if STICKMON_EXTERNAL_CONTENT
#define STICKMON_CONTENT_CONSTEXPR
#else
#define STICKMON_CONTENT_CONSTEXPR constexpr
#endif

// Explicit little-endian fields, never native structure or pointer layouts.
namespace ContentData {
struct Reader {
    const uint8_t* bytes;
    size_t length, position = 0;
    bool good = true;
    uint8_t byte() {
        if (position >= length) { good = false; return 0; }
        return bytes[position++];
    }
    template<class... T> bool operator()(T&... value);
};
struct Writer {
    void* context;
    void (*put)(void*, uint8_t);
    void byte(uint8_t value) { put(context, value); }
    bool good = true;
    template<class... T> bool operator()(T&... value);
};
template<class V, class T> void field(V& v, T& value) {
    if constexpr (std::is_array_v<T>) {
        for (auto& part : value) field(v, part);
    } else if constexpr (std::is_same_v<T, const char*>) {
        if constexpr (std::is_same_v<V, Reader>) {
            uint16_t n = 0; field(v, n);
            if (!n || n > 4096 || n > v.length - v.position ||
                v.bytes[v.position+n-1] != 0 ||
                std::memchr(v.bytes+v.position, 0, n-1)) { v.good = false; return; }
            value = reinterpret_cast<const char*>(v.bytes+v.position);
            v.position += n;
        } else {
            const size_t n = std::strlen(value)+1;
            if (n > 4096) { v.good = false; return; }
            uint16_t length = uint16_t(n); field(v, length);
            for (size_t i=0;i<n;++i) v.byte(uint8_t(value[i]));
        }
    } else if constexpr (std::is_same_v<T,float>) {
        uint32_t bits=0;
        if constexpr (std::is_same_v<V,Writer>) std::memcpy(&bits,&value,4);
        field(v,bits);
        if constexpr (std::is_same_v<V,Reader>) std::memcpy(&value,&bits,4);
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        static_assert(sizeof(T) <= 4, "content scalar too wide");
        if constexpr (std::is_same_v<V, Reader>) {
            uint32_t n=0;
            for (size_t i=0;i<sizeof(T);++i) n |= uint32_t(v.byte()) << (8*i);
            if constexpr (std::is_same_v<T,bool>) { if(n>1) v.good=false; }
            value = static_cast<T>(n);
        } else {
            for(size_t i=0;i<sizeof(T);++i) v.byte(uint8_t(uint32_t(value) >> (8*i)));
        }
    } else {
        contentFields(v, value); // ADL: the type owns its wire field order.
    }
}
template<class... T> bool Reader::operator()(T&... value) {
    (field(*this,value),...); return good;
}
template<class... T> bool Writer::operator()(T&... value) {
    (field(*this,value),...); return good;
}
using Decode = bool (*)(Reader&,void*,size_t);
class Source {
public:
    virtual ~Source() = default;
    virtual const void* table(uint16_t id, size_t itemSize, size_t maxCount,
                              Decode decode, size_t& count) = 0;
};
inline Source* source = nullptr;
inline void bind(Source* value) { source=value; }

template<class T, size_t Limit, bool Exact=true> class Table {
    uint16_t id_;
    static bool decode(Reader& r,void* output,size_t count) {
        auto* values=static_cast<T*>(output);
        for(size_t i=0;i<count && r.good;++i) field(r,values[i]);
        return r.good && r.position==r.length;
    }
public:
    using value_type=T;
    constexpr explicit Table(uint16_t id):id_(id) {}
    const T* data() const {
        size_t count=0;
        const auto* p=source ? static_cast<const T*>(source->table(id_,sizeof(T),Limit,decode,count)) : nullptr;
        return Exact && count!=Limit ? nullptr : p;
    }
    size_t size() const {
        size_t count=0;
        if(source) source->table(id_,sizeof(T),Limit,decode,count);
        return count;
    }
    const T* begin() const { return data(); }
    const T* end() const { const auto* p=data(); return p ? p+size() : p; }
    const T& operator[](size_t index) const { return data()[index]; }
    operator const T*() const { return data(); }
    bool ready() const { return data() && size(); }
};
} // namespace ContentData
