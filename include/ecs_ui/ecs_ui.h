#ifndef ECS_UI_ECS_UI_H
#define ECS_UI_ECS_UI_H

#include <stdbool.h>
#include <stdint.h>

#include <flecs.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_ID_MAX 64u
#define ECS_UI_TEXT_MAX 256u
#define ECS_UI_BUILDER_STACK_MAX 64u
#define ECS_UI_TREE_NODE_MAX 1024u
#define ECS_UI_EVENT_MAX 64u
#define ECS_UI_TREE_INVALID_INDEX UINT32_MAX

typedef enum EcsUiNodeKind {
    ECS_UI_NODE_NONE = 0,
    ECS_UI_NODE_ROOT = 1,
    ECS_UI_NODE_VSTACK = 2,
    ECS_UI_NODE_HSTACK = 3,
    ECS_UI_NODE_ZSTACK = 4,
    ECS_UI_NODE_BUTTON = 5,
    ECS_UI_NODE_TEXT = 6,
    ECS_UI_NODE_ICON = 7,
    ECS_UI_NODE_CUSTOM = 8,
    ECS_UI_NODE_PRESSABLE = 9,
} EcsUiNodeKind;

typedef enum EcsUiAxis {
    ECS_UI_AXIS_VERTICAL = 0,
    ECS_UI_AXIS_HORIZONTAL = 1,
    ECS_UI_AXIS_DEPTH = 2,
} EcsUiAxis;

typedef enum EcsUiAlign {
    ECS_UI_ALIGN_START = 0,
    ECS_UI_ALIGN_CENTER = 1,
    ECS_UI_ALIGN_END = 2,
} EcsUiAlign;

typedef enum EcsUiSizing {
    ECS_UI_SIZE_AUTO = 0,
    ECS_UI_SIZE_GROW = 1,
    ECS_UI_SIZE_FIT = 2,
} EcsUiSizing;

typedef enum EcsUiButtonVariant {
    ECS_UI_BUTTON_DEFAULT = 0,
    ECS_UI_BUTTON_PRIMARY = 1,
    ECS_UI_BUTTON_SUBTLE = 2,
    ECS_UI_BUTTON_DANGER = 3,
} EcsUiButtonVariant;

typedef enum EcsUiBevel {
    ECS_UI_BEVEL_NONE = 0,
    ECS_UI_BEVEL_RAISED = 1,
    ECS_UI_BEVEL_SUNKEN = 2,
} EcsUiBevel;

typedef enum EcsUiTextRole {
    ECS_UI_TEXT_BODY = 0,
    ECS_UI_TEXT_TITLE = 1,
    ECS_UI_TEXT_LABEL = 2,
    ECS_UI_TEXT_BUTTON = 3,
    ECS_UI_TEXT_CAPTION = 4,
} EcsUiTextRole;

typedef enum EcsUiEventType {
    ECS_UI_EVENT_NONE = 0,
    ECS_UI_EVENT_HOVERED = 1,
    ECS_UI_EVENT_PRESSED = 2,
    ECS_UI_EVENT_CLICKED = 3,
    ECS_UI_EVENT_DRAG_STARTED = 4,
    ECS_UI_EVENT_DRAGGED = 5,
    ECS_UI_EVENT_DRAG_ENDED = 6,
    ECS_UI_EVENT_TEXT_INPUT = 7,
    ECS_UI_EVENT_TEXT_DELETE = 8,
    ECS_UI_EVENT_TEXT_SUBMIT = 9,
    ECS_UI_EVENT_TEXT_CANCEL = 10,
    ECS_UI_EVENT_TEXT_FOCUS_NEXT = 11,
    ECS_UI_EVENT_TEXT_FOCUS_PREVIOUS = 12,
    ECS_UI_EVENT_TEXT_CURSOR_LEFT = 13,
    ECS_UI_EVENT_TEXT_CURSOR_RIGHT = 14,
    ECS_UI_EVENT_TEXT_CURSOR_START = 15,
    ECS_UI_EVENT_TEXT_CURSOR_END = 16,
    ECS_UI_EVENT_TEXT_SELECT_LEFT = 17,
    ECS_UI_EVENT_TEXT_SELECT_RIGHT = 18,
    ECS_UI_EVENT_TEXT_SELECT_START = 19,
    ECS_UI_EVENT_TEXT_SELECT_END = 20,
    ECS_UI_EVENT_TEXT_COPY = 21,
    ECS_UI_EVENT_TEXT_CUT = 22,
    ECS_UI_EVENT_TEXT_PASTE = 23,
    ECS_UI_EVENT_SECONDARY_PRESSED = 24,
} EcsUiEventType;

typedef enum EcsUiPointerButton {
    ECS_UI_POINTER_BUTTON_PRIMARY = 0,
    ECS_UI_POINTER_BUTTON_SECONDARY = 1,
} EcsUiPointerButton;

typedef enum EcsUiHitTestMode {
    ECS_UI_HIT_TEST_AUTO = 0,
    ECS_UI_HIT_TEST_NONE = 1,
    ECS_UI_HIT_TEST_CHILDREN = 2,
    ECS_UI_HIT_TEST_CAPTURE = 3,
} EcsUiHitTestMode;

typedef enum EcsUiScrollAxisFlags {
    ECS_UI_SCROLL_AXIS_NONE = 0u,
    ECS_UI_SCROLL_AXIS_X = 1u << 0u,
    ECS_UI_SCROLL_AXIS_Y = 1u << 1u,
    ECS_UI_SCROLL_AXIS_BOTH =
        ECS_UI_SCROLL_AXIS_X | ECS_UI_SCROLL_AXIS_Y,
} EcsUiScrollAxisFlags;

typedef struct EcsUiNodeId {
    char value[ECS_UI_ID_MAX];
} EcsUiNodeId;

typedef struct EcsUiNode {
    EcsUiNodeKind kind;
} EcsUiNode;

typedef struct EcsUiStack {
    EcsUiAxis axis;
    float gap;
    float padding;
    float preferred_width;
    float preferred_height;
    EcsUiAlign align_x;
    EcsUiAlign align_y;
    EcsUiSizing width_sizing;
    EcsUiSizing height_sizing;
} EcsUiStack;

typedef struct EcsUiColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} EcsUiColor;

typedef struct EcsUiTheme {
    EcsUiColor root_background;
    EcsUiColor surface;
    EcsUiColor surface_subtle;
    EcsUiColor button;
    EcsUiColor button_primary;
    EcsUiColor button_subtle;
    EcsUiColor button_danger;
    EcsUiColor button_disabled;
    EcsUiColor text;
    EcsUiColor text_muted;
    EcsUiColor text_inverse;
    float radius;
} EcsUiTheme;

typedef struct EcsUiBoxStyle {
    EcsUiColor background;
    EcsUiColor hover_background;
    EcsUiColor disabled_background;
    EcsUiColor highlight_background;
    float radius;
    float padding;
    EcsUiColor border_color;
    float border_width;
    EcsUiBevel bevel;
    EcsUiColor bevel_light;
    EcsUiColor bevel_dark;
} EcsUiBoxStyle;

typedef struct EcsUiTextStyle {
    EcsUiColor color;
    EcsUiColor muted_color;
    EcsUiColor disabled_color;
} EcsUiTextStyle;

typedef struct EcsUiTextLayout {
    EcsUiAlign align_x;
    EcsUiAlign align_y;
} EcsUiTextLayout;

typedef struct EcsUiButton {
    EcsUiButtonVariant variant;
    bool disabled;
} EcsUiButton;

typedef struct EcsUiPressable {
    float preferred_height;
    bool disabled;
} EcsUiPressable;

typedef struct EcsUiText {
    char text[ECS_UI_TEXT_MAX];
    EcsUiTextRole role;
} EcsUiText;

typedef struct EcsUiIcon {
    char name[ECS_UI_ID_MAX];
} EcsUiIcon;

typedef struct EcsUiCustom {
    char kind[ECS_UI_ID_MAX];
    float preferred_width;
    float preferred_height;
    EcsUiSizing width_sizing;
    EcsUiSizing height_sizing;
} EcsUiCustom;

typedef struct EcsUiVisual {
    float opacity;
    float offset_x;
    float offset_y;
    float highlight;
} EcsUiVisual;

typedef struct EcsUiPlacement {
    EcsUiAlign parent_x;
    EcsUiAlign parent_y;
    EcsUiAlign child_x;
    EcsUiAlign child_y;
    float offset_x;
    float offset_y;
    float width;
    float height;
} EcsUiPlacement;

typedef struct EcsUiHitTest {
    EcsUiHitTestMode mode;
} EcsUiHitTest;

typedef struct EcsUiScrollView {
    uint32_t axes;
} EcsUiScrollView;

typedef struct EcsUiTextFieldView {
    ecs_entity_t value_node;
    uint32_t cursor;
    uint32_t selection_anchor;
    uint32_t selection_focus;
    float caret_width;
    bool focused;
    bool disabled;
} EcsUiTextFieldView;

typedef struct EcsUiStackDesc {
    const char *id;
    float gap;
    float padding;
    float preferred_width;
    float preferred_height;
    EcsUiAlign align_x;
    EcsUiAlign align_y;
    EcsUiSizing width_sizing;
    EcsUiSizing height_sizing;
    ecs_entity_t style_token;
    const EcsUiBoxStyle *style;
} EcsUiStackDesc;

typedef struct EcsUiScrollViewDesc {
    EcsUiStackDesc stack;
    uint32_t axes;
} EcsUiScrollViewDesc;

typedef struct EcsUiButtonDesc {
    const char *id;
    EcsUiButtonVariant variant;
    ecs_entity_t on_click;
    bool disabled;
    ecs_entity_t style_token;
} EcsUiButtonDesc;

typedef struct EcsUiPressableDesc {
    const char *id;
    ecs_entity_t on_click;
    bool disabled;
    ecs_entity_t style_token;
    float preferred_height;
} EcsUiPressableDesc;

typedef struct EcsUiTextDesc {
    const char *id;
    const char *text;
    EcsUiTextRole role;
} EcsUiTextDesc;

typedef struct EcsUiIconDesc {
    const char *id;
    const char *name;
} EcsUiIconDesc;

typedef struct EcsUiCustomDesc {
    const char *id;
    const char *kind;
    float preferred_width;
    float preferred_height;
    ecs_entity_t on_click;
    EcsUiSizing width_sizing;
    EcsUiSizing height_sizing;
} EcsUiCustomDesc;

typedef struct EcsUiBuilder {
    ecs_world_t *world;
    ecs_entity_t root;
    ecs_entity_t parent_stack[ECS_UI_BUILDER_STACK_MAX];
    uint32_t depth;
    bool failed;
} EcsUiBuilder;

typedef struct EcsUiTreeNodeSnapshot {
    ecs_entity_t entity;
    ecs_entity_t parent;
    ecs_entity_t on_click;
    char id[ECS_UI_ID_MAX];
    EcsUiNodeKind kind;
    uint32_t depth;
    uint32_t parent_index;
    uint32_t first_child;
    uint32_t next_sibling;
    EcsUiStack stack;
    EcsUiBoxStyle box_style;
    EcsUiTextStyle text_style;
    EcsUiTextLayout text_layout;
    EcsUiButton button;
    EcsUiPressable pressable;
    EcsUiText text;
    EcsUiIcon icon;
    EcsUiCustom custom;
    EcsUiVisual visual;
    EcsUiPlacement placement;
    EcsUiHitTest hit_test;
    EcsUiScrollView scroll_view;
    EcsUiTextFieldView text_field_view;
    bool has_box_style;
    bool has_text_style;
    bool has_text_layout;
    bool has_placement;
    bool has_scroll_view;
    bool has_text_field_view;
    bool hovered;
    bool hover_within;
} EcsUiTreeNodeSnapshot;

typedef struct EcsUiTreeSnapshot {
    ecs_entity_t root;
    uint32_t count;
    bool truncated;
    EcsUiTreeNodeSnapshot nodes[ECS_UI_TREE_NODE_MAX];
} EcsUiTreeSnapshot;

typedef struct EcsUiEvent {
    EcsUiEventType type;
    ecs_entity_t tree;
    ecs_entity_t node;
    ecs_entity_t action;
    char node_id[ECS_UI_ID_MAX];
    float x;
    float y;
    float start_x;
    float start_y;
    float delta_x;
    float delta_y;
    float elapsed;
    float velocity_x;
    float velocity_y;
    EcsUiPointerButton button;
    uint32_t codepoint;
    char text[ECS_UI_TEXT_MAX];
} EcsUiEvent;

typedef struct EcsUiEventList {
    uint32_t count;
    bool truncated;
    EcsUiEvent events[ECS_UI_EVENT_MAX];
} EcsUiEventList;

extern ECS_COMPONENT_DECLARE(EcsUiNodeId);
extern ECS_COMPONENT_DECLARE(EcsUiNode);
extern ECS_COMPONENT_DECLARE(EcsUiStack);
extern ECS_COMPONENT_DECLARE(EcsUiBoxStyle);
extern ECS_COMPONENT_DECLARE(EcsUiTextStyle);
extern ECS_COMPONENT_DECLARE(EcsUiTextLayout);
extern ECS_COMPONENT_DECLARE(EcsUiButton);
extern ECS_COMPONENT_DECLARE(EcsUiPressable);
extern ECS_COMPONENT_DECLARE(EcsUiText);
extern ECS_COMPONENT_DECLARE(EcsUiIcon);
extern ECS_COMPONENT_DECLARE(EcsUiCustom);
extern ECS_COMPONENT_DECLARE(EcsUiVisual);
extern ECS_COMPONENT_DECLARE(EcsUiPlacement);
extern ECS_COMPONENT_DECLARE(EcsUiHitTest);
extern ECS_COMPONENT_DECLARE(EcsUiScrollView);
extern ECS_COMPONENT_DECLARE(EcsUiTextFieldView);

extern ECS_TAG_DECLARE(EcsUiRoot);
extern ECS_TAG_DECLARE(EcsUiInteractive);
extern ECS_TAG_DECLARE(EcsUiOnClick);
extern ECS_TAG_DECLARE(EcsUiUsesStyle);
extern ECS_TAG_DECLARE(EcsUiHovered);
extern ECS_TAG_DECLARE(EcsUiHoverWithin);
extern ECS_TAG_DECLARE(EcsUiRevealedByHover);
extern ECS_TAG_DECLARE(EcsUiThemeTag);
extern ECS_TAG_DECLARE(EcsUiActiveTheme);
extern ECS_TAG_DECLARE(EcsUiThemeStyle);

void EcsUiImport(ecs_world_t *world);
EcsUiTheme EcsUiThemeDefault(void);

ecs_entity_t EcsUiStyleTokenRoot(ecs_world_t *world);
ecs_entity_t EcsUiStyleToken(ecs_world_t *world, const char *id);
bool EcsUiSetStyleToken(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t style_token);
bool EcsUiSetRevealOnHover(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t trigger);
bool EcsUiClearRevealOnHover(ecs_world_t *world, ecs_entity_t entity);
bool EcsUiApplyHoverState(ecs_world_t *world, ecs_entity_t hovered_node);
ecs_entity_t EcsUiCurrentHoveredNode(const ecs_world_t *world);

ecs_entity_t EcsUiThemeRoot(ecs_world_t *world);
ecs_entity_t EcsUiThemeEntity(ecs_world_t *world, const char *id);
bool EcsUiSetActiveTheme(ecs_world_t *world, ecs_entity_t theme);
ecs_entity_t EcsUiGetActiveTheme(const ecs_world_t *world);
bool EcsUiThemeSetBoxStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiBoxStyle style);
bool EcsUiThemeSetTextStyle(
    ecs_world_t *world,
    ecs_entity_t theme,
    ecs_entity_t style_token,
    EcsUiTextStyle style);
bool EcsUiThemeApply(ecs_world_t *world);

ecs_entity_t EcsUiRootEntity(ecs_world_t *world, const char *id);

EcsUiBuilder EcsUiBuilderBegin(ecs_world_t *world, ecs_entity_t root);
void EcsUiBuilderEnd(EcsUiBuilder *builder);
bool EcsUiBuilderOk(const EcsUiBuilder *builder);

ecs_entity_t EcsUiBeginVStack(EcsUiBuilder *builder, EcsUiStackDesc desc);
ecs_entity_t EcsUiBeginHStack(EcsUiBuilder *builder, EcsUiStackDesc desc);
ecs_entity_t EcsUiBeginZStack(EcsUiBuilder *builder, EcsUiStackDesc desc);
ecs_entity_t EcsUiBeginVScrollView(
    EcsUiBuilder *builder,
    EcsUiScrollViewDesc desc);
ecs_entity_t EcsUiBeginHScrollView(
    EcsUiBuilder *builder,
    EcsUiScrollViewDesc desc);
ecs_entity_t EcsUiBeginButton(EcsUiBuilder *builder, EcsUiButtonDesc desc);
ecs_entity_t EcsUiBeginPressable(
    EcsUiBuilder *builder,
    EcsUiPressableDesc desc);
void EcsUiEnd(EcsUiBuilder *builder);

bool EcsUiSetScrollView(
    ecs_world_t *world,
    ecs_entity_t entity,
    EcsUiScrollView scroll_view);
bool EcsUiClearScrollView(ecs_world_t *world, ecs_entity_t entity);

ecs_entity_t EcsUiAddText(EcsUiBuilder *builder, EcsUiTextDesc desc);
ecs_entity_t EcsUiAddIcon(EcsUiBuilder *builder, EcsUiIconDesc desc);
ecs_entity_t EcsUiAddCustom(EcsUiBuilder *builder, EcsUiCustomDesc desc);

bool EcsUiReadTree(
    const ecs_world_t *world,
    ecs_entity_t root,
    EcsUiTreeSnapshot *out);

void EcsUiEventListClear(EcsUiEventList *events);
bool EcsUiEventListPush(EcsUiEventList *events, const EcsUiEvent *event);

#define ECS_UI_JOIN_INNER(a, b) a##b
#define ECS_UI_JOIN(a, b) ECS_UI_JOIN_INNER(a, b)

#define EcsUiScopeAt(open_expr, close_expr, line) \
    for (int ECS_UI_JOIN(ecs_ui_scope_once_, line) = ((open_expr), 0); \
         ECS_UI_JOIN(ecs_ui_scope_once_, line) < 1; \
         ECS_UI_JOIN(ecs_ui_scope_once_, line) = 1, (close_expr))

#define EcsUiScope(open_expr, close_expr) \
    EcsUiScopeAt(open_expr, close_expr, __LINE__)

#define EcsUiVStack(builder, ...) \
    EcsUiScope( \
        EcsUiBeginVStack((builder), (EcsUiStackDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiHStack(builder, ...) \
    EcsUiScope( \
        EcsUiBeginHStack((builder), (EcsUiStackDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiZStack(builder, ...) \
    EcsUiScope( \
        EcsUiBeginZStack((builder), (EcsUiStackDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiVScrollView(builder, ...) \
    EcsUiScope( \
        EcsUiBeginVScrollView((builder), (EcsUiScrollViewDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiHScrollView(builder, ...) \
    EcsUiScope( \
        EcsUiBeginHScrollView((builder), (EcsUiScrollViewDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiButtonScope(builder, ...) \
    EcsUiScope( \
        EcsUiBeginButton((builder), (EcsUiButtonDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiPressableScope(builder, ...) \
    EcsUiScope( \
        EcsUiBeginPressable((builder), (EcsUiPressableDesc)__VA_ARGS__), \
        EcsUiEnd((builder)))

#define EcsUiTextNode(builder, ...) \
    EcsUiAddText((builder), (EcsUiTextDesc)__VA_ARGS__)

#define EcsUiIconNode(builder, ...) \
    EcsUiAddIcon((builder), (EcsUiIconDesc)__VA_ARGS__)

#define EcsUiCustomNode(builder, ...) \
    EcsUiAddCustom((builder), (EcsUiCustomDesc)__VA_ARGS__)

#ifndef ECS_UI_NO_SHORT_NAMES
#define VStack EcsUiVStack
#define HStack EcsUiHStack
#define ZStack EcsUiZStack
#define VScrollView EcsUiVScrollView
#define HScrollView EcsUiHScrollView
#define Button EcsUiButtonScope
#define Pressable EcsUiPressableScope
#define Text EcsUiTextNode
#define Icon EcsUiIconNode
#define Custom EcsUiCustomNode
#endif

#ifdef __cplusplus
}
#endif

#endif
