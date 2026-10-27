// Test-only library: valid descriptor whose preparation deliberately fails.
#define WS_MODULE_ENTRY_NAME original_entry
#include "../../modules/vca/module.cpp"
namespace { int WS_CALL fail_prepare(void*, const ws_module_config*) { return -1; } }
extern "C" WS_EXPORT const ws_module_descriptor* WS_CALL ws_module_entry() {
    static const auto failure = [] {
        auto d = *original_entry(); d.id = "test.prepare_failure"; d.name = "Prepare failure";
        d.prepare = fail_prepare; return d;
    }();
    return &failure;
}
