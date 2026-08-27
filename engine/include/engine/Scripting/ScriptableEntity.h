#pragma once
#include "engine/ECS/Registry.h"
#include "engine/ECS/Components.h"
#include "engine/Core/Input.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <typeinfo>

class ScriptableEntity {
public:
    virtual ~ScriptableEntity() = default;

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

    virtual void onCreate() {}
    virtual void onStart() {}
    virtual void onUpdate(float dt) { (void)dt; }
    virtual void onDestroy() {}
    virtual void onInspectorGUI() {}

    void _init(Entity e, Registry* reg) {
        m_entity = e;
        m_registry = reg;
    }

private:
    Entity m_entity = NullEntity;
    Registry* m_registry = nullptr;
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
