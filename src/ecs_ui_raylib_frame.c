#include "ecs_ui/ecs_ui_raylib.h"
#include "ecs_ui_frame_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *g_ecs_ui_raylib_draw_list_text_buffer = NULL;
static size_t g_ecs_ui_raylib_draw_list_text_buffer_len = 0u;

static const char *EcsUiRaylibDrawListCString(
    const char *chars,
    int32_t length)
{
    size_t safe_length = 0u;
    if (length > 0) {
        safe_length = (size_t)length;
    }

    const size_t required = safe_length + 1u;
    if (required > g_ecs_ui_raylib_draw_list_text_buffer_len) {
        char *next = realloc(
            g_ecs_ui_raylib_draw_list_text_buffer,
            required);
        if (next == NULL) {
            return "";
        }
        g_ecs_ui_raylib_draw_list_text_buffer = next;
        g_ecs_ui_raylib_draw_list_text_buffer_len = required;
    }

    if (safe_length > 0u && chars != NULL) {
        memcpy(g_ecs_ui_raylib_draw_list_text_buffer, chars, safe_length);
    }
    g_ecs_ui_raylib_draw_list_text_buffer[safe_length] = '\0';
    return g_ecs_ui_raylib_draw_list_text_buffer;
}

static Font EcsUiRaylibDrawListFont(Font *fonts, uint16_t font_id)
{
    if (fonts == NULL || fonts[font_id].glyphs == NULL) {
        return GetFontDefault();
    }
    return fonts[font_id];
}

static float EcsUiRaylibDrawListFontSize(Font font, float requested_size)
{
    if (requested_size > 0.0f) {
        return requested_size;
    }
    return font.baseSize > 0 ? (float)font.baseSize : 1.0f;
}

static Color EcsUiRaylibDrawListColor(Clay_Color color)
{
    return (Color){
        .r = (unsigned char)roundf(color.r),
        .g = (unsigned char)roundf(color.g),
        .b = (unsigned char)roundf(color.b),
        .a = (unsigned char)roundf(color.a),
    };
}

static Rectangle EcsUiRaylibDrawListRect(Clay_BoundingBox bounds)
{
    return (Rectangle){
        .x = roundf(bounds.x),
        .y = roundf(bounds.y),
        .width = roundf(bounds.width),
        .height = roundf(bounds.height),
    };
}

static EcsUiRaylibRenderContext EcsUiRaylibDrawListContext(
    const EcsUiRaylibRenderContext *root_context,
    Rectangle bounds)
{
    if (root_context == NULL) {
        return (EcsUiRaylibRenderContext){
            .physical_bounds = bounds,
            .physical_root_bounds = bounds,
            .logical_origin = {0},
            .scale = 1.0f,
        };
    }

    EcsUiRaylibRenderContext context = *root_context;
    context.physical_bounds = bounds;
    if (context.scale <= 0.0f) {
        context.scale = 1.0f;
    }
    return context;
}

EcsUiSize EcsUiRaylibMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    Font *fonts = user_data;
    const uint16_t font_id = spec != NULL ? spec->font_id : 0u;
    Font font = EcsUiRaylibDrawListFont(fonts, font_id);
    const float font_size = EcsUiRaylibDrawListFontSize(
        font,
        spec != NULL ? spec->font_size : 0.0f);
    const float spacing = spec != NULL ? spec->letter_spacing : 1.0f;
    const char *value = EcsUiRaylibDrawListCString(utf8, length);
    Vector2 size = MeasureTextEx(font, value, font_size, spacing);
    return (EcsUiSize){
        .width = size.x,
        .height = size.y,
    };
}

static void EcsUiRaylibDrawListRenderRectangle(
    Rectangle bounds,
    Clay_RectangleRenderData *config)
{
    if (config == NULL) {
        return;
    }
    Color color = EcsUiRaylibDrawListColor(config->backgroundColor);
    if (config->cornerRadius.topLeft > 0.0f && bounds.width > 0.0f &&
        bounds.height > 0.0f) {
        const float denominator =
            bounds.width > bounds.height ? bounds.height : bounds.width;
        const float radius = denominator > 0.0f ?
            (config->cornerRadius.topLeft * 2.0f) / denominator :
            0.0f;
        DrawRectangleRounded(bounds, radius, 8, color);
    } else {
        DrawRectangleRec(bounds, color);
    }
}

static void EcsUiRaylibDrawListRenderBorder(
    Rectangle bounds,
    Clay_BorderRenderData *config)
{
    if (config == NULL) {
        return;
    }
    Color color = EcsUiRaylibDrawListColor(config->color);
    if (config->width.left > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y,
                .width = (float)config->width.left,
                .height = bounds.height,
            },
            color);
    }
    if (config->width.right > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x + bounds.width - (float)config->width.right,
                .y = bounds.y,
                .width = (float)config->width.right,
                .height = bounds.height,
            },
            color);
    }
    if (config->width.top > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y,
                .width = bounds.width,
                .height = (float)config->width.top,
            },
            color);
    }
    if (config->width.bottom > 0u) {
        DrawRectangleRec(
            (Rectangle){
                .x = bounds.x,
                .y = bounds.y + bounds.height - (float)config->width.bottom,
                .width = bounds.width,
                .height = (float)config->width.bottom,
            },
            color);
    }
}

static void EcsUiRaylibDrawListRenderCustomFallback(
    const EcsUiTreeNodeSnapshot *node,
    Rectangle bounds,
    Clay_CustomRenderData *config)
{
    if (config == NULL) {
        return;
    }
    Color fill = EcsUiRaylibDrawListColor(config->backgroundColor);
    DrawRectangleRounded(bounds, 0.10f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.10f, 8, Fade(WHITE, 0.35f));
    if (node != NULL) {
        Font font = GetFontDefault();
        const float font_size = EcsUiRaylibDrawListFontSize(font, 0.0f);
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

void EcsUiRaylibRenderDrawList(
    const EcsUiDrawList *draw_list,
    Font *fonts,
    const EcsUiRaylibRenderContext *root_context,
    const EcsUiRaylibDrawOptions *options)
{
    const Clay_RenderCommandArray *draw_commands =
        EcsUiFrameDrawListClayCommands(draw_list);
    if (draw_commands == NULL) {
        return;
    }

    Clay_RenderCommandArray commands = *draw_commands;
    for (int32_t i = 0; i < commands.length; i += 1) {
        Clay_RenderCommand *command =
            Clay_RenderCommandArray_Get(&commands, i);
        if (command == NULL) {
            continue;
        }

        Rectangle bounds = EcsUiRaylibDrawListRect(command->boundingBox);
        switch (command->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *text = &command->renderData.text;
            Font font = EcsUiRaylibDrawListFont(fonts, text->fontId);
            const char *value = EcsUiRaylibDrawListCString(
                text->stringContents.chars,
                text->stringContents.length);
            DrawTextEx(
                font,
                value,
                (Vector2){.x = bounds.x, .y = bounds.y},
                (float)text->fontSize,
                (float)text->letterSpacing,
                EcsUiRaylibDrawListColor(text->textColor));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
            Clay_ImageRenderData *image = &command->renderData.image;
            if (image->imageData == NULL) {
                break;
            }
            Texture2D texture = *(Texture2D *)image->imageData;
            Clay_Color tint = image->backgroundColor;
            if (tint.r == 0.0f && tint.g == 0.0f && tint.b == 0.0f &&
                tint.a == 0.0f) {
                tint = (Clay_Color){255.0f, 255.0f, 255.0f, 255.0f};
            }
            DrawTexturePro(
                texture,
                (Rectangle){
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = (float)texture.width,
                    .height = (float)texture.height,
                },
                bounds,
                (Vector2){.x = 0.0f, .y = 0.0f},
                0.0f,
                EcsUiRaylibDrawListColor(tint));
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            BeginScissorMode(
                (int)roundf(bounds.x),
                (int)roundf(bounds.y),
                (int)roundf(bounds.width),
                (int)roundf(bounds.height));
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            EndScissorMode();
            break;
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            EcsUiRaylibDrawListRenderRectangle(
                bounds,
                &command->renderData.rectangle);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            EcsUiRaylibDrawListRenderBorder(
                bounds,
                &command->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData *custom = &command->renderData.custom;
            const EcsUiTreeNodeSnapshot *node = custom->customData;
            const float opacity = custom->backgroundColor.a / 255.0f;
            const EcsUiRaylibRenderContext draw_context =
                EcsUiRaylibDrawListContext(root_context, bounds);
            if (node != NULL && node->kind != ECS_UI_NODE_CUSTOM &&
                node->has_nine_slice_style) {
                if (options != NULL && options->nine_slice_draw != NULL) {
                    options->nine_slice_draw(
                        node,
                        &draw_context,
                        opacity,
                        options->user_data);
                }
                break;
            }
            if (node != NULL && node->kind == ECS_UI_NODE_ICON) {
                if (options != NULL && options->icon_draw != NULL) {
                    options->icon_draw(
                        node,
                        &draw_context,
                        opacity,
                        options->user_data);
                } else if (options != NULL &&
                    options->custom_draw != NULL) {
                    options->custom_draw(
                        node,
                        &draw_context,
                        opacity,
                        options->user_data);
                }
                break;
            }
            if (options != NULL && options->custom_draw != NULL) {
                options->custom_draw(
                    node,
                    &draw_context,
                    opacity,
                    options->user_data);
            } else {
                EcsUiRaylibDrawListRenderCustomFallback(
                    node,
                    bounds,
                    custom);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_NONE:
        default:
            break;
        }
    }
}

void EcsUiRaylibReleaseDrawListRenderer(void)
{
    free(g_ecs_ui_raylib_draw_list_text_buffer);
    g_ecs_ui_raylib_draw_list_text_buffer = NULL;
    g_ecs_ui_raylib_draw_list_text_buffer_len = 0u;
}
