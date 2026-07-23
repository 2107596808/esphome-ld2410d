#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include <vector>

namespace esphome {
namespace ld2410d {

// HLK-LD2410D has 16 distance gates (each gate = 0.7 m).
static const uint8_t LD2410D_TOTAL_GATES = 16;

// Frame markers.
static const uint8_t DATA_FRAME_HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t DATA_FRAME_FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
static const uint8_t CMD_FRAME_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
static const uint8_t CMD_FRAME_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};

// Command words.
static const uint16_t CMD_READ_VERSION = 0x0000;
static const uint16_t CMD_ENABLE_CONFIG = 0x00FF;
static const uint16_t CMD_END_CONFIG = 0x00FE;
static const uint16_t CMD_READ_SN_STR = 0x0011;
static const uint16_t CMD_READ_PARAMS = 0x0008;
static const uint16_t CMD_SET_PARAMS = 0x0007;
static const uint16_t CMD_SET_OUTPUT_MODE = 0x0012;
static const uint16_t CMD_GENERATE_THRESHOLD = 0x0009;
static const uint16_t CMD_QUERY_THRESHOLD_PROGRESS = 0x000A;
static const uint16_t CMD_SAVE_PARAMS = 0x00FD;
static const uint16_t CMD_AUTO_GAIN = 0x00EE;

// Parameter IDs (see manual table 5-5).
static const uint16_t PARAM_MAX_DISTANCE = 0x0001;
static const uint16_t PARAM_TIMEOUT = 0x0004;
static const uint16_t PARAM_MOVE_THRESHOLD_BASE = 0x0010;  // gate 0..15
static const uint16_t PARAM_STILL_THRESHOLD_BASE = 0x0030;  // gate 0..15

class LD2410DComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Actions exposed to buttons / switches / numbers.
  void read_all_info();
  void query_parameters();
  void set_engineering_mode(bool enable);
  void generate_threshold();
  void auto_gain();

  void set_max_distance(float meters);
  void set_absence_timeout(uint16_t seconds);
  void set_move_threshold(uint8_t gate, float db);
  void set_still_threshold(uint8_t gate, float db);

  void set_trigger_coefficient(float value) { this->trigger_coefficient_ = value; }
  void set_hold_coefficient(float value) { this->hold_coefficient_ = value; }
  void set_micro_coefficient(float value) { this->micro_coefficient_ = value; }

#ifdef USE_BINARY_SENSOR
  void set_has_target_binary_sensor(binary_sensor::BinarySensor *s) { this->has_target_bs_ = s; }
  void set_moving_target_binary_sensor(binary_sensor::BinarySensor *s) { this->moving_target_bs_ = s; }
  void set_still_target_binary_sensor(binary_sensor::BinarySensor *s) { this->still_target_bs_ = s; }
#endif
#ifdef USE_SENSOR
  void set_detection_distance_sensor(sensor::Sensor *s) { this->detection_distance_sensor_ = s; }
  void set_move_energy_sensor(uint8_t gate, sensor::Sensor *s) { this->move_energy_sensors_[gate] = s; }
  void set_still_energy_sensor(uint8_t gate, sensor::Sensor *s) { this->still_energy_sensors_[gate] = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_version_text_sensor(text_sensor::TextSensor *s) { this->version_text_sensor_ = s; }
  void set_sn_text_sensor(text_sensor::TextSensor *s) { this->sn_text_sensor_ = s; }
#endif
#ifdef USE_NUMBER
  void set_max_distance_number(number::Number *n) { this->max_distance_number_ = n; }
  void set_timeout_number(number::Number *n) { this->timeout_number_ = n; }
  void set_move_threshold_number(uint8_t gate, number::Number *n) { this->move_threshold_numbers_[gate] = n; }
  void set_still_threshold_number(uint8_t gate, number::Number *n) { this->still_threshold_numbers_[gate] = n; }
#endif
#ifdef USE_SWITCH
  void set_engineering_mode_switch(switch_::Switch *s) { this->engineering_mode_switch_ = s; }
#endif

 protected:
  // --- UART frame handling ---
  std::vector<uint8_t> rx_buffer_;
  std::string ascii_line_;
  void process_buffer_();
  bool prefix_matches_(const uint8_t *header) const;
  void handle_data_frame_(size_t payload_len);
  void handle_ack_frame_(size_t payload_len);
  void handle_ascii_line_(const std::string &line);

  // --- command helpers ---
  void write_command_(uint16_t command, const std::vector<uint8_t> &value = {});
  void enter_config_mode_();
  void exit_config_mode_();
  bool wait_ack_(uint16_t command, uint32_t timeout_ms = 300);
  int32_t last_ack_command_ = -1;
  bool config_mode_ = false;

  // --- state publishing ---
  void publish_presence_(uint8_t detection_result);
  void publish_distance_(uint16_t distance_cm);

  // threshold coefficients for auto generation (1.0 .. 20.0)
  float trigger_coefficient_ = 3.0f;
  float hold_coefficient_ = 3.0f;
  float micro_coefficient_ = 3.0f;

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *has_target_bs_{nullptr};
  binary_sensor::BinarySensor *moving_target_bs_{nullptr};
  binary_sensor::BinarySensor *still_target_bs_{nullptr};
#endif
#ifdef USE_SENSOR
  sensor::Sensor *detection_distance_sensor_{nullptr};
  sensor::Sensor *move_energy_sensors_[LD2410D_TOTAL_GATES]{nullptr};
  sensor::Sensor *still_energy_sensors_[LD2410D_TOTAL_GATES]{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *version_text_sensor_{nullptr};
  text_sensor::TextSensor *sn_text_sensor_{nullptr};
#endif
#ifdef USE_NUMBER
  number::Number *max_distance_number_{nullptr};
  number::Number *timeout_number_{nullptr};
  number::Number *move_threshold_numbers_[LD2410D_TOTAL_GATES]{nullptr};
  number::Number *still_threshold_numbers_[LD2410D_TOTAL_GATES]{nullptr};
#endif
#ifdef USE_SWITCH
  switch_::Switch *engineering_mode_switch_{nullptr};
#endif
};

// ---------------------------------------------------------------------------
// Child entity classes
// ---------------------------------------------------------------------------

#ifdef USE_NUMBER
class MaxDistanceNumber : public number::Number, public Parented<LD2410DComponent> {
 public:
  MaxDistanceNumber() = default;

 protected:
  void control(float value) override { this->parent_->set_max_distance(value); }
};

class TimeoutNumber : public number::Number, public Parented<LD2410DComponent> {
 public:
  TimeoutNumber() = default;

 protected:
  void control(float value) override { this->parent_->set_absence_timeout(static_cast<uint16_t>(value)); }
};

class MoveThresholdNumber : public number::Number, public Parented<LD2410DComponent> {
 public:
  explicit MoveThresholdNumber(uint8_t gate) : gate_(gate) {}

 protected:
  void control(float value) override { this->parent_->set_move_threshold(this->gate_, value); }
  uint8_t gate_;
};

class StillThresholdNumber : public number::Number, public Parented<LD2410DComponent> {
 public:
  explicit StillThresholdNumber(uint8_t gate) : gate_(gate) {}

 protected:
  void control(float value) override { this->parent_->set_still_threshold(this->gate_, value); }
  uint8_t gate_;
};

class TriggerCoefficientNumber : public number::Number, public Parented<LD2410DComponent> {
 protected:
  void control(float value) override {
    this->parent_->set_trigger_coefficient(value);
    this->publish_state(value);
  }
};

class HoldCoefficientNumber : public number::Number, public Parented<LD2410DComponent> {
 protected:
  void control(float value) override {
    this->parent_->set_hold_coefficient(value);
    this->publish_state(value);
  }
};

class MicroCoefficientNumber : public number::Number, public Parented<LD2410DComponent> {
 protected:
  void control(float value) override {
    this->parent_->set_micro_coefficient(value);
    this->publish_state(value);
  }
};
#endif

#ifdef USE_BUTTON
class QueryParamsButton : public button::Button, public Parented<LD2410DComponent> {
 protected:
  void press_action() override { this->parent_->query_parameters(); }
};

class GenerateThresholdButton : public button::Button, public Parented<LD2410DComponent> {
 protected:
  void press_action() override { this->parent_->generate_threshold(); }
};

class AutoGainButton : public button::Button, public Parented<LD2410DComponent> {
 protected:
  void press_action() override { this->parent_->auto_gain(); }
};
#endif

#ifdef USE_SWITCH
class EngineeringModeSwitch : public switch_::Switch, public Parented<LD2410DComponent> {
 protected:
  void write_state(bool state) override {
    this->parent_->set_engineering_mode(state);
    this->publish_state(state);
  }
};
#endif

}  // namespace ld2410d
}  // namespace esphome
