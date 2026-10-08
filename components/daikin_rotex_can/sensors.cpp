#include "esphome/components/daikin_rotex_can/sensors.h"
#include "esphome/components/daikin_rotex_can/entity.h"
#include <cmath>

namespace esphome {
namespace daikin_rotex_can {

static const char* CAN_SENSOR_TAG = "CanSensor";
static const char* CAN_NUMBER_TAG = "CanNumber";
static const char* CAN_SELECT_TAG = "CanSelect";
static const char* CAN_SWITCH_TAG = "CanSwitch";

/////////////////////// CanSensor ///////////////////////

CanSensor::CanSensor()
: m_state(std::numeric_limits<float>::quiet_NaN())
, m_range()
, m_pid(0.2, 0.05f, 0.05f, 0.2, 0.2, 0.1f)
, m_smooth(false)
, m_logging(false)
, m_smooth_state(std::numeric_limits<float>::quiet_NaN())
{
}

CanSensor::CanSensor(std::string const& id)
: CanSensor()
{
    m_config.id = id;
}

bool CanSensor::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = state;
    if (m_config.isSigned) {
        current = static_cast<int16_t>(value) / m_config.divider;
    } else {
        current = value / m_config.divider;
    }

    const float float_value = std::get<float>(current);

    if (m_range.contains(float_value)) {
        publish_state(float_value);
        return true;
    }

    // Out of range means "no value", not "keep the last one": the machine uses
    // such placeholders on purpose (target_supply_temperature reads 101 with
    // no cooling demand, 0 in standby). Report unknown, and treat only the
    // known -> unknown edge as a change so dependants (e.g. the setpoint-TV
    // delta) recompute once and repeated placeholders stay silent.
    const bool was_known = !std::isnan(state);
    if (was_known) {
        ESP_LOGW(CAN_SENSOR_TAG, "Sensor<%s> raw<%s> value<%f> outside [%f, %f], reporting unknown",
            get_id().c_str(), Utils::to_hex(value).c_str(), float_value, m_range.min, m_range.max);
        publish_state(NAN);
    }
    current = NAN;
    return was_known;
}

void CanSensor::update(uint32_t millis) {
    TEntity::update(millis);

    if (m_smooth) {
        const uint32_t now = esphome::millis();
        // Unsigned subtraction stays correct across the 49.7-day millis() wrap;
        // the previous float subtraction went negative there and froze the filter.
        const float dt = static_cast<float>(now - m_pid.get_last_update()) / 1000.0f; // seconds
        if (dt > 10.0f) {
            if (!std::isfinite(m_state)) {
                // An input is unknown (at boot, or tv/tr/flow_rate out of range).
                // PID::compute() would return 0 without advancing its clock, so dt
                // would stay above 10 and the last value -- stale, or NaN at boot --
                // would go out on every loop. Report unknown once, on the
                // known -> unknown edge, drop the filter's history so it restarts
                // from the real value, and rearm the 10 s cadence.
                if (!std::isnan(state)) {
                    ESP_LOGW(CAN_SENSOR_TAG, "Sensor<%s> input unknown, reporting unknown and resetting the filter",
                        get_id().c_str());
                    publish_state(NAN);
                }
                m_pid.reset(now);
                m_smooth_state = NAN;
                return;
            }

            if (std::isnan(m_smooth_state)) {
                m_smooth_state = m_state;
            }

            std::string logstr;
            m_smooth_state += m_pid.compute(m_state, m_smooth_state, dt, logstr);
            Utils::log("PID", "%s: %s, val: %f", get_id().c_str(), logstr.c_str(), m_smooth_state);

            // Round only what is published, to the nearest hundredth. Rounding the
            // filter state itself (formerly with ceil) fed the quantisation back
            // into the PID: a persistent +0.01/-0.0/-0.01 limit cycle at zero flow,
            // and an upward bias that grows with input noise. Adding 0.0f turns a
            // rounded -0.0 into +0.0, so a stopped pump never reads "-0.0".
            publish_state(std::round(m_smooth_state * 100.0f) / 100.0f + 0.0f);
        }
    }
}

void CanSensor::publish(float state) {
    m_state = state;
    if (!m_smooth) {
        publish_state(state);
    }
}

/////////////////////// CanTextSensor ///////////////////////

bool CanTextSensor::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = state;
    auto it = m_map.findByKey(value);
    current = m_recalculate_state(m_config.pEntity, it != m_map.end() ? it->second : Utils::format("INVALID<%u>", value));
    publish_state(std::get<std::string>(current));
    return true;
}

/////////////////////// CanBinarySensor ///////////////////////

bool CanBinarySensor::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = state;
    current = value > 0;
    publish_state(std::get<bool>(current));
    return true;
}

/////////////////////// CanNumber ///////////////////////

void CanNumber::control(float value) {
    Utils::log(CAN_NUMBER_TAG, "control(%f), state: %f", value, this->state);

    if (std::fabs(value - this->state) >= 0.001f) {
        this->publish_state(value);
        sendSet(m_pCanbus, value * get_config().divider);
    }
}

bool CanNumber::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = state;
    if (m_config.isSigned) {
        current = static_cast<int16_t>(value) / m_config.divider;
    } else {
        current = value / m_config.divider;
    }

    publish_state(std::get<float>(current));
    return true;
}

/////////////////////// CanSelect ///////////////////////

void CanSelect::control(const std::string &value) {
    std::string prevValue = this->current_option().str();
    this->publish_state(value);
    const uint16_t key = getKey(current_option());
    const bool handled = m_custom_select_lambda(get_id(), key);

    Utils::log(CAN_SELECT_TAG, "control(%s), current_option: %s, prevValue: %s, handled: %d",
        value.c_str(), this->current_option().str().c_str(), prevValue.c_str(), handled);

    if (!handled && value != prevValue) {
        sendSet(m_pCanbus, key);
    }
}

uint16_t CanSelect::getKey(std::string const& value) const {
    return m_map.getKey(value);
}

void CanSelect::publish_select_key(uint16_t key) {
    auto it = m_map.findByKey(key);
    if (it != m_map.end()) {
        publish_state(it->second);
    } else {
        ESP_LOGE(CAN_SELECT_TAG, "publish_select_key(%u) => Key not found!", key);
    }
}

bool CanSelect::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = current_option();
    current = findNextByKey(value, Utils::format("INVALID<%u>", value));
    publish_state(std::get<std::string>(current));
    return true;
}

/////////////////////// CanSwitch ///////////////////////

void CanSwitch::write_state(bool state) {
    Utils::log(CAN_SWITCH_TAG, "write_state(%s), state: %s", state ? "ON" : "OFF", this->state ? "ON" : "OFF");

    if (state != this->state) {
        this->publish_state(state);
        sendSet(m_pCanbus, state);
    }
}

bool CanSwitch::handleValue(uint16_t value, TEntity::TVariant& current, TVariant& previous) {
    previous = state;
    current = static_cast<bool>(value);
    publish_state(std::get<bool>(current));
    return true;
}

}
}