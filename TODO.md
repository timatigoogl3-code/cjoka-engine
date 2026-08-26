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
- [x] **Temporal Anti-Aliasing (TAA)**: YCoCg AABB-Clamping с репроекцией истории по буферу глубины.
- [x] **Full Scene Serialization**: Сохранение и загрузка полного графа компонентов (PBR материалы, свет, атмосфера).

## Этап 4: Отражения и Непрямой Свет (SSR, Velocity, LPV / Lightmaps & VXGI) — **[В РАБОТЕ]**
1. **Motion Vectors / Velocity Buffer (RG16F)**:
   - [ ] Запись скоростей пикселей для движущихся объектов (`prevModel` vs `currModel`) для устранения гоустинга и смазывания в TAA.
2. **Screen-Space Reflections (SSR)**:
   - [ ] Реймарчинг по Depth/HDR буферу в экранном пространстве.
   - [ ] Зеркальные и глянцевые отражения соседних объектов, машин и света на полированном полу и мокрых поверхностях.
   - [ ] Плавный переход от SSR к Sky IBL на краях экрана и при промахах лучей.
3. **Light Propagation Volumes (LPV) / Static Lightmaps**:
   - [ ] Быстрый расчет вторичных отскоков света (Indirect Diffuse Bounce) для статической геометрии и помещений.
4. **Voxel Cone Tracing (VXGI)**:
   - [ ] Вокселизация сцены в 3D текстуру (Clipmap вокруг камеры).
   - [ ] Анизотропный мипмаппинг вокселей.
   - [ ] Трассировка конусов для динамического глобального освещения и затенения.
