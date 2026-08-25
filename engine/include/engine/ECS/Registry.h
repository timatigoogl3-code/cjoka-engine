#pragma once
// Registry.h — лёгкий ECS (header-only, C++20)
// Entity = uint32_t, компоненты хранятся как unordered_map<Entity,T> per тип.
// Пример: Entity e = registry.create(); registry.emplace<Transform>(e, {{0,1,0}});
//         for (auto e : registry.view<Transform, MeshRenderer>()) { auto& t = registry.get<Transform>(e); }
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <typeindex>
#include <any>
#include <functional>
#include <cassert>

using Entity = uint32_t;
constexpr Entity NullEntity = 0;
class Registry {
public:
    Entity create() {
        Entity e = next_++;
        alive_.insert(e);
        return e;
    }

    void destroy(Entity e) {
        if (!alive_.contains(e)) return;
        alive_.erase(e);
        for (auto& fn : erasers_) fn(e);
    }

    bool valid(Entity e) const { return alive_.contains(e); }
    size_t aliveCount() const { return alive_.size(); }

    template<typename T, typename... Args>
    T& emplace(Entity e, Args&&... args) {
        assert(valid(e) && "emplace on dead entity");
        auto& map = storage<T>();
        auto [it, inserted] = map.try_emplace(e, std::forward<Args>(args)...);
        if (!inserted) it->second = T{std::forward<Args>(args)...};
        return it->second;
    }

    template<typename T>
    bool has(Entity e) const {
        const auto* m = findStorage<T>();
        return m && m->contains(e);
    }

    template<typename T>
    T& get(Entity e) {
        auto& m = storage<T>();
        auto it = m.find(e);
        assert(it != m.end() && "get: no component");
        return it->second;
    }
    template<typename T>
    const T& get(Entity e) const {
        const auto& m = storage<T>();
        auto it = m.find(e);
        assert(it != m.end() && "get: no component");
        return it->second;
    }

    template<typename T>
    void remove(Entity e) {
        if (auto* m = findStorage<T>()) m->erase(e);
    }

    template<typename T>
    T* try_get(Entity e) {
        auto* m = findStorage<T>();
        if (!m) return nullptr;
        auto it = m->find(e);
        return it == m->end() ? nullptr : &it->second;
    }
    template<typename T>
    const T* try_get(Entity e) const {
        const auto* m = findStorage<T>();
        if (!m) return nullptr;
        auto it = m->find(e);
        return it == m->end() ? nullptr : &it->second;
    }
    template<typename T, typename... Args>
    T& get_or_emplace(Entity e, Args&&... args) {
        if (auto* p = try_get<T>(e)) return *p;
        return emplace<T>(e, std::forward<Args>(args)...);
    }
    template<typename T>
    size_t count() const {
        const auto* m = findStorage<T>();
        return m ? m->size() : 0;
    }

    // High-level each — энтовский стиль: registry.each<Transform>([](Entity e, Transform& t){...})
    template<typename T, typename Func>
    void each(Func&& fn) {
        for (Entity e : view<T>()) fn(e, get<T>(e));
    }
    template<typename A, typename B, typename Func>
    void each(Func&& fn) {
        for (Entity e : view<A,B>()) fn(e, get<A>(e), get<B>(e));
    }

    template<typename T>
    std::vector<Entity> view() const {
        const auto* m = findStorage<T>();
        if (!m) return {};
        std::vector<Entity> out; out.reserve(m->size());
        for (auto& [e, _] : *m) if (alive_.contains(e)) out.push_back(e);
        return out;
    }

    template<typename A, typename B>
    std::vector<Entity> view() const {
        const auto* ma = findStorage<A>();
        const auto* mb = findStorage<B>();
        if (!ma || !mb) return {};
        std::vector<Entity> out;
        if (ma->size() < mb->size()) {
            out.reserve(ma->size());
            for (auto& [e, _] : *ma) if (mb->contains(e) && alive_.contains(e)) out.push_back(e);
        } else {
            out.reserve(mb->size());
            for (auto& [e, _] : *mb) if (ma->contains(e) && alive_.contains(e)) out.push_back(e);
        }
        return out;
    }

    template<typename First, typename Second, typename... Rest>
    requires (sizeof...(Rest) > 0)
    std::vector<Entity> view() const {
        auto base = view<First, Second>();
        std::vector<Entity> out; out.reserve(base.size());
        for (Entity e : base) if ((has<Rest>(e) && ...)) out.push_back(e);
        return out;
    }

    void clear() {
        alive_.clear();
        storages_.clear();
        erasers_.clear();
        next_ = 1;
    }

private:
    template<typename T>
    std::unordered_map<Entity, T>& storage() {
        std::type_index ti(typeid(T));
        auto it = storages_.find(ti);
        if (it == storages_.end()) {
            auto [newIt, _] = storages_.emplace(ti, std::unordered_map<Entity, T>{});
            erasers_.push_back([this, ti](Entity e) {
                auto f = storages_.find(ti);
                if (f != storages_.end())
                    std::any_cast<std::unordered_map<Entity, T>&>(f->second).erase(e);
            });
            return std::any_cast<std::unordered_map<Entity, T>&>(newIt->second);
        }
        return std::any_cast<std::unordered_map<Entity, T>&>(it->second);
    }

    template<typename T>
    const std::unordered_map<Entity, T>& storage() const {
        return const_cast<Registry*>(this)->storage<T>();
    }

    template<typename T>
    const std::unordered_map<Entity, T>* findStorage() const {
        auto it = storages_.find(std::type_index(typeid(T)));
        if (it == storages_.end()) return nullptr;
        return &std::any_cast<const std::unordered_map<Entity, T>&>(it->second);
    }
    template<typename T>
    std::unordered_map<Entity, T>* findStorage() {
        auto it = storages_.find(std::type_index(typeid(T)));
        if (it == storages_.end()) return nullptr;
        return &std::any_cast<std::unordered_map<Entity, T>&>(it->second);
    }

    std::unordered_map<std::type_index, std::any> storages_;
    std::unordered_set<Entity> alive_;
    std::vector<std::function<void(Entity)>> erasers_;
    Entity next_ = 1;
};
