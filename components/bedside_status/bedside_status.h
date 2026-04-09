#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace bedside_status {

class BedsideStatus : public PollingComponent {
 public:
  BedsideStatus() : PollingComponent(5000) {}

  void setup() override;
  void update() override;
};

}  // namespace bedside_status
}  // namespace esphome