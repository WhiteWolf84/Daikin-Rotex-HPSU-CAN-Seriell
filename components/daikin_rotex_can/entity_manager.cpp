#include "esphome/components/daikin_rotex_can/entity_manager.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace daikin_rotex_can {

static const char* TAG = "daikin_rotex_can";

namespace {
// Casts a (possibly const) entity to the requested type, logging a uniform
// message when the entity is missing or of the wrong type.
template<typename T, typename E>
T* cast_entity(E* pEntity, std::string const& id, char const* type_name, bool log_missing) {
    if (T* pTyped = entity_cast<T>(pEntity)) {
        return pTyped;
    }
    if (log_missing) {
        if (pEntity != nullptr) {
            ESP_LOGE(TAG, "Entity is not a %s: %s", type_name, pEntity->getName().c_str());
        } else {
            ESP_LOGE(TAG, "Entity not found (%s): %s", type_name, id.c_str());
        }
    }
    return nullptr;
}
} // namespace

TEntityManager::TEntityManager()
: m_entities()
, m_pCanbus(nullptr)
, m_last_handle(0u)
, m_delay_between_requests(250)
, m_set_request_queue()
{
}

void TEntityManager::add(TEntity* pEntity) {
    m_entities.push_back(pEntity);
    m_entity_by_id[pEntity->get_id()] = pEntity;
}

void TEntityManager::removeInvalidRequests() {
    m_entities.erase(
        std::remove_if(
            m_entities.begin(),
            m_entities.end(),
            [](TEntity* pEntity) { return !pEntity->isGetSupported(); }
        ),
        m_entities.end()
    );

    // Rebuild the lookup index so it stays consistent with m_entities.
    m_entity_by_id.clear();
    for (TEntity* pEntity : m_entities) {
        m_entity_by_id[pEntity->get_id()] = pEntity;
    }
}

CanSensor* TEntityManager::get_sensor(std::string const& id) {
    return cast_entity<CanSensor>(get(id), id, "sensor", true);
}

CanSensor const* TEntityManager::get_sensor(std::string const& id) const {
    return cast_entity<const CanSensor>(get(id), id, "sensor", true);
}

CanTextSensor* TEntityManager::get_text_sensor(std::string const& id) {
    return cast_entity<CanTextSensor>(get(id), id, "text sensor", true);
}

CanTextSensor const* TEntityManager::get_text_sensor(std::string const& id) const {
    return cast_entity<const CanTextSensor>(get(id), id, "text sensor", true);
}

CanBinarySensor const* TEntityManager::get_binary_sensor(std::string const& id) const {
    return cast_entity<const CanBinarySensor>(get(id), id, "binary sensor", true);
}

CanNumber const* TEntityManager::get_number(std::string const& id, bool log_missing) const {
    return cast_entity<const CanNumber>(get(id), id, "number", log_missing);
}

CanSelect* TEntityManager::get_select(std::string const& id) {
    return cast_entity<CanSelect>(get(id), id, "select", true);
}

CanSelect const* TEntityManager::get_select(std::string const& id) const {
    return cast_entity<const CanSelect>(get(id), id, "select", true);
}

bool TEntityManager::sendNextPendingRequest() {
    if (m_set_request_queue.empty() == false && is_allowed_by_delay_between_requests()) {
        auto request = m_set_request_queue.front();
        m_set_request_queue.pop_front();
        ESP_LOGI(TAG, "sendNextPendingRequest: Process queued set request => name: %s, value: %f", request.first.c_str(), request.second);
        return sendSet(request.first, request.second);
    }

    TEntity* pEntity = getNextGetRequestToSend();
    if (pEntity != nullptr) {
        return pEntity->sendGet(m_pCanbus);
    }
    return false;
}

bool TEntityManager::sendSet(std::string const& request_name, float value) {
    const uint32_t now = esphome::millis();

    if (!is_allowed_by_delay_between_requests()) {
        ESP_LOGE(TAG, "sendSet: Add to queue => name: %s, value: %f", request_name.c_str(), value);
        m_set_request_queue.push_back(std::make_pair(request_name, value));
        return false;
    }

    const auto it = std::find_if(m_entities.begin(), m_entities.end(),
        [&request_name](auto pEntity) { return pEntity->getName() == request_name; }
    );
    bool result = false;
    if (it != m_entities.end()) {
        result = (*it)->sendSet(m_pCanbus, value * (*it)->get_config().divider);
        m_last_handle = esphome::millis();
    } else {
        ESP_LOGE(TAG, "sendSet: Unknown request: %s", request_name.c_str());
    }
    return result;
}

void TEntityManager::handle(uint32_t can_id, TMessage const& responseData) {
    bool bHandled = false;
    for (auto pEntity : m_entities) {
        if (pEntity->handle(can_id, responseData)) {
            bHandled = true;
            break;
        }
    }
    m_last_handle = esphome::millis();
    if (!bHandled) {
        Utils::log("unhandled", "can_id<%s> data<%s>", Utils::to_hex(can_id).c_str(), Utils::to_hex(responseData).c_str());
    }
}

TEntity* TEntityManager::get(std::string const& id) {
    const auto it = m_entity_by_id.find(id);
    return it != m_entity_by_id.end() ? it->second : nullptr;
}

TEntity const* TEntityManager::get(std::string const& id) const {
    const auto it = m_entity_by_id.find(id);
    return it != m_entity_by_id.end() ? it->second : nullptr;
}

TEntity* TEntityManager::getNextGetRequestToSend() {
    const uint32_t now = esphome::millis();

    if (!is_allowed_by_delay_between_requests()) {
        return nullptr;
    }

    for (auto pEntity : m_entities) {
        if (pEntity->isGetInProgress()) {
            return nullptr;
        }
    }

    TEntity* pNext = nullptr;
    for (auto pEntity : m_entities) {
        if (pEntity->isGetNeeded()) {
            if (pNext == nullptr || pEntity->getOverdueTime() > pNext->getOverdueTime()) {
                pNext = pEntity;
            }
        }
    }
    return pNext;
}


}
}