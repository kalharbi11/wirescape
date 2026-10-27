# New module template

Copy `submissions/example_gain/` into a new folder under `submissions/`. Keep the four required files together. Use a unique dotted module ID and a matching `ws_...` target. Module CMake should call `wirescape_add_module(target)` only; the host root creates a CTest executable from `tests.cpp` and dynamically loads the built library. Start with the example's port/parameter descriptor and offline lifecycle tests, then add behavior-specific checks.
