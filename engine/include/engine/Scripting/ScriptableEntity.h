#pragma once
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Core/Input.h"
#include "engine/Core/DebugLog.h"
#include "engine/Gameplay/EventBus.h"
#include "engine/Gameplay/Blackboard.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeinfo>

class ScriptableEntity {
public:
    virtual ~ScriptableEntity() {
        for (uint64_t subId : m_eventSubscriptions) {
            EventBus::Get().unsubscribe(subId);
        }
    }

    template<typename T>
    T& get() { return m_registry->get<T>(m_entity); }

    template<typename T>
    const T& get() const { return m_registry->get<T>(m_entity); }

    template<typename T>
    bool has() const { return m_registry->has<T>(m_entity); }

    template<typename T, typename... Args>
    T& add(Args&&... args) { return m_registry->emplace<T>(m_entity, std::forward<Args>(args)...); }

    Entity entity() const { return m_entity; }
    Transform& transform() { return get<Transform>(); }
    Registry& registry() { return *m_registry; }
    const Registry& registry() const { return *m_registry; }
    Registry& sceneRegistry() { return *m_registry; }
    const Registry& sceneRegistry() const { return *m_registry; }

    // Editor-only debug logging
    void log(const std::string& msg) {
        DebugLog::LogScript(msg, "Script");
    }
    void logWarning(const std::string& msg) {
        DebugLog::LogWarning(msg, "Script");
    }
    void logError(const std::string& msg) {
        DebugLog::LogError(msg, "Script");
    }

    // Global & Gameplay Events
    void emitEvent(const std::string& name, Entity target = NullEntity, std::any payload = {}) {
        EventBus::Get().emit(name, m_entity, target, payload);
    }

    void subscribeEvent(const std::string& name, EventCallback callback) {
        uint64_t id = EventBus::Get().subscribe(name, callback);
        m_eventSubscriptions.push_back(id);
    }

    // World Blackboard & Quest State
    void setFlag(const std::string& key, bool val) { Blackboard::Get().setBool(key, val); }
    bool getFlag(const std::string& key, bool defaultVal = false) const { return Blackboard::Get().getBool(key, defaultVal); }

    void setInt(const std::string& key, int val) { Blackboard::Get().setInt(key, val); }
    int getInt(const std::string& key, int defaultVal = 0) const { return Blackboard::Get().getInt(key, defaultVal); }

    void setFloat(const std::string& key, float val) { Blackboard::Get().setFloat(key, val); }
    float getFloat(const std::string& key, float defaultVal = 0.0f) const { return Blackboard::Get().getFloat(key, defaultVal); }

    void setString(const std::string& key, const std::string& val) { Blackboard::Get().setString(key, val); }
    std::string getString(const std::string& key, const std::string& defaultVal = "") const { return Blackboard::Get().getString(key, defaultVal); }

    virtual void onCreate() {}
    virtual void onStart() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onDestroy() {}
    virtual void onInspectorGUI() {}

    virtual void onTriggerEnter(Entity visitor) { (void)visitor; }
    virtual void onTriggerExit(Entity visitor) { (void)visitor; }
    virtual void onEvent(const EventData& event) { (void)event; }

    void _init(Entity e, Registry* reg) {
        m_entity = e;
        m_registry = reg;
    }

private:
    Entity m_entity = NullEntity;
    Registry* m_registry = nullptr;
    std::vector<uint64_t> m_eventSubscriptions;
    friend class ScriptSystem;
    friend class Scene;
};

// Native Script Component
struct NativeScript {
    std::string scriptName;
    std::shared_ptr<ScriptableEntity> instance = nullptr;
    std::function<std::shared_ptr<ScriptableEntity>()> instantiate = nullptr;

    NativeScript() = default;

    template<typename T>
    void bind(const std::string& name = "") {
        scriptName = name.empty() ? typeid(T).name() : name;
        instantiate = []() -> std::shared_ptr<ScriptableEntity> {
            return std::make_shared<T>();
        };
    }
};

class ScriptRegistry {
public:
    using FactoryFunc = std::function<std::shared_ptr<ScriptableEntity>()>;

    static ScriptRegistry& Get() {
        static ScriptRegistry instance;
        return instance;
    }

    void registerScript(const std::string& name, FactoryFunc factory) {
        m_factories[name] = factory;
    }

    std::shared_ptr<ScriptableEntity> create(const std::string& name) {
        auto it = m_factories.find(name);
        if (it != m_factories.end()) return it->second();
        return nullptr;
    }

    const std::unordered_map<std::string, FactoryFunc>& allScripts() const {
        return m_factories;
    }

private:
    ScriptRegistry() = default;
    std::unordered_map<std::string, FactoryFunc> m_factories;
};

template<typename T>
struct RegisterScriptHelper {
    RegisterScriptHelper(const std::string& name) {
        ScriptRegistry::Get().registerScript(name, []() { return std::make_shared<T>(); });
    }
};

#define REGISTER_SCRIPT(ScriptClass, DisplayName) \
    static ::RegisterScriptHelper<ScriptClass> s_reg_##ScriptClass(DisplayName);
