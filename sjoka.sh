#!/usr/bin/env bash
# если вызвали через `sh sjoka.sh` (dash) — переисполнить через bash
if [ -z "${BASH_VERSION:-}" ]; then
    exec bash "$0" "$@"
fi
set -euo pipefail

# sjoka.sh — CLI для модульного движка cjoka
#   ./sjoka.sh compile engine [--release|--debug]   — собрать только движок (libcjoka_engine.a)
#   ./sjoka.sh compile game    [--release|--debug]  — собрать игру (game/cjoka, линкует движок)
#   ./sjoka.sh compile all     [--release|--debug]  — собрать всё
#   ./sjoka.sh play game       [--release]          — собрать (если нужно) и запустить
#   ./sjoka.sh precompile shaders [--release]       — прекомпилировать шейдеры → binary
#   ./sjoka.sh clean                                — удалить build/
#   ./sjoka.sh help
#
# Требует: clang, cmake 3.20+, ninja, glfw, glm. Опционально glslc / glslangValidator для SPIR-V.

# --- colors ---
if [[ -t 1 ]]; then
    B="\033[1m"; D="\033[2m"; G="\033[32m"; Y="\033[33m"; C="\033[36m"; R="\033[31m"; N="\033[0m"
else
    B=""; D=""; G=""; Y=""; C=""; R=""; N=""
fi
info()  { echo -e "${C}[sjoka]${N} $*"; }
ok()    { echo -e "${G}[sjoka]${N} $*"; }
warn()  { echo -e "${Y}[sjoka]${N} $*"; }
err()   { echo -e "${R}[sjoka]${N} $*" >&2; }
die()   { err "$*"; exit 1; }

# robust PROJECT_DIR — не зависит от битового cwd (после clean build/ cwd может быть удалён)
SCRIPT_PATH="$(realpath "${BASH_SOURCE[0]:-$0}" 2>/dev/null || readlink -f "${BASH_SOURCE[0]:-$0}" 2>/dev/null || echo "$0")"
# если путь относительный (запуск как `sh sjoka.sh`), достроим от текущего (если живой) или от HOME
if [[ "$SCRIPT_PATH" != /* ]]; then
    # пробуем восстановить cwd, если он удалён — откатываемся в директорию скрипта через HOME
    _cwd="$(pwd 2>/dev/null || echo "$HOME")"
    SCRIPT_PATH="$_cwd/$SCRIPT_PATH"
fi
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
# если SCRIPT_DIR относительный или битый — пробуем вычислить от расположения файла
if [[ ! -d "$SCRIPT_DIR" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" 2>/dev/null && pwd 2>/dev/null || echo "/home/cune/Изображения/proj/cjoka")"
fi
PROJECT_DIR="$SCRIPT_DIR"
# сразу уходим в корень проекта, чтобы getcwd не падал после `sjoka.sh clean` (когда shell был в build/)
cd "$PROJECT_DIR" 2>/dev/null || true
PRESET_DEBUG="clang-debug"
PRESET_RELEASE="clang-release"
BUILD_DIR="$PROJECT_DIR/build"
BUILD_DIR_REL="$PROJECT_DIR/build-release"
SHADER_SRC_DIR="$PROJECT_DIR/assets/shaders"
SHADER_OUT_DIR="$PROJECT_DIR/build/shaders"
SHADER_OUT_DIR_REL="$PROJECT_DIR/build-release/shaders"

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "не найден: $1"; }

usage() {
    cat <<EOF
${B}sjoka${N} — модульный движок cjoka (C++20 / Clang / OpenGL 3.3)

${B}Использование:${N}
  ${G}./sjoka.sh compile engine${N} [--release|--debug]  собрать только движок (engine/libcjoka_engine.a)
  ${G}./sjoka.sh compile game${N}   [--release|--debug]  собрать игру (build/cjoka)
  ${G}./sjoka.sh compile all${N}    [--release|--debug]  собрать всё
  ${G}./sjoka.sh play game${N}      [--release]          собрать и запустить
  ${G}./sjoka.sh precompile shaders${N}                  прекомпилировать шейдеры → binary (.spv + .h)
  ${G}./sjoka.sh clean${N}                               удалить build*/ 
  ${G}./sjoka.sh help${N}

${B}Примеры:${N}
  ./sjoka.sh compile engine
  ./sjoka.sh compile engine --release
  ./sjoka.sh play game
  ./sjoka.sh precompile shaders

${B}Шейдеры:${N}
  Исходники:  ${D}assets/shaders/*.vert, *.frag, *.glsl${N}
  Выход:      ${D}build/shaders/*.spv${N} + ${D}build/shaders/shaders.h${N} (xxd)
  Если нет glslc/glslangValidator — делается только embed через xxd.
EOF
}

# --- preset helpers ---
preset_for_mode() {
    local mode="${1:-debug}"
    if [[ "$mode" == "release" ]]; then
        echo "$PRESET_RELEASE"
    else
        echo "$PRESET_DEBUG"
    fi
}
build_dir_for_mode() {
    local mode="${1:-debug}"
    if [[ "$mode" == "release" ]]; then echo "$BUILD_DIR_REL"; else echo "$BUILD_DIR"; fi
}

# --- compile ---
compile_engine() {
    local mode="${1:-debug}"
    local preset; preset="$(preset_for_mode "$mode")"
    local bdir; bdir="$(build_dir_for_mode "$mode")"
    need_cmd cmake; need_cmd ninja; need_cmd clang; need_cmd clang++
    info "preset: $preset ($mode) → $bdir"
    cmake --preset "$preset" 2>&1 | tail -n 20
    # собираем только движок: libcjoka_engine + libglad
    cmake --build --preset "${preset/clang-/}" --target cjoka_engine --target glad -j"$(nproc)" 2>&1 | tail -n 20
    ok "engine собран: $bdir/engine/libcjoka_engine.a ($(du -h "$bdir/engine/libcjoka_engine.a" 2>/dev/null | cut -f1))"
    # compile_commands уже в bdir/compile_commands.json → symlink для clangd
    if [[ -f "$bdir/compile_commands.json" && ! -f "$PROJECT_DIR/compile_commands.json" ]]; then
        ln -sf "$bdir/compile_commands.json" "$PROJECT_DIR/compile_commands.json" 2>/dev/null || true
    fi
}

compile_game() {
    local mode="${1:-debug}"
    local preset; preset="$(preset_for_mode "$mode")"
    local bdir; bdir="$(build_dir_for_mode "$mode")"
    need_cmd cmake; need_cmd ninja
    info "preset: $preset ($mode) → $bdir"
    cmake --preset "$preset" 2>&1 | tail -n 20
    cmake --build --preset "${preset/clang-/}" -j"$(nproc)" 2>&1 | tail -n 20
    ok "game собран: $bdir/cjoka ($(du -h "$bdir/cjoka" 2>/dev/null | cut -f1))"
}

compile_all() { compile_game "$@"; }

# --- play ---
play_game() {
    local mode="${1:-debug}"
    local preset; preset="$(preset_for_mode "$mode")"
    local bdir; bdir="$(build_dir_for_mode "$mode")"
    local bin="$bdir/cjoka"
    if [[ ! -f "$bin" ]]; then
        warn "бинарник не найден, собираю ($mode)…"
        compile_game "$mode"
    fi
    if [[ ! -x "$bin" ]]; then die "бинарник не исполняемый: $bin"; fi
    if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
        warn "нет DISPLAY/WAYLAND_DISPLAY — GLFW может не открыть окно (headless)"
    fi
    # LSAN на GLFW/libdecor дает 423KB ложных ликов — глушим
    local supp="$PROJECT_DIR/engine/sanitizer/lsan.supp"
    if [[ -f "$supp" ]]; then
        export LSAN_OPTIONS="suppressions=$supp:${LSAN_OPTIONS:-}"
    else
        export ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}"
        # также глушим через LSAN_OPTIONS если ASAN не сработает
        export LSAN_OPTIONS="detect_leaks=0:${LSAN_OPTIONS:-}"
    fi
    # Если хочешь видеть лики своего кода — запусти с ASAN_OPTIONS=detect_leaks=1
    if [[ "${SJOKA_LEAK_CHECK:-0}" == "1" ]]; then
        unset ASAN_OPTIONS
        export LSAN_OPTIONS="suppressions=$supp"
        warn "SJOKA_LEAK_CHECK=1 — лики включены (будут отчеты GLFW)"
    fi
    info "запуск: $bin (LSAN suppressed)"
    echo -e "${D}────────────────────────────────────────${N}"
    exec "$bin"
}

# --- precompile shaders ---
ensure_example_shaders() {
    if [[ -d "$SHADER_SRC_DIR" && -n "$(find "$SHADER_SRC_DIR" -maxdepth 2 -name '*.vert' -o -name '*.frag' -o -name '*.glsl' 2>/dev/null | head -n1)" ]]; then
        return 0
    fi
    info "создаю пример шейдеров в $SHADER_SRC_DIR/"
    mkdir -p "$SHADER_SRC_DIR"
    cat > "$SHADER_SRC_DIR/default.vert" <<'GLSL'
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
layout(location=3) in vec2 aUV;
out vec3 vColor;
uniform mat4 uMVP;
void main(){ vColor=aColor; gl_Position=uMVP*vec4(aPos,1.0); }
GLSL
    cat > "$SHADER_SRC_DIR/default.frag" <<'GLSL'
#version 330 core
in vec3 vColor; out vec4 FragColor;
void main(){ FragColor=vec4(vColor,1.0); }
GLSL
    cat > "$SHADER_SRC_DIR/fullscreen.vert" <<'GLSL'
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV=(aPos+1.0)*0.5; gl_Position=vec4(aPos,0,1); }
GLSL
    cat > "$SHADER_SRC_DIR/fullscreen.frag" <<'GLSL'
#version 330 core
in vec2 vUV; out vec4 FragColor;
uniform sampler2D uTex;
void main(){ FragColor=texture(uTex,vUV); }
GLSL
}

precompile_shaders() {
    local mode="${1:-debug}"
    local bdir; bdir="$(build_dir_for_mode "$mode")"
    local outdir="$bdir/shaders"
    if [[ "$mode" == "release" ]]; then outdir="$SHADER_OUT_DIR_REL"; else outdir="$SHADER_OUT_DIR"; fi
    # также дублируем в assets/shaders/binary для хранения в репо (опционально)
    local assetsBin="$PROJECT_DIR/assets/shaders/binary"

    ensure_example_shaders

    local files
    files="$(find "$SHADER_SRC_DIR" -maxdepth 2 -type f \( -name '*.vert' -o -name '*.frag' -o -name '*.glsl' -o -name '*.vs' -o -name '*.fs' \) 2>/dev/null | sort || true)"
    if [[ -z "$files" ]]; then die "не найдены шейдеры в $SHADER_SRC_DIR"; fi

    mkdir -p "$outdir" "$assetsBin"
    need_cmd xxd

    local has_glslc=0 has_glslang=0
    if command -v glslc >/dev/null 2>&1; then has_glslc=1; fi
    if command -v glslangValidator >/dev/null 2>&1; then has_glslang=1; fi

    if [[ $has_glslc -eq 0 && $has_glslang -eq 0 ]]; then
        warn "glslc/glslangValidator не найден — SPIR-V не соберется, делаю только embed (xxd). Установи: sudo apt install glslang-tools"
    fi

    local count=0 spv_count=0
    local gen_header="$outdir/shaders.h"
    local gen_binlist="$outdir/shaders.binlist"

    # заголовок для embed
    {
        echo "// Auto-generated by sjoka.sh precompile shaders — $(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "// mode=$mode  source=$SHADER_SRC_DIR  out=$outdir"
        echo "#pragma once"
        echo "#include <cstddef>"
        echo ""
    } > "$gen_header"
    : > "$gen_binlist"

    while IFS= read -r src; do
        [[ -z "$src" ]] && continue
        local base; base="$(basename "$src")"
        local name; name="${base%.*}"
        local ext;  ext="${base##*.}"
        local spv="$outdir/${base}.spv"
        local spvAssets="$assetsBin/${base}.spv"

        # 1) SPIR-V
        local did_spv=0
        if [[ $has_glslc -eq 1 ]]; then
            if glslc "$src" -o "$spv" 2>"$outdir/${base}.log"; then
                cp -f "$spv" "$spvAssets" 2>/dev/null || true
                info "SPIR-V glslc: $base → $(du -h "$spv"|cut -f1)"
                did_spv=1; spv_count=$((spv_count+1))
            else
                warn "glslc fail $base: $(cat "$outdir/${base}.log")"
            fi
        elif [[ $has_glslang -eq 1 ]]; then
            if glslangValidator -V "$src" -o "$spv" 2>"$outdir/${base}.log"; then
                cp -f "$spv" "$spvAssets" 2>/dev/null || true
                info "SPIR-V glslang: $base → $(du -h "$spv"|cut -f1)"
                did_spv=1; spv_count=$((spv_count+1))
            else
                warn "glslang fail $base"
            fi
        fi

        # 2) embed xxd → header + raw bin
        local bin="$outdir/${base}.bin"
        # бинарник = исходник как есть (для загрузки в движке) + xxd header
        cp -f "$src" "$bin"
        local varname; varname="$(echo "${name}_${ext}" | tr '.-' '_' | tr '[:lower:]' '[:upper:]')"
        # xxd генерирует unsigned char + unsigned int
        xxd -i "$src" | sed "s/unsigned char/const unsigned char/g; s/unsigned int/const unsigned int/g" >> "$gen_header"
        # добавим удобные алиасы
        {
            echo "static const char* ${varname}_PATH = \"${src}\";"
            echo ""
        } >> "$gen_header"

        echo "${src}|${bin}|${spv}|$(wc -c < "$src")|${did_spv}" >> "$gen_binlist"
        count=$((count+1))
    done <<< "$files"

    # 3) единый бинарный бандл (конкатенация + индекс)
    local bundle="$outdir/shaders.bundle"
    cat $outdir/*.bin 2>/dev/null > "$bundle" || true
    local bundleAssets="$assetsBin/shaders.bundle"
    cp -f "$bundle" "$bundleAssets" 2>/dev/null || true

    # 4) C++ хедер уже готов; также сгенерим простой манифест
    {
        echo ""
        echo "// Manifest: $count shaders, $spv_count SPIR-V"
        echo "// Generated bundle: shaders.bundle ($(du -h "$bundle" 2>/dev/null|cut -f1))"
    } >> "$gen_header"
    cp -f "$gen_header" "$assetsBin/shaders.h" 2>/dev/null || true

    ok "шейдеры: $count файлов → $outdir"
    if [[ $spv_count -gt 0 ]]; then ok "SPIR-V: $spv_count → $outdir/*.spv + $assetsBin/"; fi
    info "embed header: $gen_header ($(wc -l < "$gen_header") lines)"
    info "bundle: $bundle ($(du -h "$bundle" 2>/dev/null|cut -f1))"
    info "binlist: $gen_binlist"
    cat "$gen_binlist" | while IFS='|' read -r a b c d e; do echo "  - $(basename "$a") ${d}B spv=$e"; done
    ok "precompile done ($mode)"
}

clean_all() {
    info "удаляю build*/"
    rm -rf "$BUILD_DIR" "$BUILD_DIR_REL" "$PROJECT_DIR/build" "$PROJECT_DIR/build-release" 2>/dev/null || true
    rm -f "$PROJECT_DIR/compile_commands.json" 2>/dev/null || true
    ok "clean done (оставлен assets/shaders/binary кэш)"
}

# --- parse ---
MODE="debug"
CMD1="${1:-help}"
CMD2="${2:-}"
# флаг --release/--debug может быть в $2 или $3
for a in "$@"; do
    if [[ "$a" == "--release" ]]; then MODE="release"; fi
    if [[ "$a" == "--debug" ]]; then MODE="debug"; fi
done

case "$CMD1" in
    compile)
        case "$CMD2" in
            engine) compile_engine "$MODE" ;;
            game)   compile_game "$MODE" ;;
            all|"") compile_all "$MODE" ;;
            *) die "unknown compile target: $CMD2 (engine|game|all)" ;;
        esac
        ;;
    play)
        case "$CMD2" in
            game|"") play_game "$MODE" ;;
            *) die "unknown play target: $CMD2 (game)" ;;
        esac
        ;;
    precompile)
        # поддерживаем: precompile shaders  и  precompile shaders write to binary
        if [[ "$CMD2" == "shaders" || "$CMD2" == "shader" || -z "$CMD2" ]]; then
            precompile_shaders "$MODE"
        else
            die "unknown precompile target: $CMD2 (shaders)"
        fi
        ;;
    clean) clean_all ;;
    help|--help|-h) usage ;;
    *) err "неизвестная команда: $CMD1"; echo ""; usage; exit 1 ;;
esac
