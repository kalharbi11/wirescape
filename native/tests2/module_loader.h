// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape/module.h"
#include "wirescape/module2.h"
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
struct ModuleLoader {
#ifdef _WIN32
    using Handle=HMODULE;
#else
    using Handle=void*;
#endif
    std::string directory;std::vector<Handle>handles;
    explicit ModuleLoader(const char*path):directory(path){}
    ~ModuleLoader(){for(auto h:handles){
#ifdef _WIN32
        FreeLibrary(h);
#else
        dlclose(h);
#endif
    }}
    Handle library(const std::string&name){
#ifdef _WIN32
        auto path=directory+"/"+name+".dll";auto h=LoadLibraryA(path.c_str());
#elif __APPLE__
        auto path=directory+"/lib"+name+".dylib";auto h=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
#else
        auto path=directory+"/lib"+name+".so";auto h=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
#endif
        if(!h)throw std::runtime_error("Cannot load "+path);handles.push_back(h);return h;
    }
    static void*symbol(Handle h,const char*name){
#ifdef _WIN32
        return reinterpret_cast<void*>(GetProcAddress(h,name));
#else
        return dlsym(h,name);
#endif
    }
    const ws_module_descriptor*abi1(const std::string&name){auto h=library("ws_"+name);auto entry=reinterpret_cast<ws_module_entry_fn>(symbol(h,"ws_module_entry"));if(!entry)throw std::runtime_error("Missing ABI1 entry");return entry();}
    const ws2_descriptor*abi2(const std::string&name){auto h=library("ws2_"+name);auto entry=reinterpret_cast<ws2_entry_fn>(symbol(h,"ws_module2_entry"));auto version=reinterpret_cast<uint32_t(*)()>(symbol(h,"ws_module2_abi_version"));if(!entry||!version||version()!=2)throw std::runtime_error("Missing/mismatched ABI2 entry");return entry();}
};
