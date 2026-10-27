// SPDX-License-Identifier: MIT
#pragma once
#if !defined(WS_SIMD_SCALAR) && (defined(__SSE2__) || defined(_M_X64))
#include <emmintrin.h>
#define WS_SIMD_SSE2 1
#elif !defined(WS_SIMD_SCALAR) && defined(__ARM_NEON)
#include <arm_neon.h>
#define WS_SIMD_NEON 1
#endif
namespace ws::simd {
struct f4 {
#if defined(WS_SIMD_SSE2)
    __m128 v;
    f4():v(_mm_setzero_ps()){} explicit f4(float x):v(_mm_set1_ps(x)){}
    explicit f4(__m128 x):v(x){}
    static f4 load(const float*p){return f4(_mm_loadu_ps(p));}
    void store(float*p)const{_mm_storeu_ps(p,v);}
    friend f4 operator+(f4 a,f4 b){return f4(_mm_add_ps(a.v,b.v));}
    friend f4 operator-(f4 a,f4 b){return f4(_mm_sub_ps(a.v,b.v));}
    friend f4 operator*(f4 a,f4 b){return f4(_mm_mul_ps(a.v,b.v));}
#elif defined(WS_SIMD_NEON)
    float32x4_t v;
    f4():v(vdupq_n_f32(0)){} explicit f4(float x):v(vdupq_n_f32(x)){}
    explicit f4(float32x4_t x):v(x){}
    static f4 load(const float*p){return f4(vld1q_f32(p));}
    void store(float*p)const{vst1q_f32(p,v);}
    friend f4 operator+(f4 a,f4 b){return f4(vaddq_f32(a.v,b.v));}
    friend f4 operator-(f4 a,f4 b){return f4(vsubq_f32(a.v,b.v));}
    friend f4 operator*(f4 a,f4 b){return f4(vmulq_f32(a.v,b.v));}
#else
    float v[4]{};
    f4()=default; explicit f4(float x){for(auto&y:v)y=x;}
    static f4 load(const float*p){f4 x;for(int i=0;i<4;++i)x.v[i]=p[i];return x;}
    void store(float*p)const{for(int i=0;i<4;++i)p[i]=v[i];}
    friend f4 operator+(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]+=b.v[i];return a;}
    friend f4 operator-(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]-=b.v[i];return a;}
    friend f4 operator*(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]*=b.v[i];return a;}
#endif
};
}
