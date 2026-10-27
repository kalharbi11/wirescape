// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
struct SoakStats {
 double min,median,p99,p999,max;size_t over_half,over_period;
 explicit SoakStats(std::vector<double>&times,double period){
  if(times.empty())throw std::invalid_argument("empty timing sample");
  over_half=std::count_if(times.begin(),times.end(),[&](double t){return t>period*.5;});
  over_period=std::count_if(times.begin(),times.end(),[&](double t){return t>period;});
  std::sort(times.begin(),times.end());
  auto percentile=[&](double q){return times[static_cast<size_t>(std::ceil(q*times.size()))-1];};
  min=times.front();max=times.back();p99=percentile(.99);p999=percentile(.999);
  size_t n=times.size();median=n%2?times[n/2]:(times[n/2-1]+times[n/2])*.5;
 }
};
