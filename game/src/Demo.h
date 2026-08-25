#pragma once
#include "engine/Engine.h"
#include <memory>

// Демка возможностей движка cjoka.
// Всё в одном файле — смотри Game.cpp: OBJ, текстуры, свет, HDR, bloom, FXAA, batching, kGUI.
class Demo : public Application {
public:
    Demo();
    ~Demo() override;
protected:
    void onInit() override;
    void onUpdate(float dt) override;
    void onShutdown() override;
private:
    void setupAtmosphere();
    void setupLights();
    void setupShowcase();
    void setupGUI();
    void updateHUD(float dt, int w, int h);

    std::unique_ptr<Shader> m_litShader;
    Entity m_camera = NullEntity;

    // витрина
    Entity m_plant = NullEntity;          // тяжёлая модель 137k verts
    Entity m_metalSphere = NullEntity;    // блестящий материал
    Entity m_emissiveOrb = NullEntity;    // светящийся шар (bloom)
    std::vector<Entity> m_batchRow;       // инстансинг-ряд
    float m_time = 0.0f;
};
