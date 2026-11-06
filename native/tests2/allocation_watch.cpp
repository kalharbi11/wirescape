// SPDX-License-Identifier: MIT
#include "allocation_watch.h"
#include <cstdlib>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace allocation_watch {
std::atomic<bool> enabled{false};
std::atomic<uint64_t> count{0};
thread_local bool thread_enabled = false;
thread_local uint64_t thread_count = 0;
} // namespace allocation_watch
static void record() {
    if (allocation_watch::enabled.load(std::memory_order_relaxed)) ++allocation_watch::count;
    if (allocation_watch::thread_enabled) ++allocation_watch::thread_count;
}
void*operator new(size_t n){record();if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void*operator new(size_t n,std::align_val_t a){record();void*p=nullptr;
#ifdef _WIN32
 p=_aligned_malloc(n?n:1,static_cast<size_t>(a));
#else
 if(posix_memalign(&p,static_cast<size_t>(a),n?n:1))p=nullptr;
#endif
 if(!p)throw std::bad_alloc();return p;}
void*operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t)noexcept{
#ifdef _WIN32
 _aligned_free(p);
#else
 std::free(p);
#endif
}
void operator delete[](void*p,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete(void*p,size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
void operator delete[](void*p,size_t,std::align_val_t a)noexcept{::operator delete(p,a);}
void*operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void*operator new[](size_t n,const std::nothrow_t&)noexcept{return ::operator new(n,std::nothrow);}
void*operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void*operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{return ::operator new(n,a,std::nothrow);}
void operator delete(void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{::operator delete(p);}
void operator delete(void*p,std::align_val_t a,const std::nothrow_t&)noexcept{::operator delete(p,a);}
void operator delete[](void*p,std::align_val_t a,const std::nothrow_t&)noexcept{::operator delete(p,a);}
