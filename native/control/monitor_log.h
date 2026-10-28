// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include "../modules2/monitor/tap.h"
#include <string>
std::string format_monitor_entry(const AppMonitorTap&,const wirescape::monitor::Entry&);
