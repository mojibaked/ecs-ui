#include "ecs_ui/ecs_ui_raylib.h"
#include "ecs_ui_frame_internal.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *g_ecs_ui_raylib_text_buffer = NULL;
static size_t g_ecs_ui_raylib_text_buffer_len = 0u;

static const char *EcsUiRaylibFrameCString(
    const char *chars,
    int32_t length)
{
    size_t safe_length = 0u;
    if (length > 0) {
        safe_length = (size_t)length;
    }

    const size_t required = safe_length + 1u;
    if (required > g_ecs_ui_raylib_text_buffer_len) {
        char *next = realloc(
            g_ecs_ui_raylib_text_buffer,
            required);
        if (next == NULL) {
            return "";
        }
        g_ecs_ui_raylib_text_buffer = next;
        g_ecs_ui_raylib_text_buffer_len = required;
    }

    if (safe_length > 0u && chars != NULL) {
        memcpy(g_ecs_ui_raylib_text_buffer, chars, safe_length);
    }
    g_ecs_ui_raylib_text_buffer[safe_length] = '\0';
    return g_ecs_ui_raylib_text_buffer;
}

static Font EcsUiRaylibFrameFont(Font *fonts, uint16_t font_id)
{
    if (fonts == NULL || fonts[font_id].glyphs == NULL) {
        return GetFontDefault();
    }
    return fonts[font_id];
}

static float EcsUiRaylibFrameFontSize(Font font, float requested_size)
{
    if (requested_size > 0.0f) {
        return requested_size;
    }
    return font.baseSize > 0 ? (float)font.baseSize : 1.0f;
}

EcsUiSize EcsUiRaylibMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    Font *fonts = user_data;
    const uint16_t font_id = spec != NULL ? spec->font_id : 0u;
    Font font = EcsUiRaylibFrameFont(fonts, font_id);
    const float font_size = EcsUiRaylibFrameFontSize(
        font,
        spec != NULL ? spec->font_size : 0.0f);
    const float spacing = spec != NULL ? spec->letter_spacing : 1.0f;
    const char *value = EcsUiRaylibFrameCString(utf8, length);
    Vector2 size = MeasureTextEx(font, value, font_size, spacing);
    return (EcsUiSize){
        .width = size.x,
        .height = size.y,
    };
}

static float EcsUiRaylibPaintScale(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    const EcsUiRaylibRenderContext *root_context)
{
    (void)paint;
    if (root_context != NULL && root_context->scale > 0.0f) {
        return root_context->scale;
    }
    return tree != NULL && tree->scale > 0.0f ? tree->scale : 1.0f;
}

static Rectangle EcsUiRaylibPaintRootBounds(
    const EcsUiRaylibRenderContext *root_context)
{
    if (root_context == NULL) {
        return (Rectangle){0};
    }
    if (root_context->physical_root_bounds.width > 0.0f ||
            root_context->physical_root_bounds.height > 0.0f) {
        return root_context->physical_root_bounds;
    }
    return root_context->physical_bounds;
}

static Rectangle EcsUiRaylibPaintRect(
    EcsUiPaintRect rect,
    const EcsUiRaylibRenderContext *root_context,
    float scale)
{
    const Rectangle root = EcsUiRaylibPaintRootBounds(root_context);
    return (Rectangle){
        .x = roundf(root.x + rect.x * scale),
        .y = roundf(root.y + rect.y * scale),
        .width = roundf(rect.width * scale),
        .height = roundf(rect.height * scale),
    };
}

static Color EcsUiRaylibPaintColor(EcsUiColorF color, float opacity)
{
    float alpha = opacity;
    if (alpha < 0.0f) {
        alpha = 0.0f;
    } else if (alpha > 1.0f) {
        alpha = 1.0f;
    }
    return (Color){
        .r = (unsigned char)roundf(color.r),
        .g = (unsigned char)roundf(color.g),
        .b = (unsigned char)roundf(color.b),
        .a = (unsigned char)roundf(color.a * alpha),
    };
}

static void EcsUiRaylibPaintRenderCustomFallback(
    const EcsUiTreeNodeSnapshot *node,
    Rectangle bounds,
    EcsUiColorF fill_color,
    float opacity)
{
    Color fill = EcsUiRaylibPaintColor(fill_color, opacity);
    DrawRectangleRounded(bounds, 0.10f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.10f, 8, Fade(WHITE, 0.35f));
    if (node != NULL) {
        Font font = GetFontDefault();
        const float font_size = EcsUiRaylibFrameFontSize(font, 0.0f);
        DrawTextEx(
            font,
            node->custom.kind,
            (Vector2){
                .x = bounds.x + font_size,
                .y = bounds.y + font_size,
            },
            font_size,
            1.0f,
            Fade(WHITE, (float)fill.a / 255.0f));
    }
}

static bool EcsUiRaylibPaintIntersects(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width &&
        a.x + a.width > b.x &&
        a.y < b.y + b.height &&
        a.y + a.height > b.y;
}

static bool EcsUiRaylibPaintCulled(
    Rectangle bounds,
    const EcsUiRaylibRenderContext *root_context,
    const EcsUiRaylibDrawOptions *options)
{
    if (options == NULL || !options->culling_enabled) {
        return false;
    }
    Rectangle cull = options->culling_bounds;
    if (cull.width <= 0.0f && cull.height <= 0.0f) {
        cull = EcsUiRaylibPaintRootBounds(root_context);
    }
    if (cull.width <= 0.0f || cull.height <= 0.0f) {
        return false;
    }
    return !EcsUiRaylibPaintIntersects(bounds, cull);
}

static uint16_t EcsUiRaylibPaintU16(float value)
{
    if (value <= 0.0f) {
        return 0u;
    }
    if (value >= 65535.0f) {
        return 65535u;
    }
    return (uint16_t)value;
}

static const EcsUiTreeNodeSnapshot *EcsUiRaylibPaintFindNode(
    const EcsUiTreeSnapshot *tree,
    ecs_entity_t entity)
{
    if (tree == NULL || entity == 0) {
        return NULL;
    }
    for (uint32_t i = 0u; i < tree->count; i += 1u) {
        if (tree->nodes[i].entity == entity) {
            return &tree->nodes[i];
        }
    }
    return NULL;
}

static void EcsUiRaylibPaintRenderRectangle(
    Rectangle bounds,
    EcsUiColorF fill,
    EcsUiPaintCornerRadius radius,
    float opacity,
    float scale)
{
    Color color = EcsUiRaylibPaintColor(fill, opacity);
    const float top_left = radius.top_left * scale;
    if (top_left > 0.0f && bounds.width > 0.0f && bounds.height > 0.0f) {
        const float denominator =
            bounds.width > bounds.height ? bounds.height : bounds.width;
        const float roundness =
            denominator > 0.0f ? (top_left * 2.0f) / denominator : 0.0f;
        DrawRectangleRounded(bounds, roundness, 8, color);
    } else {
        DrawRectangleRec(bounds, color);
    }
}

static void EcsUiRaylibPaintRenderBorder(
    Rectangle bounds,
    EcsUiPaintBorder border,
    float opacity,
    float scale)
{
    Color color = EcsUiRaylibPaintColor(border.color, opacity);
    const uint16_t left = EcsUiRaylibPaintU16(border.left * scale);
    const uint16_t top = EcsUiRaylibPaintU16(border.top * scale);
    const uint16_t right = EcsUiRaylibPaintU16(border.right * scale);
    const uint16_t bottom = EcsUiRaylibPaintU16(border.bottom * scale);
    if (left > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y,
                .width = (float)left,
                .height = bounds.height,
            },
            color);
    }
    if (right > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x + bounds.width - (float)right,
                .y = bounds.y,
                .width = (float)right,
                .height = bounds.height,
            },
            color);
    }
    if (top > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y,
                .width = bounds.width,
                .height = (float)top,
            },
            color);
    }
    if (bottom > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y + bounds.height - (float)bottom,
                .width = bounds.width,
                .height = (float)bottom,
            },
            color);
    }
}

static EcsUiRaylibRenderContext EcsUiRaylibPaintContext(
    Rectangle bounds,
    const EcsUiRaylibRenderContext *root_context,
    float scale)
{
    const Rectangle root = EcsUiRaylibPaintRootBounds(root_context);
    return (EcsUiRaylibRenderContext){
        .physical_bounds = bounds,
        .physical_root_bounds = root,
        .logical_origin = {
            .x = scale > 0.0f ? root.x / scale : 0.0f,
            .y = scale > 0.0f ? root.y / scale : 0.0f,
        },
        .scale = scale > 0.0f ? scale : 1.0f,
    };
}

void EcsUiRaylibRenderPaintList(
    const EcsUiPaintList *paint,
    const EcsUiTreeSnapshot *tree,
    Font *fonts,
    const EcsUiRaylibRenderContext *root_context,
    const EcsUiRaylibDrawOptions *options)
{
    if (paint == NULL || tree == NULL) {
        return;
    }
    if (paint->generation != tree->generation) {
        (void)fprintf(
            stderr,
            "ecs-ui paint generation mismatch: paint=%u tree=%u\n",
            (unsigned int)paint->generation,
            (unsigned int)tree->generation);
        return;
    }
    /*
     * Paint text font sizes are already baked from tree->scale. Geometry must
     * use the same scale or glyphs and boxes drift apart.
     */
    if (root_context != NULL && root_context->scale > 0.0f &&
            tree->scale > 0.0f) {
        assert(fabsf(root_context->scale - tree->scale) <= 0.001f);
    }
    const float scale = EcsUiRaylibPaintScale(paint, tree, root_context);
    for (uint32_t i = 0u; i < paint->count; i += 1u) {
        const EcsUiPaintItem *item = &paint->items[i];
        Rectangle bounds =
            EcsUiRaylibPaintRect(item->rect, root_context, scale);
        if (item->primitive != ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE &&
                EcsUiRaylibPaintCulled(bounds, root_context, options)) {
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_CLIP_SCOPE &&
                item->key.role == ECS_UI_PAINT_ROLE_CLIP_SCOPE) {
            if (item->key.part == ECS_UI_PAINT_CLIP_SCOPE_START) {
                BeginScissorMode(
                    (int)roundf(bounds.x),
                    (int)roundf(bounds.y),
                    (int)roundf(bounds.width),
                    (int)roundf(bounds.height));
            } else {
                EndScissorMode();
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_TEXT_RUN &&
                item->key.role == ECS_UI_PAINT_ROLE_TEXT_RUN) {
            const EcsUiPaintTextRun *run = &item->payload.text_run;
            Font font = EcsUiRaylibFrameFont(fonts, run->font_id);
            const char *value = EcsUiRaylibFrameCString(
                run->text != NULL ? &run->text[run->byte_start] : "",
                (int32_t)(run->byte_end - run->byte_start));
            DrawTextEx(
                font,
                value,
                (Vector2){.x = bounds.x, .y = bounds.y},
                (float)run->font_size,
                (float)run->letter_spacing,
                EcsUiRaylibPaintColor(run->color, item->opacity));
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_BORDER &&
                item->key.role == ECS_UI_PAINT_ROLE_BORDER) {
            EcsUiRaylibPaintRenderBorder(
                bounds,
                item->payload.border,
                item->opacity,
                scale);
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_BOX) {
            if (item->key.role == ECS_UI_PAINT_ROLE_BEVEL_EDGE) {
                EcsUiRaylibPaintRenderRectangle(
                    bounds,
                    item->payload.bevel_edge.color,
                    (EcsUiPaintCornerRadius){0},
                    item->opacity,
                    scale);
            } else {
                EcsUiRaylibPaintRenderRectangle(
                    bounds,
                    item->payload.box.fill,
                    item->payload.box.radius,
                    item->opacity,
                    scale);
            }
            continue;
        }

        if (item->primitive == ECS_UI_PAINT_PRIMITIVE_CUSTOM) {
            const EcsUiTreeNodeSnapshot *node =
                EcsUiRaylibPaintFindNode(tree, item->payload.custom.source);
            const float opacity =
                item->opacity * (item->payload.custom.color.a / 255.0f);
            const EcsUiRaylibRenderContext context =
                EcsUiRaylibPaintContext(bounds, root_context, scale);
            if (item->key.role == ECS_UI_PAINT_ROLE_NINE_SLICE &&
                    node != NULL &&
                    node->kind != ECS_UI_NODE_CUSTOM &&
                    node->has_nine_slice_style) {
                if (options != NULL && options->nine_slice_draw != NULL) {
                    options->nine_slice_draw(
                        node,
                        &context,
                        opacity,
                        options->user_data);
                }
                continue;
            }
            if (item->key.role == ECS_UI_PAINT_ROLE_ICON &&
                    node != NULL &&
                    node->kind == ECS_UI_NODE_ICON) {
                if (options != NULL && options->icon_draw != NULL) {
                    options->icon_draw(
                        node,
                        &context,
                        opacity,
                        options->user_data);
                } else if (options != NULL && options->custom_draw != NULL) {
                    options->custom_draw(
                        node,
                        &context,
                        opacity,
                        options->user_data);
                }
                continue;
            }
            if (options != NULL && options->custom_draw != NULL) {
                options->custom_draw(
                    node,
                    &context,
                    opacity,
                    options->user_data);
            } else {
                EcsUiRaylibPaintRenderCustomFallback(
                    node,
                    bounds,
                    item->payload.custom.color,
                    item->opacity);
            }
        }
    }
}

void EcsUiRaylibReleaseFrameRenderer(void)
{
    free(g_ecs_ui_raylib_text_buffer);
    g_ecs_ui_raylib_text_buffer = NULL;
    g_ecs_ui_raylib_text_buffer_len = 0u;
}
