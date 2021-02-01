#pragma once

typedef   signed char sbyte;
typedef unsigned char ubyte;

typedef   signed char      s8;
typedef   signed short     s16;
typedef   signed int       s32;
typedef   signed long long s64;

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

#include <functional>

struct Defer
{
    template<typename T>
    Defer(T&& func) : m_Func(static_cast<std::function<void()>>(func)) {};
    ~Defer() { if (armed) m_Func(); }
    void disarm() { armed = false; }
private:
    std::function<void(void)> m_Func;
    bool armed = true;
};

#define _COMBINE(X,Y) X##Y
#define COMBINE(X,Y) _COMBINE(X,Y)
#define defer if (0); Defer COMBINE(Defer_,__LINE__) = [&]()

#define NULL 0
