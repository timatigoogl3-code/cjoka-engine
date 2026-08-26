#pragma once
// Registry.h — высокопроизводительный ECS на базе EnTT (C++20)
// Обеспечивает кэш-дружественное хранение компонентов (Sparse Sets) и нулевой оверхед на итерацию.
#include <entt/entt.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <type_traits>

using Entity = entt::entity;
constexpr Entity NullEntity = entt::null;

class Registry : public entt::registry {
public:
    Registry() = default;

    // Проверка наличия одного или нескольких компонентов у сущности
    template<typename T>
    bool has(Entity e) const {
        return all_of<T>(e);
    }

    template<typename T1, typename T2, typename... Rest>
    bool has(Entity e) const {
        return all_of<T1, T2, Rest...>(e);
    }

    // Удаление компонента с проверкой существования
    template<typename T>
    void remove(Entity e) {
        if (all_of<T>(e)) {
            erase<T>(e);
        }
    }

    // Получить или создать компонент
    template<typename T, typename... Args>
    T& get_or_emplace(Entity e, Args&&... args) {
        return get_or_emplace<T>(e, std::forward<Args>(args)...);
    }

    // Количество сущностей с заданным компонентом
    template<typename T>
    size_t count() const {
        const auto* s = storage<T>();
        return s ? s->size() : 0;
    }

    // Общее количество живых сущностей
    size_t aliveCount() const {
        const auto* s = storage<Entity>();
        return s ? s->size() : 0;
    }

    // Удобный хелпер: получение первой сущности с компонентом (или NullEntity)
    template<typename T>
    Entity first() const {
        auto v = view<T>();
        return (v.begin() == v.end()) ? NullEntity : *v.begin();
    }

    template<typename T1, typename T2, typename... Rest>
    Entity first() const {
        auto v = view<T1, T2, Rest...>();
        return (v.begin() == v.end()) ? NullEntity : *v.begin();
    }
};
