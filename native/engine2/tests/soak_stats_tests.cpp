// SPDX-License-Identifier: GPL-3.0-only
#include "soak_stats.h"
#include <cstdio>
int main(){
 std::vector<double>v;for(int i=1000;i>0;--i)v.push_back(i);
 SoakStats s(v,1000);
 if(s.min!=1||s.max!=1000||s.median!=500.5||s.p99!=990||s.p999!=999||s.over_half!=500||s.over_period!=0)return 1;
 std::vector<double>one{12};SoakStats t(one,10);
 if(t.median!=12||t.p999!=12||t.over_half!=1||t.over_period!=1)return 1;
 try{std::vector<double>empty;SoakStats bad(empty,10);return 1;}catch(const std::invalid_argument&){}
 std::puts("PASS nearest-rank tails, median, strict overrun boundaries, empty sample");
}
