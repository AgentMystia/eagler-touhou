// th16 ANM 绘制实现——地址见 AnmRenderer.hpp。
#include "AnmRenderer.hpp"

#include <cmath>
#include <cstring>

namespace th16 {

using namespace touhou::graphics;

namespace {

u32 pack_color(const u8 bgra[4]) {
    u32 c;
    std::memcpy(&c, bgra, 4);   // 内存序 B,G,R,A → D3DCOLOR 0xAARRGGBB
    return c;
}

// 两个 D3DCOLOR 逐通道相乘（>>7 后钳 255）——原版顶点色 × 父色 / × 色调的公共形
u32 modulate_color(u32 color, u32 factor) {
    u32 out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        u32 v = ((factor >> shift) & 0xffu) * ((color >> shift) & 0xffu) >> 7;
        if (v > 0xffu) v = 0xffu;
        out |= v << shift;
    }
    return out;
}

// 0x464f10：flags 的寻址位 → D3DTADDRESS；3 保持上一次设置（原版不下发）
bool address_mode(u32 bits, Address& out) {
    switch (bits & 3u) {
    case 0: out = Address::Repeat; return true;
    case 1: out = Address::Clamp; return true;
    case 2: out = Address::Mirror; return true;
    default: return false;
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// 相机与 Supervisor 缓存
// ---------------------------------------------------------------------------------------------

void AnmRenderer::use_camera(const Camera& camera) {
    camera_ = &camera;
    if (device == nullptr) return;
    flush();
    device->set_matrix(MatrixKind::View, camera.view);
    device->set_matrix(MatrixKind::Projection, camera.projection);
    draw_offset_x_ = camera.shift_x;
    draw_offset_y_ = camera.shift_y;
    device->set_viewport(camera.viewport);
}

void AnmRenderer::set_fog(bool enabled) {
    if (fog_cache_ == (enabled ? 1 : 0)) return;
    fog_cache_ = enabled ? 1 : 0;
    if (device == nullptr) return;
    flush();
    device->pipeline().fog = enabled;
}

void AnmRenderer::set_depth_write(bool enabled) {
    if (depth_write_cache_ == (enabled ? 1 : 0)) return;
    depth_write_cache_ = enabled ? 1 : 0;
    if (device == nullptr) return;
    flush();
    device->pipeline().depthWrite = enabled;
}

void AnmRenderer::set_depth_compare(Compare compare) {
    if (device == nullptr) return;
    flush();
    device->pipeline().depthCompare = compare;
}

void AnmRenderer::set_fog_parameters(u32 color, float near_, float far_) {
    if (device == nullptr) return;
    flush();
    PipelineState& p = device->pipeline();
    p.fogColor = color;
    p.fogNear = near_;
    p.fogFar = far_;
}

// ---------------------------------------------------------------------------------------------
// 批与缓存
// ---------------------------------------------------------------------------------------------

void AnmRenderer::ensure_modulate_combiner() {
    if (combiner_args_set_ == 1) return;
    combiner_args_set_ = 1;
    PipelineState& p = device->pipeline();
    p.alpha.operation = ColorOperation::Multiply;
    p.color.operation = ColorOperation::Multiply;
}

void AnmRenderer::flush() {
    if (batch_.empty() || device == nullptr) return;
    // 0x465a80：ALPHAARG2 / COLORARG2 = DIFFUSE，FVF XYZRHW|DIFFUSE|TEX1，三角形列表
    ensure_modulate_combiner();
    PipelineState& p = device->pipeline();
    p.alpha.second = {ArgumentSource::Diffuse};
    p.color.second = {ArgumentSource::Diffuse};
    p.textureTransform = false;
    device->draw(Topology::Triangles, VertexLayout::ScreenColorUv, u32(batch_.size() / 3), batch_.data(),
                 sizeof(ScreenColorUvVertex));
    batch_.clear();
}

void AnmRenderer::begin_frame() {
    batch_.clear();
    fog_cache_ = -1;
    set_fog(false);
}

void AnmRenderer::end_frame() {
    flush();
    if (device == nullptr) return;
    device->bind_texture(TextureRef::none());
}

void AnmRenderer::reset_state_cache() {
    texture_matrix_sprite_ = nullptr;
    texture_key_cache_ = -1;
    blend_cache_ = 0x0a;
    combiner_args_set_ = 0xff;
    filter_cache_ = 0xff;
    address_u_cache_ = 0xff;
    address_v_cache_ = 0xff;
    vertex_format_cache_ = VertexFormat::Unknown;
    tint_enabled_ = false;
    tint_ = 0x80808080;
    draw_offset_x_ = draw_offset_y_ = 0;
}

void AnmRenderer::reset_state_cache_after_device_defaults() {
    blend_cache_ = 0x0a;
    vertex_format_cache_ = VertexFormat::Unknown;
    texture_key_cache_ = -1;
}

u32 AnmRenderer::apply_tint(u32 color) const {
    return tint_enabled_ ? modulate_color(color, tint_) : color;
}

void AnmRenderer::bind_sprite_texture(const AnmRuntimeSprite& sprite) {
    if (texture_key_cache_ == sprite.key) return;
    texture_key_cache_ = sprite.key;
    flush();
    device->bind_texture({sprite.file_slot, sprite.entry_index});
}

void AnmRenderer::apply_vm_state(const AnmVm& vm) {
    PipelineState& p = device->pipeline();
    const u8 blend = u8((vm.flags_lo >> 5) & 0xfu);
    if (blend_cache_ != blend) {
        flush();
        blend_cache_ = blend;
        p.alphaTest = true;
        auto set = [&](BlendFactor src, BlendFactor dst, BlendEquation op) {
            p.sourceBlend = src;
            p.destinationBlend = dst;
            p.blendEquation = op;
        };
        switch (AnmBlendMode(blend)) {
        case AnmBlendMode::Alpha: set(BlendFactor::SourceAlpha, BlendFactor::InverseSourceAlpha, BlendEquation::Add); break;
        case AnmBlendMode::Additive: set(BlendFactor::SourceAlpha, BlendFactor::One, BlendEquation::Add); break;
        case AnmBlendMode::Subtractive: set(BlendFactor::SourceAlpha, BlendFactor::One, BlendEquation::ReverseSubtract); break;
        case AnmBlendMode::Replace:
            p.alphaTest = false;
            set(BlendFactor::One, BlendFactor::Zero, BlendEquation::Add);
            break;
        case AnmBlendMode::Screen: set(BlendFactor::InverseDestinationColor, BlendFactor::InverseSourceColor, BlendEquation::Add); break;
        case AnmBlendMode::Multiply: set(BlendFactor::DestinationColor, BlendFactor::Zero, BlendEquation::Add); break;
        case AnmBlendMode::InverseMultiply: set(BlendFactor::InverseSourceColor, BlendFactor::InverseSourceAlpha, BlendEquation::Add); break;
        case AnmBlendMode::DestinationAlpha: set(BlendFactor::DestinationAlpha, BlendFactor::InverseDestinationAlpha, BlendEquation::Add); break;
        case AnmBlendMode::Darken: set(BlendFactor::SourceAlpha, BlendFactor::One, BlendEquation::Minimum); break;
        case AnmBlendMode::Lighten: set(BlendFactor::SourceAlpha, BlendFactor::One, BlendEquation::Maximum); break;
        default: break;   // ≥ 10：原版只更新缓存，不改混合参数
        }
    }
    const u8 nearest = u8((vm.flags_hi >> 11) & 1u);   // ins_311
    if (filter_cache_ != nearest) {
        flush();
        filter_cache_ = nearest;
        p.magFilter = p.minFilter = nearest ? Filter::Nearest : Filter::Linear;
    }
    const u8 address_u = u8(vm.flags_hi & 3u);
    if (address_u_cache_ != address_u) {
        flush();
        address_u_cache_ = address_u;
        Address mode;
        if (address_mode(address_u, mode)) p.addressU = mode;
    }
    const u8 address_v = u8(vm.flags_lo >> 30);
    if (address_v_cache_ != address_v) {
        flush();
        address_v_cache_ = address_v;
        Address mode;
        if (address_mode(address_v, mode)) p.addressV = mode;
    }
}

bool AnmRenderer::in_camera_viewport(const float corners[4][2]) const {
    // 0x465280：四角包围盒与当前相机视口（DAT_004c17b8 +0xe0）不相交则跳过
    if (camera_ == nullptr) return true;
    const Viewport& vp = camera_->viewport;
    float min_x = corners[0][0], max_x = corners[0][0], min_y = corners[0][1], max_y = corners[0][1];
    for (int i = 1; i < 4; ++i) {
        if (corners[i][0] > max_x) max_x = corners[i][0];
        if (corners[i][0] < min_x) min_x = corners[i][0];
        if (corners[i][1] > max_y) max_y = corners[i][1];
        if (corners[i][1] < min_y) min_y = corners[i][1];
    }
    if (max_x < float(vp.x)) return false;
    if (max_y < float(vp.y)) return false;
    if (float(vp.x + vp.width) < min_x) return false;
    if (float(vp.y + vp.height) < min_y) return false;
    return true;
}

const AnmRuntimeSprite* AnmRenderer::sprite_of(const AnmVm& vm) const {
    if (vm.sprite_index < 0) return nullptr;
    // 精灵记录取自 UV 来源文件（ins_300(-1) 时为 ascii.anm，见 AnmVm::sprite_source_slot）
    const AnmFile* file = manager_.file(vm.sprite_source_slot >= 0 ? vm.sprite_source_slot : vm.file_slot);
    if (file == nullptr || u32(vm.sprite_index) >= file->sprites.size()) return nullptr;
    return &file->sprites[u32(vm.sprite_index)];
}

// ---------------------------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------------------------

int AnmRenderer::draw_vm(AnmVm& vm) {
    if (device == nullptr) return -1;
    if ((vm.flags_lo & AnmVm::kVisible) == 0 || (vm.flags_lo & AnmVm::kFlag2) == 0) return -1;
    if ((vm.flags_hi & AnmVm::kHiDeletingMask) != 0) return -1;
    set_depth_write(false);   // 0x43c570

    const AnmRuntimeSprite* sprite = sprite_of(vm);
    const bool has_alpha = vm.color[3] != 0 || vm.color2[3] != 0;
    const u32 mode = vm.render_mode();
    switch (AnmRenderMode(mode)) {
    case AnmRenderMode::PixelQuad:
    case AnmRenderMode::Quad: {
        if (!has_alpha || sprite == nullptr) return -1;
        float corners[4][2];
        geometry_.axis_corners(vm, corners);
        return submit_screen_quad(vm, *sprite, corners, mode == 0);
    }
    case AnmRenderMode::RotatedQuad:
    case AnmRenderMode::RotatedQuad2: {
        if (!has_alpha || sprite == nullptr) return -1;
        float corners[4][2];
        geometry_.rotated_corners(vm, corners);
        return submit_screen_quad(vm, *sprite, corners, false);
    }
    case AnmRenderMode::World:
        if (!has_alpha || sprite == nullptr) return -1;
        return draw_world_quad(vm, *sprite);
    case AnmRenderMode::WorldFogToggle:
        if (!has_alpha || sprite == nullptr) return -1;
        set_fog(true);    // 0x43c4b0
        draw_world_quad(vm, *sprite);
        set_fog(false);   // 0x43c4f0
        return 0;
    case AnmRenderMode::PolygonFan:
    case AnmRenderMode::PolygonOutline:
    case AnmRenderMode::PolygonRing:
        return draw_polygon(vm, AnmRenderMode(mode));
    case AnmRenderMode::Rect:
    case AnmRenderMode::GradientRect:
    case AnmRenderMode::OutlinedRect:
    case AnmRenderMode::OutlinedGradientRect:
    case AnmRenderMode::Line:
    case AnmRenderMode::RectOutline:
        return draw_rectangle(vm, AnmRenderMode(mode));
    default:
        if (mode < 32) ++unimplemented_modes[mode];
        return 0;
    }
}

int AnmRenderer::submit_screen_quad(AnmVm& vm, const AnmRuntimeSprite& sprite, float corners[4][2], bool pixel_align) {
    // 0x465c40 / 0x4660b0 末尾：加位置；0x465280 开头：加绘制偏移（抖动）
    const Vec3 origin = geometry_.world_position(vm);
    for (int i = 0; i < 4; ++i) {
        corners[i][0] = corners[i][0] + origin.x + draw_offset_x_;
        corners[i][1] = corners[i][1] + origin.y + draw_offset_y_;
    }
    if (pixel_align) {
        // ROUND(x) − 0.5：四角对齐到像素中心（cvtss2si 就近取偶）
        auto snap = [](float v) { return std::nearbyintf(v) - 0.5f; };
        corners[0][0] = snap(corners[0][0]);
        corners[1][0] = snap(corners[1][0]);
        corners[0][1] = snap(corners[0][1]);
        corners[2][1] = snap(corners[2][1]);
        corners[1][1] = corners[0][1];
        corners[2][0] = corners[0][0];
        corners[3][0] = corners[1][0];
        corners[3][1] = corners[2][1];
    }
    const float z = vm.entity_offset.z + vm.pos.z + vm.pos2.z;
    for (int i = 0; i < 4; ++i) vm.drawn_corners[i] = {corners[i][0], corners[i][1], z};

    // UV：基 UV + 滚动，uv_scale 缩放宽 / 高分量
    const Vec2* uv = vm.uv;
    const Vec2 scroll = vm.uv_scroll;
    float u[4], v[4];
    u[0] = scroll.x + uv[0].x;
    v[0] = uv[0].y + scroll.y;
    u[1] = (uv[1].x - uv[0].x) * vm.uv_scale.x + scroll.x + uv[0].x;
    v[1] = uv[1].y + scroll.y;
    u[2] = uv[2].x + scroll.x;
    v[2] = (uv[2].y - uv[0].y) * vm.uv_scale.y + scroll.y + uv[0].y;
    u[3] = (uv[3].x - uv[2].x) * vm.uv_scale.x + scroll.x + uv[2].x;
    v[3] = (uv[3].y - uv[1].y) * vm.uv_scale.y + scroll.y + uv[1].y;

    if (!in_camera_viewport(corners)) return 0;

    bind_sprite_texture(sprite);
    if (vertex_format_cache_ != VertexFormat::ScreenQuads) {
        flush();
        vertex_format_cache_ = VertexFormat::ScreenQuads;
    }

    // 顶点色：color mode（flags_lo bit17..18）0/1 单色（1 取 color2），2 左右渐变，3 上下渐变
    u32 colors[4];
    const u32 color_mode = (vm.flags_lo >> 17) & 3u;
    if (color_mode >= 2) {
        const u32 c = apply_tint(pack_color(vm.color));
        const u32 c2 = apply_tint(pack_color(vm.color2));
        if (color_mode == 2) { colors[0] = c; colors[1] = c2; colors[2] = c; colors[3] = c2; }
        else { colors[0] = c; colors[1] = c; colors[2] = c2; colors[3] = c2; }
    } else {
        u32 c = pack_color(color_mode == 0 ? vm.color : vm.color2);
        // ins_315：子 VM 的顶点色再乘父 VM 上一次绘制用的颜色
        if ((vm.flags_hi & AnmVm::kHi315) != 0 && vm.parent != nullptr) c = modulate_color(c, vm.parent->drawn_color);
        vm.drawn_color = c;
        c = apply_tint(c);
        colors[0] = colors[1] = colors[2] = colors[3] = c;
    }

    apply_vm_state(vm);
    // 0x465b50：TL, TR, BL, TR, BL, BR
    const ScreenColorUvVertex quad[4] = {
        {corners[0][0], corners[0][1], z, 1.0f, colors[0], u[0], v[0]},
        {corners[1][0], corners[1][1], z, 1.0f, colors[1], u[1], v[1]},
        {corners[2][0], corners[2][1], z, 1.0f, colors[2], u[2], v[2]},
        {corners[3][0], corners[3][1], z, 1.0f, colors[3], u[3], v[3]},
    };
    for (int index : {0, 1, 2, 1, 2, 3}) batch_.push_back(quad[index]);
    return 0;
}

int AnmRenderer::draw_world_quad(AnmVm& vm, const AnmRuntimeSprite& sprite) {
    // 0x467410 入口只看 color 的 alpha
    if (vm.color[3] == 0) return 0;
    if (!batch_.empty()) flush();
    set_depth_write((vm.flags_lo & AnmVm::kZWriteDisable) == 0);   // 0x43c530 / 0x43c570

    float* world = vm.texture_matrix_copy;
    if ((vm.flags_lo & 0x10000u) == 0) {
        // 世界矩阵 = 精灵基矩阵（宽高/256）× scale·scale2 × 窗口缩放 × 欧拉旋转（顺序由 ins_437 决定）
        std::memcpy(world, vm.texture_matrix, sizeof vm.texture_matrix);
        world[0] = vm.scale2.x * vm.scale.x * world[0];
        world[5] = vm.scale2.y * vm.scale.y * world[5];
        vm.flags_lo &= ~AnmVm::kScaleDirty;
        const u32 scale_mode = vm.flags_hi & AnmVm::kHiScaleModeMask;
        if (scale_mode == 0x100000u) { world[0] *= geometry_.window_scale; world[5] = geometry_.window_scale * world[5]; }
        else if (scale_mode == 0x200000u) {
            world[0] = geometry_.window_scale * 0.5f * world[0];
            world[5] = geometry_.window_scale * 0.5f * world[5];
        }
        const Vec3 r = anm_world_rotation(vm);
        float rotation[16];
        auto rotate = [&](char axis, float angle) {
            if (angle == 0.0f) return;
            if (axis == 'x') matrix::rotation_x(rotation, angle);
            else if (axis == 'y') matrix::rotation_y(rotation, angle);
            else matrix::rotation_z(rotation, angle);
            matrix::multiply(world, world, rotation);
        };
        switch ((vm.flags_hi >> 2) & 7u) {
        case 0: rotate('x', r.x); rotate('y', r.y); rotate('z', r.z); break;
        case 1: rotate('x', r.x); rotate('z', r.z); rotate('y', r.y); break;
        case 2: rotate('y', r.y); rotate('x', r.x); rotate('z', r.z); break;
        case 3: rotate('y', r.y); rotate('z', r.z); rotate('x', r.x); break;
        case 4: rotate('z', r.z); rotate('x', r.x); rotate('y', r.y); break;
        case 5: rotate('z', r.z); rotate('y', r.y); rotate('x', r.x); break;
        default: break;
        }
        vm.flags_lo &= ~AnmVm::kRotationDirty;
    }

    // 平移行：(entity_offset + pos + pos2) − anchor_offset × scale × scale2 → 0x406a70；z 取未经变换的和
    float placed[16];
    std::memcpy(placed, world, sizeof placed);
    const Vec3 t = geometry_.transform_position(vm, Vec3{
        (vm.entity_offset.x + vm.pos.x + vm.pos2.x) - vm.anchor_offset.x * vm.scale.x * vm.scale2.x,
        (vm.entity_offset.y + vm.pos.y + vm.pos2.y) - vm.anchor_offset.y * vm.scale.y * vm.scale2.y,
        world[14],
    });
    placed[12] = t.x;
    placed[13] = t.y;
    placed[14] = vm.entity_offset.z + vm.pos.z + vm.pos2.z;

    apply_vm_state(vm);

    // D3DRS_TEXTUREFACTOR：渐变位非 0 取 color2，否则 color；再乘色调
    const u32 factor = apply_tint(pack_color((vm.flags_lo & AnmVm::kColorModeMask) == 0 ? vm.color : vm.color2));
    if (texture_factor_cache_ != factor) {
        flush();
        texture_factor_cache_ = factor;
        device->pipeline().textureFactor = factor;
    }
    device->set_matrix(MatrixKind::World, placed);
    bind_sprite_texture(sprite);

    // 纹理矩阵（D3DTTFF_COUNT2：平移在第 3 行）：u' = u × uv_scale.x × sprite_matrix[0] + (u0 + scroll.x)
    if (texture_matrix_sprite_ != &sprite || vm.uv_scroll.x != 0.0f || vm.uv_scroll.y != 0.0f ||
        vm.uv_scale.x != 1.0f || vm.uv_scale.y != 1.0f) {
        texture_matrix_sprite_ = &sprite;
        float texture[16];
        std::memcpy(texture, vm.sprite_matrix, sizeof texture);
        texture[0] = vm.uv_scale.x * vm.sprite_matrix[0];
        texture[5] = vm.uv_scale.y * vm.sprite_matrix[5];
        texture[8] = vm.uv[0].x + vm.uv_scroll.x;
        texture[9] = vm.uv[0].y + vm.uv_scroll.y;
        device->set_matrix(MatrixKind::Texture, texture);
    }

    PipelineState& p = device->pipeline();
    if (vertex_format_cache_ != VertexFormat::WorldQuad) {
        // SetStreamSource(0x46b900 九锚点四边形) + FVF XYZ|TEX1 + ALPHAARG2 / COLORARG2 = TFACTOR
        p.alpha.second = {ArgumentSource::Factor};
        p.color.second = {ArgumentSource::Factor};
        vertex_format_cache_ = VertexFormat::WorldQuad;
    }
    ensure_modulate_combiner();
    p.textureTransform = true;

    // 0x46b900 顶点缓冲：按锚点（anchorY*3 + anchorX）平移的 ±128 单位四边形，条带 TL TR BL BR
    const u32 anchor_x = (vm.flags_lo >> 21) & 3u;
    const u32 anchor_y = (vm.flags_lo >> 23) & 3u;
    const float ox = anchor_x == 1 ? 128.0f : anchor_x == 2 ? -128.0f : 0.0f;
    const float oy = anchor_y == 1 ? 128.0f : anchor_y == 2 ? -128.0f : 0.0f;
    const WorldUvVertex quad[4] = {
        {-128.0f + ox, -128.0f + oy, 0.0f, 0.0f, 0.0f},
        {128.0f + ox, -128.0f + oy, 0.0f, 1.0f, 0.0f},
        {-128.0f + ox, 128.0f + oy, 0.0f, 0.0f, 1.0f},
        {128.0f + ox, 128.0f + oy, 0.0f, 1.0f, 1.0f},
    };
    device->draw(Topology::Strip, VertexLayout::WorldUv, 2, quad, sizeof(WorldUvVertex));
    return 0;
}

namespace {

constexpr float kTwoPi = 6.2831855f;
constexpr float kPi = 3.1415927f;

// 0x469640 等的角度回绕：每步最多修正 0x22 圈，把角度拉回 (-π, π]
float wrap_angle(float angle) {
    int steps = 0;
    while (angle > kPi && steps < 0x22) { angle -= kTwoPi; ++steps; }
    while (angle < -kPi && steps < 0x22) { angle += kTwoPi; ++steps; }
    return angle;
}

// 0x469e00：半径 r、角 a 的点（x87 cos / sin 后截成 float）
void polar_point(float angle, float radius, float& x, float& y) {
    x = float(std::cos(angle) * radius);
    y = float(std::sin(angle) * radius);
}

}  // namespace

void AnmRenderer::prepare_untextured_draw() {
    PipelineState& p = device->pipeline();
    // +0x184fbbb：MODULATE（贴图 × 顶点色）切回 SELECTARG2（只取顶点色）
    if (combiner_args_set_ != 0) {
        combiner_args_set_ = 0;
        p.alpha.operation = ColorOperation::Second;
        p.color.operation = ColorOperation::Second;
    }
    if (vertex_format_cache_ != VertexFormat::ScreenQuads) {
        vertex_format_cache_ = VertexFormat::ScreenQuads;
        p.alpha.second = {ArgumentSource::Diffuse};
        p.color.second = {ArgumentSource::Diffuse};
    }
    p.textureTransform = false;
}

int AnmRenderer::draw_polygon(AnmVm& vm, AnmRenderMode mode) {
    // 0x468490 mode 0x11..0x13 分支：半径取精灵宽 × scale，环宽取精灵高 × scale；随父旋转与缩放（ins_507 除外）
    float rotation = vm.rotation.z;
    float radius = vm.sprite_size.x * vm.scale.x;
    float ring_width = vm.sprite_size.y * vm.scale.y;
    const Vec3 center = geometry_.world_position(vm);
    if (vm.parent != nullptr && (vm.flags_hi & AnmVm::kHiDetached) == 0) {
        rotation += vm.parent->rotation.z;
        radius *= vm.parent->scale.x;
        ring_width *= vm.parent->scale.y;
    }
    const u32 scale_mode = vm.flags_hi & AnmVm::kHiScaleModeMask;
    if (scale_mode == 0x100000u) { radius *= geometry_.window_scale; ring_width *= geometry_.window_scale; }
    else if (scale_mode == 0x200000u) {
        radius = geometry_.window_scale * 0.5f * radius;
        ring_width = geometry_.window_scale * 0.5f * ring_width;
    }
    apply_vm_state(vm);
    const i32 segments = vm.int_vars[0];
    if (segments < 0) return 0;   // 原版的 (segments + 1 > 0) 守卫
    const u32 color = pack_color(vm.color);
    const float step = kTwoPi / float(segments);
    std::vector<ScreenColorVertex> vertices;
    vertices.reserve(size_t(segments) * 2 + 2);
    auto rim = [&](float angle, float r, u32 c) {
        float x, y;
        polar_point(angle, r, x, y);
        vertices.push_back({x + center.x, y + center.y, 0.0f, 1.0f, c});
    };

    flush();   // 0x465a80：先把之前的贴图四边形批画掉
    Topology topology;
    u32 primitives;
    switch (mode) {
    case AnmRenderMode::PolygonFan: {
        vertices.push_back({center.x, center.y, 0.0f, 1.0f, color});
        const u32 rim_color = pack_color(vm.color2);
        for (i32 i = 0; i <= segments; ++i) {
            rim(rotation, radius, rim_color);
            rotation = wrap_angle(rotation + step);
        }
        // 0x469640 独有：关 Z 写、ZFUNC = ALWAYS
        set_depth_write(false);
        flush();
        device->pipeline().depthCompare = Compare::Always;
        topology = Topology::Fan;
        primitives = u32(segments);
        break;
    }
    case AnmRenderMode::PolygonOutline:
        for (i32 i = 0; i <= segments; ++i) {
            rim(rotation, radius, color);
            rotation = wrap_angle(rotation + step);
        }
        topology = Topology::LineStrip;
        primitives = u32(segments);
        break;
    default: {
        const float inner = radius - ring_width * 0.5f;
        const float outer = ring_width * 0.5f + radius;
        for (i32 i = 0; i <= segments; ++i) {
            rim(rotation, inner, color);
            rim(rotation, outer, color);
            rotation = wrap_angle(rotation + step);
        }
        topology = Topology::Strip;
        primitives = u32(segments) * 2;
        break;
    }
    }
    prepare_untextured_draw();
    device->draw(topology, VertexLayout::ScreenColor, primitives, vertices.data(), sizeof(ScreenColorVertex));
    return 0;
}

namespace {

// 0x468c70 / 0x469330 / 0x468fc0 共用：锚点 0 居中、1 从原点向正方向、2 向负方向
void anchor_extent(u32 anchor, float size, float& from, float& to) {
    if (anchor == 1) { from = 0.0f; to = size; }
    else if (anchor == 2) { from = -size; to = 0.0f; }
    else { from = size * -0.5f; to = size * 0.5f; }
}

// 0x469570：描边底色——alpha 减半（(c >> 1) & 0x7f000000），RGB 不变
u32 half_alpha(u32 color) { return ((color >> 1) & 0x7f000000u) | (color & 0x00ffffffu); }

}  // namespace

void AnmRenderer::submit_rectangle(float x, float y, float width, float height, float rotation, u32 c1, u32 c2,
                                   u32 anchor_x, u32 anchor_y) {
    flush();
    const float c = float(std::cos(rotation)), s = float(std::sin(rotation));
    float left, right, top, bottom;
    anchor_extent(anchor_x, width, left, right);
    anchor_extent(anchor_y, height, top, bottom);
    auto corner = [&](float dx, float dy, u32 color) {
        return ScreenColorVertex{(dx * c - dy * s) + x, dx * s + dy * c + y, 0.0f, 1.0f, color};
    };
    const ScreenColorVertex quad[4] = {
        corner(left, top, c1), corner(right, top, c2), corner(left, bottom, c1), corner(right, bottom, c2),
    };
    set_depth_write(false);
    prepare_untextured_draw();
    device->draw(Topology::Strip, VertexLayout::ScreenColor, 2, quad, sizeof(ScreenColorVertex));
}

int AnmRenderer::draw_rectangle(AnmVm& vm, AnmRenderMode mode) {
    // 0x468490 mode 0x10 / 0x14.. 分支：宽高 = 精灵尺寸 × scale，随父旋转与缩放（ins_507 除外），再乘窗口缩放
    float rotation = vm.rotation.z;
    float width = vm.sprite_size.x * vm.scale.x;
    float height = vm.sprite_size.y * vm.scale.y;
    const Vec3 origin = geometry_.world_position(vm);
    if (vm.parent != nullptr && (vm.flags_hi & AnmVm::kHiDetached) == 0) {
        width *= vm.parent->scale.x;
        height *= vm.parent->scale.y;
        rotation += vm.parent->rotation.z;
    }
    apply_vm_state(vm);
    const u32 scale_mode = vm.flags_hi & AnmVm::kHiScaleModeMask;
    if (scale_mode == 0x100000u) { width *= geometry_.window_scale; height *= geometry_.window_scale; }
    else if (scale_mode == 0x200000u) {
        width = geometry_.window_scale * 0.5f * width;
        height = geometry_.window_scale * 0.5f * height;
    }
    // 顶点色直接取 VM 的 color / color2（不经色调与父色调制）；"第二色" 按渐变位（ins_423）选 color2
    const u32 color = pack_color(vm.color);
    const u32 color2 = pack_color(vm.color2);
    const u32 color_by_mode = (vm.flags_lo & AnmVm::kColorModeMask) == 0 ? color : color2;
    const u32 anchor_x = (vm.flags_lo & AnmVm::kAnchorXMask) >> 21;
    const u32 anchor_y = (vm.flags_lo & AnmVm::kAnchorYMask) >> 23;

    switch (mode) {
    case AnmRenderMode::Rect:
        submit_rectangle(origin.x, origin.y, width, height, rotation, color, color, anchor_x, anchor_y);
        return 0;
    case AnmRenderMode::GradientRect:
        submit_rectangle(origin.x, origin.y, width, height, rotation, color, color2, anchor_x, anchor_y);
        return 0;
    case AnmRenderMode::OutlinedRect:
    case AnmRenderMode::OutlinedGradientRect: {
        const u32 c2 = mode == AnmRenderMode::OutlinedRect ? color : color2;
        submit_rectangle(origin.x, origin.y, width + 1.0f, height + 1.0f, rotation, half_alpha(color), half_alpha(c2),
                         anchor_x, anchor_y);
        submit_rectangle(origin.x, origin.y, width, height, rotation, color, c2, anchor_x, anchor_y);
        return 0;
    }
    case AnmRenderMode::Line: {
        // 0x469330：只沿 X 延伸的两点线段，旋转后平移到 VM 位置
        flush();
        const float c = float(std::cos(rotation)), s = float(std::sin(rotation));
        float from, to;
        anchor_extent(anchor_x, width, from, to);
        const ScreenColorVertex line[2] = {
            {from * c + origin.x, from * s + origin.y, 0.0f, 1.0f, color},
            {to * c + origin.x, to * s + origin.y, 0.0f, 1.0f, color_by_mode},
        };
        set_depth_write(false);
        prepare_untextured_draw();
        device->draw(Topology::Lines, VertexLayout::ScreenColor, 1, line, sizeof(ScreenColorVertex));
        return 0;
    }
    default: {
        // 0x468fc0：左上 → 右上 → 右下 → 左下 → 左上 的线段带；上边两角与左下取 color，右侧取第二色
        flush();
        const float c = float(std::cos(rotation)), s = float(std::sin(rotation));
        float left, right, top, bottom;
        anchor_extent(anchor_x, width, left, right);
        anchor_extent(anchor_y, height, top, bottom);
        auto corner = [&](float dx, float dy, u32 col) {
            return ScreenColorVertex{(dx * c - dy * s) + origin.x, dx * s + dy * c + origin.y, 0.0f, 1.0f, col};
        };
        const ScreenColorVertex outline[5] = {
            corner(left, top, color), corner(right, top, color_by_mode), corner(right, bottom, color_by_mode),
            corner(left, bottom, color), corner(left, top, color),
        };
        set_depth_write(false);
        prepare_untextured_draw();
        device->draw(Topology::LineStrip, VertexLayout::ScreenColor, 4, outline, sizeof(ScreenColorVertex));
        return 0;
    }
    }
}

void AnmRenderer::draw_layer(int layer) {
    if (device == nullptr || layer < 0 || layer >= AnmManager::kLayerCount) return;
    for (AnmVm* vm = manager_.layer_chain(layer); vm != nullptr; vm = vm->layer_next) {
        if ((vm->flags_hi & AnmVm::kHiDeletingMask) != 0) continue;
        draw_vm(*vm);
    }
}

}  // namespace th16
