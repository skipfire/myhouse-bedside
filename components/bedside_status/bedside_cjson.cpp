// Bundles Dave Gamble's cJSON (MIT) into one C++ TU for ESP-IDF builds that only compile .cpp.
// Source: https://github.com/DaveGamble/cJSON v1.7.18
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
extern "C" {
#include "cjson_upstream.inc"
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
