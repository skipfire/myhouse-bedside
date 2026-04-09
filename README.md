# myhouse-bedside

ESPHome external component for the Elecrow CrowPanel 5.79″ bedside status display.

**ESP-IDF:** `bedside_status` uses `esp_http_client`. Add `framework.advanced.include_builtin_idf_components: [esp_http_client]` under `esp32` (see `bedside_example.yaml` and `BUILD_NOTES.txt`).