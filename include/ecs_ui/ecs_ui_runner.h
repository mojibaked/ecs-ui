#ifndef ECS_UI_ECS_UI_RUNNER_H
#define ECS_UI_ECS_UI_RUNNER_H

#include "ecs_ui/ecs_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ECS_UI_WAKE_SOURCE_MAX 64u
#define ECS_UI_WAKE_FD_MAX ECS_UI_WAKE_SOURCE_MAX
#define ECS_UI_FRAME_SIGNAL_MAX 64u

typedef enum EcsUiWakeSourceKind {
    ECS_UI_WAKE_SOURCE_NONE = 0,
    ECS_UI_WAKE_SOURCE_PENDING = 1,
    ECS_UI_WAKE_SOURCE_DEADLINE = 2,
    ECS_UI_WAKE_SOURCE_POSIX_FD = 3,
} EcsUiWakeSourceKind;

typedef enum EcsUiWakeFdInterestFlags {
    ECS_UI_WAKE_FD_INTEREST_NONE = 0u,
    ECS_UI_WAKE_FD_INTEREST_READ = 1u << 0u,
    ECS_UI_WAKE_FD_INTEREST_WRITE = 1u << 1u,
} EcsUiWakeFdInterestFlags;

typedef struct EcsUiWakeHandle {
    uint32_t index;
    uint32_t generation;
} EcsUiWakeHandle;

typedef struct EcsUiFrameSignalHandle {
    uint32_t index;
    uint32_t generation;
} EcsUiFrameSignalHandle;

typedef enum EcsUiFrameMarkFlags {
    ECS_UI_FRAME_MARK_NONE = 0u,
    ECS_UI_FRAME_MARK_INPUT = 1u << 0u,
    ECS_UI_FRAME_MARK_WINDOW_EVENT = 1u << 1u,
    ECS_UI_FRAME_MARK_FORCE_RENDER = 1u << 2u,
    ECS_UI_FRAME_MARK_SCREENSHOT_REQUEST = 1u << 3u,
} EcsUiFrameMarkFlags;

typedef enum EcsUiFrameClassification {
    ECS_UI_FRAME_PARK = 0,
    ECS_UI_FRAME_PRESENT_ONLY = 1,
    ECS_UI_FRAME_RENDER_AND_PRESENT = 2,
} EcsUiFrameClassification;

typedef enum EcsUiFrameReasonKind {
    ECS_UI_FRAME_REASON_NONE = 0,
    ECS_UI_FRAME_REASON_FIRST_FRAME = 1,
    ECS_UI_FRAME_REASON_STABLE_REVISION = 2,
    ECS_UI_FRAME_REASON_INPUT = 3,
    ECS_UI_FRAME_REASON_WINDOW_EVENT = 4,
    ECS_UI_FRAME_REASON_FORCE_RENDER = 5,
    ECS_UI_FRAME_REASON_SCREENSHOT_REQUEST = 6,
    ECS_UI_FRAME_REASON_WAKE_HOT = 7,
    ECS_UI_FRAME_REASON_PRESENT_POLICY = 8,
} EcsUiFrameReasonKind;

typedef struct EcsUiWakeSourceInfo {
    EcsUiWakeHandle handle;
    EcsUiWakeSourceKind kind;
    int fd;
    uint32_t fd_interests;
    uint64_t deadline_ns;
    char label[ECS_UI_ID_MAX];
} EcsUiWakeSourceInfo;

typedef struct EcsUiWakeFdWait {
    EcsUiWakeHandle handle;
    int fd;
    uint32_t interests;
    char label[ECS_UI_ID_MAX];
} EcsUiWakeFdWait;

typedef struct EcsUiFrameReason {
    EcsUiFrameReasonKind kind;
    EcsUiFrameSignalHandle signal;
    EcsUiWakeHandle wake;
    uint64_t stable_id;
    uint64_t previous_revision;
    uint64_t revision;
    char label[ECS_UI_ID_MAX];
} EcsUiFrameReason;

/*
 * Backend-neutral park input. `hot` means a source is ready now and the host
 * should not block. `deadline_ns` is an absolute monotonic timestamp supplied
 * by the caller's clock domain. POSIX fd sources are reported with interest
 * masks, but the registry does not poll or wait on them.
 */
typedef struct EcsUiWakeWaitSpec {
    bool hot;
    bool has_deadline;
    uint64_t deadline_ns;
    EcsUiWakeHandle deadline_handle;
    char deadline_label[ECS_UI_ID_MAX];
    EcsUiWakeSourceInfo active[ECS_UI_WAKE_SOURCE_MAX];
    uint32_t active_count;
    bool active_truncated;
    EcsUiWakeFdWait fds[ECS_UI_WAKE_FD_MAX];
    uint32_t fd_count;
    bool fd_truncated;
} EcsUiWakeWaitSpec;

typedef struct EcsUiFrameClassifyDesc {
    const EcsUiWakeWaitSpec *wake;
    bool present_when_clean;
    const char *present_policy_label;
} EcsUiFrameClassifyDesc;

typedef struct EcsUiFrameClassifyResult {
    EcsUiFrameClassification classification;
    EcsUiFrameReason reason;
    bool should_render;
    bool should_present;
    bool should_park;
    bool screenshot_requested;
} EcsUiFrameClassifyResult;

typedef struct EcsUiWakeRegistry EcsUiWakeRegistry;
typedef struct EcsUiFrameSignalAccumulator EcsUiFrameSignalAccumulator;

bool EcsUiWakeHandleIsValid(EcsUiWakeHandle handle);
EcsUiWakeRegistry *EcsUiWakeRegistryCreate(void);
void EcsUiWakeRegistryDestroy(EcsUiWakeRegistry *registry);

EcsUiWakeHandle EcsUiWakeRegisterPending(
    EcsUiWakeRegistry *registry,
    const char *label);
EcsUiWakeHandle EcsUiWakeRegisterDeadline(
    EcsUiWakeRegistry *registry,
    const char *label);
EcsUiWakeHandle EcsUiWakeRegisterPosixFd(
    EcsUiWakeRegistry *registry,
    const char *label,
    int fd,
    uint32_t interests);
bool EcsUiWakeRemove(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle);

bool EcsUiWakeSetPending(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    bool pending);
/*
 * Threading contract: registration and source mutation are loop-thread-only;
 * posting is the only any-thread operation. Multiple posts to the same source
 * coalesce until `EcsUiWakeConsumePosts` is called on the loop thread.
 */
bool EcsUiWakePost(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle);
uint32_t EcsUiWakeConsumePosts(EcsUiWakeRegistry *registry);

bool EcsUiWakeArmDeadline(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    uint64_t deadline_ns);
bool EcsUiWakeDisarmDeadline(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle);

bool EcsUiWakeSetFdInterests(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    uint32_t interests);
bool EcsUiWakeSetPosixFd(
    EcsUiWakeRegistry *registry,
    EcsUiWakeHandle handle,
    int fd,
    uint32_t interests);

bool EcsUiWakeBuildWaitSpec(
    const EcsUiWakeRegistry *registry,
    uint64_t now_ns,
    EcsUiWakeWaitSpec *out);

bool EcsUiFrameSignalHandleIsValid(EcsUiFrameSignalHandle handle);
EcsUiFrameSignalAccumulator *EcsUiFrameSignalAccumulatorCreate(void);
void EcsUiFrameSignalAccumulatorDestroy(
    EcsUiFrameSignalAccumulator *accumulator);
EcsUiFrameSignalHandle EcsUiFrameSignalRegisterStable(
    EcsUiFrameSignalAccumulator *accumulator,
    uint64_t id,
    const char *label,
    uint64_t revision);
bool EcsUiFrameSignalSetRevision(
    EcsUiFrameSignalAccumulator *accumulator,
    EcsUiFrameSignalHandle handle,
    uint64_t revision);
bool EcsUiFrameSignalRemove(
    EcsUiFrameSignalAccumulator *accumulator,
    EcsUiFrameSignalHandle handle);
bool EcsUiFrameSignalMark(
    EcsUiFrameSignalAccumulator *accumulator,
    uint32_t marks);
bool EcsUiFrameSignalClearMarks(EcsUiFrameSignalAccumulator *accumulator);
bool EcsUiFrameClassify(
    EcsUiFrameSignalAccumulator *accumulator,
    const EcsUiFrameClassifyDesc *desc,
    EcsUiFrameClassifyResult *out);

#ifdef __cplusplus
}
#endif

#endif
