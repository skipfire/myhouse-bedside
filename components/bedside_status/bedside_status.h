#pragma once
#include "esphome/core/component.h"

namespace esphome {
	namespace bedside_status {
		class BedsideStatus : public Component {
		 public:
		  void setup() override;
		  void loop() override;
		};
	}  // namespace bedside_status
}  // namespace esphome
