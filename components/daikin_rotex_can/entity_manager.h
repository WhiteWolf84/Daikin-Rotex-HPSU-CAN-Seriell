#pragma once

#include "esphome/components/daikin_rotex_can/sensors.h"
#include "esphome/components/daikin_rotex_can/entity.h"
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace esphome {
namespace daikin_rotex_can {

class TEntityManager {
public:
    TEntityManager();
    void add(TEntity* pRequest);

    void removeInvalidRequests();

    void setCanbus(esphome::esp32_can::ESP32Can* pCanbus);
    esphome::esp32_can::ESP32Can* getCanbus() const;

    uint32_t size() const;
    TEntity const* get(uint32_t index) const;
    TEntity* get(std::string const& id);
    TEntity const* get(std::string const& id) const;

    const std::vector<TEntity*>& get_entities() const { return m_entities; }
    void set_delay_between_requests(uint16_t milliseconds);

    CanSensor* get_sensor(std::string const& id);
    CanSensor const* get_sensor(std::string const& id) const;

    CanTextSensor* get_text_sensor(std::string const& id);
    CanTextSensor const* get_text_sensor(std::string const& id) const;

    CanBinarySensor const* get_binary_sensor(std::string const& id) const;

    CanNumber const* get_number(std::string const& id, bool log_missing = true) const;

    CanSelect* get_select(std::string const& id);
    CanSelect const* get_select(std::string const& id) const;

    bool is_allowed_by_delay_between_requests() const;

    bool sendNextPendingRequest();
    bool sendSet(std::string const& request_name, float value);
    void handle(uint32_t can_id, TMessage const& responseData);

private:
    TEntity* getNextGetRequestToSend();

    std::vector<TEntity*> m_entities;
    std::unordered_map<std::string, TEntity*> m_entity_by_id; // id -> entity, for O(1) lookup
    // can_id -> entities listening on it. handle() only scans the bucket of the
    // incoming frame, so ids with no entities (0x600/0x680/0x780 bursts from the
    // RoCon panel) cost zero entity iterations. 0x10A frames can match ANY
    // entity (panel responses), so they still scan m_entities; see handle().
    std::unordered_map<uint32_t, std::vector<TEntity*>> m_entities_by_can_id;
    esphome::esp32_can::ESP32Can* m_pCanbus;
    uint32_t m_last_handle;
    uint16_t m_delay_between_requests;
    std::deque<std::pair<std::string, float>> m_set_request_queue;
};

inline void TEntityManager::setCanbus(esphome::esp32_can::ESP32Can* pCanbus) {
    m_pCanbus = pCanbus;
}

inline esphome::esp32_can::ESP32Can* TEntityManager::getCanbus() const {
    return m_pCanbus;
}

inline uint32_t TEntityManager::size() const {
    return m_entities.size();
}

inline TEntity const* TEntityManager::get(uint32_t index) const {
    return (index < m_entities.size()) ? m_entities[index] : nullptr;
}

inline void TEntityManager::set_delay_between_requests(uint16_t milliseconds) {
    m_delay_between_requests = milliseconds;
}

inline bool TEntityManager::is_allowed_by_delay_between_requests() const {
    const uint32_t now = esphome::millis();
    return now >= (m_last_handle + m_delay_between_requests);
}

}
}