#define CLAY_IMPLEMENTATION
#include "clay_raylib_bridge.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *g_clay_raylib_text_buffer = NULL;
static int g_clay_raylib_text_buffer_len = 0;

static const char *EcsUiClayRaylibCString(
    const char *chars,
    int32_t length)
{
    const int required = length + 1;
    if (required > g_clay_raylib_text_buffer_len) {
        char *next = realloc(g_clay_raylib_text_buffer, (size_t)required);
        if (next == NULL) {
            return "";
        }
        g_clay_raylib_text_buffer = next;
        g_clay_raylib_text_buffer_len = required;
    }

    if (length > 0 && chars != NULL) {
        memcpy(g_clay_raylib_text_buffer, chars, (size_t)length);
    }
    g_clay_raylib_text_buffer[length] = '\0';
    return g_clay_raylib_text_buffer;
}

static Font EcsUiClayRaylibFont(Font *fonts, uint16_t font_id)
{
    if (fonts == NULL || fonts[font_id].glyphs == NULL) {
        return GetFontDefault();
    }
    return fonts[font_id];
}

static Color EcsUiClayRaylibColor(Clay_Color color)
{
    return (Color){
        .r = (unsigned char)roundf(color.r),
        .g = (unsigned char)roundf(color.g),
        .b = (unsigned char)roundf(color.b),
        .a = (unsigned char)roundf(color.a),
    };
}

static Rectangle EcsUiClayRaylibRect(Clay_BoundingBox bounds)
{
    return (Rectangle){
        .x = roundf(bounds.x),
        .y = roundf(bounds.y),
        .width = roundf(bounds.width),
        .height = roundf(bounds.height),
    };
}

Clay_Dimensions EcsUiClayRaylibMeasureText(
    Clay_StringSlice text,
    Clay_TextElementConfig *config,
    void *user_data)
{
    Font *fonts = user_data;
    Font font = EcsUiClayRaylibFont(fonts, config != NULL ? config->fontId : 0u);
    const float font_size = config != NULL ? (float)config->fontSize : 18.0f;
    const float spacing = config != NULL ? (float)config->letterSpacing : 1.0f;
    const char *value = EcsUiClayRaylibCString(text.chars, text.length);
    Vector2 size = MeasureTextEx(font, value, font_size, spacing);
    return (Clay_Dimensions){
        .width = size.x,
        .height = size.y,
    };
}

static void EcsUiClayRaylibRenderRectangle(
    Rectangle bounds,
    Clay_RectangleRenderData *config)
{
    if (config == NULL) {
        return;
    }
    Color color = EcsUiClayRaylibColor(config->backgroundColor);
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

static void EcsUiClayRaylibRenderBorder(
    Rectangle bounds,
    Clay_BorderRenderData *config)
{
    if (config == NULL) {
        return;
    }
    Color color = EcsUiClayRaylibColor(config->color);
    if (config->width.left > 0u) {
        DrawRectangleRec(
            (Rectangle){bounds.x, bounds.y, (float)config->width.left, bounds.height},
            color);
    }
    if (config->width.right > 0u) {
        DrawRectangleRec(
            (Rectangle){
                bounds.x + bounds.width - (float)config->width.right,
                bounds.y,
                (float)config->width.right,
                bounds.height,
            },
            color);
    }
    if (config->width.top > 0u) {
        DrawRectangleRec(
            (Rectangle){bounds.x, bounds.y, bounds.width, (float)config->width.top},
            color);
    }
    if (config->width.bottom > 0u) {
        DrawRectangleRec(
            (Rectangle){
                bounds.x,
                bounds.y + bounds.height - (float)config->width.bottom,
                bounds.width,
                (float)config->width.bottom,
            },
            color);
    }
}

static void EcsUiClayRaylibRenderCustomFallback(
    const EcsUiTreeNodeSnapshot *node,
    Rectangle bounds,
    Clay_CustomRenderData *config)
{
    Color fill = EcsUiClayRaylibColor(config->backgroundColor);
    DrawRectangleRounded(bounds, 0.10f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.10f, 8, Fade(WHITE, 0.35f));
    if (node != NULL) {
        DrawTextEx(
            GetFontDefault(),
            node->custom.kind,
            (Vector2){bounds.x + 12.0f, bounds.y + 12.0f},
            13.0f,
            1.0f,
            Fade(WHITE, fill.a / 255.0f));
    }
}

void EcsUiClayRaylibRenderEx(
    Clay_RenderCommandArray render_commands,
    Font *fonts,
    const EcsUiClayRaylibRenderOptions *options)
{
    for (int32_t i = 0; i < render_commands.length; i += 1) {
        Clay_RenderCommand *command =
            Clay_RenderCommandArray_Get(&render_commands, i);
        Rectangle bounds = EcsUiClayRaylibRect(command->boundingBox);

        switch (command->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *text = &command->renderData.text;
            Font font = EcsUiClayRaylibFont(fonts, text->fontId);
            const char *value = EcsUiClayRaylibCString(
                text->stringContents.chars,
                text->stringContents.length);
            DrawTextEx(
                font,
                value,
                (Vector2){bounds.x, bounds.y},
                (float)text->fontSize,
                (float)text->letterSpacing,
                EcsUiClayRaylibColor(text->textColor));
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
                (Rectangle){0.0f, 0.0f, (float)texture.width, (float)texture.height},
                bounds,
                (Vector2){0.0f, 0.0f},
                0.0f,
                EcsUiClayRaylibColor(tint));
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
            EcsUiClayRaylibRenderRectangle(
                bounds,
                &command->renderData.rectangle);
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            EcsUiClayRaylibRenderBorder(bounds, &command->renderData.border);
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
            Clay_CustomRenderData *custom = &command->renderData.custom;
            const EcsUiTreeNodeSnapshot *node = custom->customData;
            const float opacity = custom->backgroundColor.a / 255.0f;
            if (node != NULL && node->kind == ECS_UI_NODE_ICON) {
                if (options != NULL && options->icon_draw != NULL) {
                    options->icon_draw(
                        node,
                        bounds,
                        opacity,
                        options->user_data);
                } else if (options != NULL && options->custom_draw != NULL) {
                    options->custom_draw(
                        node,
                        bounds,
                        opacity,
                        options->user_data);
                }
                break;
            }
            if (options != NULL && options->custom_draw != NULL) {
                options->custom_draw(
                    node,
                    bounds,
                    opacity,
                    options->user_data);
            } else {
                EcsUiClayRaylibRenderCustomFallback(node, bounds, custom);
            }
            break;
        }
        case CLAY_RENDER_COMMAND_TYPE_NONE:
        default:
            break;
        }
    }
}

void Clay_Raylib_Render(Clay_RenderCommandArray render_commands, Font *fonts)
{
    EcsUiClayRaylibRenderEx(render_commands, fonts, NULL);
}

void Clay_Raylib_Close(void)
{
    free(g_clay_raylib_text_buffer);
    g_clay_raylib_text_buffer = NULL;
    g_clay_raylib_text_buffer_len = 0;
    CloseWindow();
}
