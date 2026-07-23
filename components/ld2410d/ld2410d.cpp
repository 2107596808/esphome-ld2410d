#include "ld2410d.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>

namespace esphome {
namespace ld2410d {

static const char *const TAG = "ld2410d";

// energy (raw counts) -> dB  :  dB = 10 * log10(raw)
static float raw_to_db(uint32_t raw) {
  if (raw == 0)
    return 0.0f;
  return 10.0f * log10f(static_cast<float>(raw));
}

// dB -> energy (raw counts)  :  raw = 10 ^ (dB / 10)
static uint32_t db_to_raw(float db) {
  if (db <= 0.0f)
    return 0;
  double raw = pow(10.0, db / 10.0);
  if (raw > 4294967295.0)
    raw = 4294967295.0;
  return static_cast<uint32_t>(raw + 0.5);
}

void LD2410DComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HLK-LD2410D...");
  // Give the module a moment after boot, then read its identity/parameters.
  this->set_timeout(600, [this]() { this->read_all_info(); });
}

void LD2410DComponent::loop() {
  const uint32_t start = millis();
  // Keep loop bounded: read at most for a few ms per iteration.
  while (this->available() && (millis() - start) < 15) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->rx_buffer_.push_back(byte);
    this->process_buffer_();
  }
}

bool LD2410DComponent::prefix_matches_(const uint8_t *header) const {
  size_t n = std::min<size_t>(4, this->rx_buffer_.size());
  for (size_t i = 0; i < n; i++) {
    if (this->rx_buffer_[i] != header[i])
      return false;
  }
  return true;
}

void LD2410DComponent::process_buffer_() {
  // Safety valve against runaway buffers.
  if (this->rx_buffer_.size() > 512)
    this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 256);

  while (!this->rx_buffer_.empty()) {
    uint8_t first = this->rx_buffer_.front();

    if (first == DATA_FRAME_HEADER[0]) {
      if (!this->prefix_matches_(DATA_FRAME_HEADER)) {
        this->rx_buffer_.erase(this->rx_buffer_.begin());
        continue;
      }
      if (this->rx_buffer_.size() < 6)
        return;  // need length field
      uint16_t len = this->rx_buffer_[4] | (this->rx_buffer_[5] << 8);
      size_t total = 6 + len + 4;
      if (this->rx_buffer_.size() < total)
        return;  // wait for full frame
      if (std::memcmp(&this->rx_buffer_[6 + len], DATA_FRAME_FOOTER, 4) == 0) {
        this->handle_data_frame_(len);
      } else {
        ESP_LOGW(TAG, "Bad data-frame footer");
      }
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total);
      continue;
    }

    if (first == CMD_FRAME_HEADER[0]) {
      if (!this->prefix_matches_(CMD_FRAME_HEADER)) {
        this->rx_buffer_.erase(this->rx_buffer_.begin());
        continue;
      }
      if (this->rx_buffer_.size() < 6)
        return;
      uint16_t len = this->rx_buffer_[4] | (this->rx_buffer_[5] << 8);
      size_t total = 6 + len + 4;
      if (this->rx_buffer_.size() < total)
        return;
      if (std::memcmp(&this->rx_buffer_[6 + len], CMD_FRAME_FOOTER, 4) == 0) {
        this->handle_ack_frame_(len);
      } else {
        ESP_LOGW(TAG, "Bad ACK-frame footer");
      }
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total);
      continue;
    }

    // ASCII normal-mode output ("OFF" / "distance: XX").
    if (first == '\r' || first == '\n') {
      if (!this->ascii_line_.empty()) {
        this->handle_ascii_line_(this->ascii_line_);
        this->ascii_line_.clear();
      }
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      continue;
    }
    if (std::isprint(first)) {
      this->ascii_line_.push_back(static_cast<char>(first));
      this->rx_buffer_.erase(this->rx_buffer_.begin());
      if (this->ascii_line_.size() > 64)
        this->ascii_line_.clear();  // not a real ASCII line, resync
      continue;
    }

    // Unknown byte - drop it to resynchronise.
    this->rx_buffer_.erase(this->rx_buffer_.begin());
  }
}

void LD2410DComponent::handle_ascii_line_(const std::string &line) {
  // Normal working mode: "OFF" when empty, "distance: XX" when a target is present.
  std::string lower = line;
  for (auto &c : lower)
    c = static_cast<char>(std::tolower(c));

  if (lower.find("off") != std::string::npos) {
    this->publish_presence_(0);
    this->publish_distance_(0);
    return;
  }
  size_t pos = lower.find("distance");
  if (pos != std::string::npos) {
    // extract the first integer after the keyword
    int value = 0;
    bool found = false;
    for (size_t i = pos; i < line.size(); i++) {
      if (std::isdigit(static_cast<unsigned char>(line[i]))) {
        value = value * 10 + (line[i] - '0');
        found = true;
      } else if (found) {
        break;
      }
    }
    if (found) {
      this->publish_presence_(1);
      this->publish_distance_(static_cast<uint16_t>(value));
    }
  }
}

void LD2410DComponent::handle_data_frame_(size_t payload_len) {
  // payload = [result(1)] [distance(2)] [16 move energies(4)] [16 still energies(4)]
  const uint8_t *p = &this->rx_buffer_[6];
  if (payload_len < 3)
    return;

  uint8_t result = p[0];
  uint16_t distance = p[1] | (p[2] << 8);
  this->publish_presence_(result);
  this->publish_distance_(distance);

  size_t offset = 3;
#ifdef USE_SENSOR
  for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++) {
    if (offset + 4 > payload_len)
      break;
    uint32_t raw = p[offset] | (p[offset + 1] << 8) | (p[offset + 2] << 16) | (p[offset + 3] << 24);
    offset += 4;
    if (this->move_energy_sensors_[g] != nullptr)
      this->move_energy_sensors_[g]->publish_state(raw_to_db(raw));
  }
  for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++) {
    if (offset + 4 > payload_len)
      break;
    uint32_t raw = p[offset] | (p[offset + 1] << 8) | (p[offset + 2] << 16) | (p[offset + 3] << 24);
    offset += 4;
    if (this->still_energy_sensors_[g] != nullptr)
      this->still_energy_sensors_[g]->publish_state(raw_to_db(raw));
  }
#endif
}

void LD2410DComponent::handle_ack_frame_(size_t payload_len) {
  const uint8_t *p = &this->rx_buffer_[6];
  if (payload_len < 2)
    return;
  uint16_t command = p[0] | (p[1] << 8);
  this->last_ack_command_ = command;

  switch (command) {
    case CMD_READ_VERSION: {
      // [cmd(2)][ack(2)][ver_len(2)][version bytes]
      if (payload_len < 6)
        break;
      uint16_t vlen = p[4] | (p[5] << 8);
      std::string version;
      for (uint16_t i = 0; i < vlen && (6 + i) < payload_len; i++)
        version.push_back(static_cast<char>(p[6 + i]));
      ESP_LOGI(TAG, "Firmware version: %s", version.c_str());
#ifdef USE_TEXT_SENSOR
      if (this->version_text_sensor_ != nullptr)
        this->version_text_sensor_->publish_state(version);
#endif
      break;
    }
    case CMD_READ_SN_STR: {
      // [cmd(2)][ack(2)][sn_len(2)][sn bytes]
      if (payload_len < 6)
        break;
      uint16_t slen = p[4] | (p[5] << 8);
      std::string sn;
      for (uint16_t i = 0; i < slen && (6 + i) < payload_len; i++)
        sn.push_back(static_cast<char>(p[6 + i]));
      ESP_LOGI(TAG, "Serial number: %s", sn.c_str());
#ifdef USE_TEXT_SENSOR
      if (this->sn_text_sensor_ != nullptr)
        this->sn_text_sensor_->publish_state(sn);
#endif
      break;
    }
    case CMD_READ_PARAMS: {
      // [cmd(2)][ack(2)][ (value(4)) * N ] in the fixed order we requested.
      size_t off = 4;
      auto read_u32 = [&](uint32_t &out) -> bool {
        if (off + 4 > payload_len)
          return false;
        out = p[off] | (p[off + 1] << 8) | (p[off + 2] << 16) | (p[off + 3] << 24);
        off += 4;
        return true;
      };
      uint32_t v;
      if (read_u32(v)) {
        float meters = v / 10.0f;  // stored in 0.1 m units
#ifdef USE_NUMBER
        if (this->max_distance_number_ != nullptr)
          this->max_distance_number_->publish_state(meters);
#endif
        ESP_LOGD(TAG, "Max distance: %.1f m", meters);
      }
      if (read_u32(v)) {
#ifdef USE_NUMBER
        if (this->timeout_number_ != nullptr)
          this->timeout_number_->publish_state(v);
#endif
        ESP_LOGD(TAG, "Timeout: %u s", v);
      }
      for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++) {
        if (!read_u32(v))
          break;
#ifdef USE_NUMBER
        if (this->move_threshold_numbers_[g] != nullptr)
          this->move_threshold_numbers_[g]->publish_state(raw_to_db(v));
#endif
      }
      for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++) {
        if (!read_u32(v))
          break;
#ifdef USE_NUMBER
        if (this->still_threshold_numbers_[g] != nullptr)
          this->still_threshold_numbers_[g]->publish_state(raw_to_db(v));
#endif
      }
      break;
    }
    default:
      break;
  }
}

void LD2410DComponent::publish_presence_(uint8_t detection_result) {
  // 0 = no target, 1 = moving target, 2 = stationary target
#ifdef USE_BINARY_SENSOR
  if (this->has_target_bs_ != nullptr)
    this->has_target_bs_->publish_state(detection_result != 0);
  if (this->moving_target_bs_ != nullptr)
    this->moving_target_bs_->publish_state(detection_result == 1);
  if (this->still_target_bs_ != nullptr)
    this->still_target_bs_->publish_state(detection_result == 2);
#endif
}

void LD2410DComponent::publish_distance_(uint16_t distance_cm) {
#ifdef USE_SENSOR
  if (this->detection_distance_sensor_ != nullptr)
    this->detection_distance_sensor_->publish_state(distance_cm);
#endif
}

// ---------------------------------------------------------------------------
// Command engine
// ---------------------------------------------------------------------------

void LD2410DComponent::write_command_(uint16_t command, const std::vector<uint8_t> &value) {
  std::vector<uint8_t> frame;
  frame.insert(frame.end(), CMD_FRAME_HEADER, CMD_FRAME_HEADER + 4);
  uint16_t len = 2 + value.size();
  frame.push_back(len & 0xFF);
  frame.push_back((len >> 8) & 0xFF);
  frame.push_back(command & 0xFF);
  frame.push_back((command >> 8) & 0xFF);
  frame.insert(frame.end(), value.begin(), value.end());
  frame.insert(frame.end(), CMD_FRAME_FOOTER, CMD_FRAME_FOOTER + 4);
  this->write_array(frame);
  this->flush();
}

bool LD2410DComponent::wait_ack_(uint16_t command, uint32_t timeout_ms) {
  this->last_ack_command_ = -1;
  uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    while (this->available()) {
      uint8_t byte;
      if (this->read_byte(&byte)) {
        this->rx_buffer_.push_back(byte);
        this->process_buffer_();
      }
    }
    if (this->last_ack_command_ == static_cast<int32_t>(command))
      return true;
    delay(2);
  }
  ESP_LOGW(TAG, "Timed out waiting for ACK of command 0x%04X", command);
  return false;
}

void LD2410DComponent::enter_config_mode_() {
  if (this->config_mode_)
    return;
  this->write_command_(CMD_ENABLE_CONFIG, {0x01, 0x00});
  this->config_mode_ = this->wait_ack_(CMD_ENABLE_CONFIG);
}

void LD2410DComponent::exit_config_mode_() {
  this->write_command_(CMD_END_CONFIG);
  this->wait_ack_(CMD_END_CONFIG);
  this->config_mode_ = false;
}

void LD2410DComponent::read_all_info() {
  this->enter_config_mode_();
  this->write_command_(CMD_READ_VERSION);
  this->wait_ack_(CMD_READ_VERSION);
  this->write_command_(CMD_READ_SN_STR);
  this->wait_ack_(CMD_READ_SN_STR);
  this->exit_config_mode_();
  this->query_parameters();
}

void LD2410DComponent::query_parameters() {
  // Request all parameters in a fixed order so the ACK can be decoded positionally.
  std::vector<uint8_t> ids;
  auto add_id = [&](uint16_t id) {
    ids.push_back(id & 0xFF);
    ids.push_back((id >> 8) & 0xFF);
  };
  add_id(PARAM_MAX_DISTANCE);
  add_id(PARAM_TIMEOUT);
  for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++)
    add_id(PARAM_MOVE_THRESHOLD_BASE + g);
  for (uint8_t g = 0; g < LD2410D_TOTAL_GATES; g++)
    add_id(PARAM_STILL_THRESHOLD_BASE + g);

  this->enter_config_mode_();
  this->write_command_(CMD_READ_PARAMS, ids);
  this->wait_ack_(CMD_READ_PARAMS);
  this->exit_config_mode_();
}

void LD2410DComponent::set_engineering_mode(bool enable) {
  // command value (2) + parameter value (4): 0x04 = engineering, 0x64 = normal.
  std::vector<uint8_t> value = {0x00, 0x00, static_cast<uint8_t>(enable ? 0x04 : 0x64), 0x00, 0x00, 0x00};
  this->enter_config_mode_();
  this->write_command_(CMD_SET_OUTPUT_MODE, value);
  this->wait_ack_(CMD_SET_OUTPUT_MODE);
  this->exit_config_mode_();
  ESP_LOGI(TAG, "Engineering mode %s", enable ? "enabled" : "disabled");
}

static std::vector<uint8_t> encode_param(uint16_t id, uint32_t value) {
  return {static_cast<uint8_t>(id & 0xFF),         static_cast<uint8_t>((id >> 8) & 0xFF),
          static_cast<uint8_t>(value & 0xFF),      static_cast<uint8_t>((value >> 8) & 0xFF),
          static_cast<uint8_t>((value >> 16) & 0xFF), static_cast<uint8_t>((value >> 24) & 0xFF)};
}

void LD2410DComponent::set_max_distance(float meters) {
  if (meters < 0.7f)
    meters = 0.7f;
  if (meters > 10.0f)
    meters = 10.0f;
  uint32_t value = static_cast<uint32_t>(meters * 10.0f + 0.5f);  // 0.1 m units
  this->enter_config_mode_();
  this->write_command_(CMD_SET_PARAMS, encode_param(PARAM_MAX_DISTANCE, value));
  this->wait_ack_(CMD_SET_PARAMS);
  this->write_command_(CMD_SAVE_PARAMS);
  this->wait_ack_(CMD_SAVE_PARAMS);
  this->exit_config_mode_();
#ifdef USE_NUMBER
  if (this->max_distance_number_ != nullptr)
    this->max_distance_number_->publish_state(value / 10.0f);
#endif
}

void LD2410DComponent::set_absence_timeout(uint16_t seconds) {
  this->enter_config_mode_();
  this->write_command_(CMD_SET_PARAMS, encode_param(PARAM_TIMEOUT, seconds));
  this->wait_ack_(CMD_SET_PARAMS);
  this->write_command_(CMD_SAVE_PARAMS);
  this->wait_ack_(CMD_SAVE_PARAMS);
  this->exit_config_mode_();
#ifdef USE_NUMBER
  if (this->timeout_number_ != nullptr)
    this->timeout_number_->publish_state(seconds);
#endif
}

void LD2410DComponent::set_move_threshold(uint8_t gate, float db) {
  if (gate >= LD2410D_TOTAL_GATES)
    return;
  uint32_t raw = db_to_raw(db);
  this->enter_config_mode_();
  this->write_command_(CMD_SET_PARAMS, encode_param(PARAM_MOVE_THRESHOLD_BASE + gate, raw));
  this->wait_ack_(CMD_SET_PARAMS);
  this->write_command_(CMD_SAVE_PARAMS);
  this->wait_ack_(CMD_SAVE_PARAMS);
  this->exit_config_mode_();
#ifdef USE_NUMBER
  if (this->move_threshold_numbers_[gate] != nullptr)
    this->move_threshold_numbers_[gate]->publish_state(db);
#endif
}

void LD2410DComponent::set_still_threshold(uint8_t gate, float db) {
  if (gate >= LD2410D_TOTAL_GATES)
    return;
  uint32_t raw = db_to_raw(db);
  this->enter_config_mode_();
  this->write_command_(CMD_SET_PARAMS, encode_param(PARAM_STILL_THRESHOLD_BASE + gate, raw));
  this->wait_ack_(CMD_SET_PARAMS);
  this->write_command_(CMD_SAVE_PARAMS);
  this->wait_ack_(CMD_SAVE_PARAMS);
  this->exit_config_mode_();
#ifdef USE_NUMBER
  if (this->still_threshold_numbers_[gate] != nullptr)
    this->still_threshold_numbers_[gate]->publish_state(db);
#endif
}

void LD2410DComponent::generate_threshold() {
  auto clamp_coeff = [](float v) -> uint16_t {
    if (v < 1.0f)
      v = 1.0f;
    if (v > 20.0f)
      v = 20.0f;
    return static_cast<uint16_t>(v * 10.0f + 0.5f);  // 10x scaling
  };
  uint16_t trig = clamp_coeff(this->trigger_coefficient_);
  uint16_t hold = clamp_coeff(this->hold_coefficient_);
  uint16_t micro = clamp_coeff(this->micro_coefficient_);
  std::vector<uint8_t> value = {static_cast<uint8_t>(trig & 0xFF),  static_cast<uint8_t>((trig >> 8) & 0xFF),
                                static_cast<uint8_t>(hold & 0xFF),  static_cast<uint8_t>((hold >> 8) & 0xFF),
                                static_cast<uint8_t>(micro & 0xFF), static_cast<uint8_t>((micro >> 8) & 0xFF)};
  this->enter_config_mode_();
  this->write_command_(CMD_GENERATE_THRESHOLD, value);
  this->wait_ack_(CMD_GENERATE_THRESHOLD);
  ESP_LOGI(TAG, "Auto threshold generation started (keep the area clear). Query params afterwards.");
  this->exit_config_mode_();
}

void LD2410DComponent::auto_gain() {
  this->enter_config_mode_();
  this->write_command_(CMD_AUTO_GAIN);
  this->wait_ack_(CMD_AUTO_GAIN);
  this->exit_config_mode_();
  ESP_LOGI(TAG, "Auto gain adjustment triggered");
}

void LD2410DComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD2410D:");
  this->check_uart_settings(115200);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Has Target", this->has_target_bs_);
  LOG_BINARY_SENSOR("  ", "Moving Target", this->moving_target_bs_);
  LOG_BINARY_SENSOR("  ", "Still Target", this->still_target_bs_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Detection Distance", this->detection_distance_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Serial Number", this->sn_text_sensor_);
#endif
}

}  // namespace ld2410d
}  // namespace esphome
