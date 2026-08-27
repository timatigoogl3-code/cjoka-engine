# cjoka Engine — Roadmap & Next-Gen Graphics Pipeline

## Этап 1: Оптимизация и Frustum Culling — **[ВЫПОЛНЕНО]**
- [x] **Frustum Culling**: Вычисление AABB для мешей и отсечение объектов за пределами пирамиды видимости камеры.
- [x] **Shadow Frustum Culling**: Отсечение объектов за пределами пирамид каскадов теней.
- [x] **Forward+ Compute Optimization**: Исправлен размер рабочих групп compute shader с устранением 55k лишних потоков.

## Этап 2: Конвейер Освещения (Forward+ Clustering) — **[ВЫПОЛНЕНО]**
- [x] **Compute Shader**: Разбиение видимого пространства на 16x9x24 3D-кластеров.
- [x] **Light SSBO**: Хранение до 1024 точечных источников в буферах SSBO.
- [x] **Light Culling**: Отсечение сфер света против AABB кластеров.
- [x] **PBR IBL Sky Reflections**: Зеркальные отражения окружения по вектору $R$ с учетом Roughness и Fresnel-Schlick.

## Этап 3: Тени и Сглаживание (Shadows & TAA) — **[ВЫПОЛНЕНО]**
- [x] **Cascaded Shadow Maps (CSM)**: 3 каскада в `GL_TEXTURE_2D_ARRAY`.
- [x] **PCSS / 16-tap Poisson Filtering**: Мягкие тени с адаптивным радиусом и Texel Snapping.
- [x] **Temporal Anti-Aliasing (TAA)**: YCoCg AABB-Clamping с velocity buffer reprojection.
- [x] **Full Scene Serialization**: Сохранение и загрузка полного графа компонентов.

## Этап 4: Отражения и Непрямой Свет — **[ВЫПОЛНЕНО]**
- [x] **Motion Vectors / Velocity Buffer (RG16F)**: MRT, prevMatrix, InstanceData, ClusteredMesh.
- [x] **Reverse-Z Depth Buffer**: far=0.0, near=1.0, precision near infinity.
- [x] **Normal Map + Specular Map**: TBN matrix, reading в kLitFS/kLitInstancedFS.
- [x] **TAA на velocity buffer**: Per-pixel motion vectors вместо depth reprojection.
- [x] **Z-Prepass**: depth-only reverse-Z pass в Systems::Render, инстансированный шейдер.
- [x] **GT Ambient Occlusion (GTAO)**: Half-res, edge-aware bilateral blur, height-reconstruct normals.
- [x] **Volumetric Fog**: Screen-space ray march, quarter-res, height falloff + FBM noise.
- [x] **Light Shafts (God Rays)**: Screen-space radial blur, additive blend.

## Этап 5: Продвинутые эффекты — **[В РАБОТЕ]**
1. **Subsurface Scattering (SSS)** — dual scattering для кожи.
2. **Anisotropic Hair** — Kajiya-Kay модель.
3. **Tessellation + Displacement Maps** — динамическая геометрия.
4. **Physical Water Model** — Gerstner waves + refraction + foam.

## Этап 6: VXGI — **[ЗАПЛАНИРОВАНО]**
1. **Voxel Cone Tracing (VXGI)** — Clipmap + aniso mipmaps + cone tracing для GI.
2. **Light Propagation Volumes (LPV)** — вторичные отскоки.
