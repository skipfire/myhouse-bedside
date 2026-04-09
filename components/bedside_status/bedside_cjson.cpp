/** cJSON is compiled in cjson_upstream.c.
 *  This translation unit is intentionally empty except for this comment. It remains so
 *  ESPHome/PlatformIO builds that still list bedside_cjson.cpp.o in an old Ninja graph
 *  (after upgrading this component) keep a valid source path. New builds pay a trivial
 *  extra compile step. You can remove this file after "Clean Build Files" if you prefer.
 */
namespace {
[[maybe_unused]] const int bedside_cjson_translation_unit_anchor = 0;
}  // namespace
