#include "ecs_ui/ecs_ui.h"
#include "ecs_ui/ecs_ui_frame.h"
#include "../src/ecs_ui_frame_internal.h"

#include <stdio.h>

typedef struct CommandInventory {
    bool rectangle;
    bool border;
    bool text;
    bool custom;
    bool scissor_start;
    bool scissor_end;
    bool image;
} CommandInventory;

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static EcsUiSize TestMeasureText(
    const char *utf8,
    int32_t length,
    const EcsUiTextMeasureSpec *spec,
    void *user_data)
{
    (void)utf8;
    (void)user_data;
    const int32_t safe_length = length > 0 ? length : 0;
    const float font_size =
        spec != NULL && spec->font_size > 0.0f ? spec->font_size : 16.0f;
    return (EcsUiSize){
        .width = (float)safe_length * font_size * 0.5f,
        .height = font_size + 4.0f,
    };
}

static int BuildInventoryTree(ecs_world_t *world, ecs_entity_t root)
{
    int result = 0;
    result |= Require(
        EcsUiSetScale(world, root, 1.0f),
        "failed to set inventory scale");

    EcsUiBuilder builder = EcsUiBuilderBegin(world, root);
    ecs_entity_t box = EcsUiBeginVStack(
        &builder,
        (EcsUiStackDesc){
            .id = "InventoryBox",
            .preferred_width = 160.0f,
            .preferred_height = 120.0f,
            .padding = 4.0f,
            .gap = 3.0f,
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "InventoryText",
            .text = "image inventory",
            .role = ECS_UI_TEXT_BODY,
        });
    (void)EcsUiAddCustom(
        &builder,
        (EcsUiCustomDesc){
            .id = "InventoryCustom",
            .kind = "inventory.custom",
            .preferred_width = 32.0f,
            .preferred_height = 18.0f,
        });
    (void)EcsUiBeginVScrollView(
        &builder,
        (EcsUiScrollViewDesc){
            .stack = {
                .id = "InventoryScroll",
                .preferred_width = 64.0f,
                .preferred_height = 24.0f,
            },
        });
    (void)EcsUiAddText(
        &builder,
        (EcsUiTextDesc){
            .id = "InventoryRow",
            .text = "row",
            .role = ECS_UI_TEXT_BODY,
        });
    EcsUiEnd(&builder);
    EcsUiEnd(&builder);
    EcsUiBuilderEnd(&builder);
    result |= Require(EcsUiBuilderOk(&builder), "inventory builder failed");

    ecs_set(world, box, EcsUiBoxStyle, {
        .background = {24u, 32u, 40u, 255u},
        .border_color = {200u, 210u, 220u, 255u},
        .border_width = 1.0f,
        .radius = 0.25f,
    });
    return result;
}

static void ReadInventory(
    const Clay_RenderCommandArray *commands,
    CommandInventory *inventory)
{
    if (commands == NULL || inventory == NULL) {
        return;
    }

    for (int32_t i = 0; i < commands->length; i += 1) {
        Clay_RenderCommand *command =
            Clay_RenderCommandArray_Get((Clay_RenderCommandArray *)commands, i);
        if (command == NULL) {
            continue;
        }
        switch (command->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            inventory->rectangle = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER:
            inventory->border = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT:
            inventory->text = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            inventory->custom = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            inventory->scissor_start = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            inventory->scissor_end = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            inventory->image = true;
            break;
        case CLAY_RENDER_COMMAND_TYPE_NONE:
        default:
            break;
        }
    }
}

int main(void)
{
    int result = 0;
    ecs_world_t *world = ecs_init();
    result |= Require(world != NULL, "failed to create world");
    if (world == NULL) {
        return result;
    }
    EcsUiImport(world);

    ecs_entity_t root = EcsUiRootEntity(world, "InventoryRoot");
    result |= BuildInventoryTree(world, root);
    EcsUiTreeSnapshot tree = {0};
    result |= Require(EcsUiReadTree(world, root, &tree), "inventory read failed");

    result |= Require(
        EcsUiFrameBackendInit(
            &(EcsUiFrameBackendDesc){
                .surface_width = 320.0f,
                .surface_height = 240.0f,
                .measure_text = TestMeasureText,
            }),
        "failed to initialize frame backend");
    EcsUiFrameBackendSetCullingEnabled(false);

    EcsUiTheme theme = EcsUiThemeDefault();
    const EcsUiDrawList *draw_list = EcsUiFrameRun(
        &tree,
        &theme,
        &(EcsUiFrameLayoutOptions){
            .physical_bounds = {
                .width = 320.0f,
                .height = 240.0f,
            },
        },
        NULL,
        NULL);
    result |= Require(draw_list != NULL, "inventory frame failed");

    CommandInventory inventory = {0};
    ReadInventory(EcsUiFrameDrawListClayCommands(draw_list), &inventory);
    result |= Require(inventory.rectangle, "inventory missing rectangle command");
    result |= Require(inventory.border, "inventory missing border command");
    result |= Require(inventory.text, "inventory missing text command");
    result |= Require(inventory.custom, "inventory missing custom command");
    result |= Require(
        inventory.scissor_start && inventory.scissor_end,
        "inventory missing scissor commands");
    result |= Require(
        !inventory.image,
        "raw Clay IMAGE commands are not part of the current ecs-ui inventory");

    EcsUiFrameBackendShutdown();
    ecs_fini(world);
    return result;
}
