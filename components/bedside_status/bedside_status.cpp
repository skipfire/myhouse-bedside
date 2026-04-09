#include "bedside_status.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bedside_status {

static const char *const TAG = "bedside_status";

void BedsideStatus::setup() {
  ESP_LOGI(TAG, "BedsideStatus setup()");
}

void BedsideStatus::update() {
  ESP_LOGI(TAG, "BedsideStatus update()");
}

}  // namespace bedside_status
}  // namespace esphome