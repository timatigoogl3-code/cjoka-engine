#pragma once
// EventBus — Универсальная шина событий геймплея движка
#include "engine/ECS/Registry.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <any>

struct EventData {
    std::string name;
    Entity sender = NullEntity;
    Entity target = NullEntity;
    std::any payload;
    float timestamp = 0.0f;
};

using EventCallback = std::function<void(const EventData&)>;

class EventBus {
public:
    static EventBus& Get() {
        static EventBus instance;
        return instance;
    }

    uint64_t subscribe(const std::string& eventName, EventCallback callback) {
        uint64_t id = m_nextId++;
        m_listeners[eventName].push_back({id, callback});
        return id;
    }

    void unsubscribe(uint64_t subscriptionId) {
        for (auto& [_, list] : m_listeners) {
            for (auto it = list.begin(); it != list.end(); ) {
                if (it->id == subscriptionId) {
                    it = list.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void emit(const std::string& eventName, Entity sender = NullEntity, Entity target = NullEntity, std::any payload = {}) {
        EventData data;
        data.name = eventName;
        data.sender = sender;
        data.target = target;
        data.payload = payload;

        auto it = m_listeners.find(eventName);
        if (it != m_listeners.end()) {
            for (const auto& listener : it->second) {
                if (listener.callback) {
                    listener.callback(data);
                }
            }
        }

        // Global wildcard listeners
        auto wildIt = m_listeners.find("*");
        if (wildIt != m_listeners.end()) {
            for (const auto& listener : wildIt->second) {
                if (listener.callback) {
                    listener.callback(data);
                }
            }
        }
    }

    void clear() {
        m_listeners.clear();
    }

private:
    EventBus() = default;

    struct Listener {
        uint64_t id;
        EventCallback callback;
    };

    uint64_t m_nextId = 1;
    std::unordered_map<std::string, std::vector<Listener>> m_listeners;
};
