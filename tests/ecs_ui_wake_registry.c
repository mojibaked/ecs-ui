#include "ecs_ui/ecs_ui_animation.h"
#include "ecs_ui/ecs_ui_runner.h"

#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#endif

static int Require(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "%s\n", message);
        return 1;
    }
    return 0;
}

static bool HandleEquals(EcsUiWakeHandle a, EcsUiWakeHandle b)
{
    return a.index == b.index && a.generation == b.generation;
}

static bool ActiveLabelIs(
    const EcsUiWakeWaitSpec *spec,
    uint32_t index,
    const char *label)
{
    return spec != NULL && index < spec->active_count &&
        strcmp(spec->active[index].label, label) == 0;
}

static int TestPendingPostCoalescing(void)
{
    int result = 0;
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeWaitSpec spec = {0};
    EcsUiWakeHandle pending =
        EcsUiWakeRegisterPending(registry, "jobs");

    result |= Require(registry != NULL, "pending registry missing");
    result |= Require(
        EcsUiWakeHandleIsValid(pending),
        "pending handle should be valid");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 10u, &spec),
        "pending empty spec failed");
    result |= Require(!spec.hot, "empty pending source should not be hot");

    result |= Require(
        EcsUiWakeSetPending(registry, pending, true),
        "set pending true failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 10u, &spec),
        "manual pending spec failed");
    result |= Require(spec.hot, "manual pending should be hot");
    result |= Require(
        spec.active_count == 1u && ActiveLabelIs(&spec, 0u, "jobs"),
        "manual pending attribution mismatch");
    result |= Require(
        EcsUiWakeConsumePosts(registry) == 0u,
        "manual pending should not consume posts");
    result |= Require(
        EcsUiWakeSetPending(registry, pending, false),
        "set pending false failed");

    result |= Require(EcsUiWakePost(registry, pending), "first post failed");
    result |= Require(EcsUiWakePost(registry, pending), "second post failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 10u, &spec),
        "posted pending spec failed");
    result |= Require(spec.hot, "posted pending should be hot");
    result |= Require(
        spec.active_count == 1u && ActiveLabelIs(&spec, 0u, "jobs"),
        "posted pending should coalesce into one attribution");
    result |= Require(
        EcsUiWakeConsumePosts(registry) == 1u,
        "coalesced posts should consume once");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 10u, &spec),
        "post-consume spec failed");
    result |= Require(!spec.hot, "consumed post should not stay hot");
    result |= Require(
        EcsUiWakeConsumePosts(registry) == 0u,
        "post consume should be one-shot");

    EcsUiWakeRegistryDestroy(registry);
    return result;
}

static int TestDeadlineOrdering(void)
{
    int result = 0;
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle slow =
        EcsUiWakeRegisterDeadline(registry, "slow");
    EcsUiWakeHandle soon =
        EcsUiWakeRegisterDeadline(registry, "soon");
    EcsUiWakeWaitSpec spec = {0};

    result |= Require(
        EcsUiWakeArmDeadline(registry, slow, 1000u),
        "arm slow deadline failed");
    result |= Require(
        EcsUiWakeArmDeadline(registry, soon, 500u),
        "arm soon deadline failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 100u, &spec),
        "future deadline spec failed");
    result |= Require(!spec.hot, "future deadlines should not be hot");
    result |= Require(
        spec.has_deadline && spec.deadline_ns == 500u &&
            HandleEquals(spec.deadline_handle, soon) &&
            strcmp(spec.deadline_label, "soon") == 0,
        "nearest deadline should win");

    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 500u, &spec),
        "due deadline spec failed");
    result |= Require(spec.hot, "due deadline should be hot");
    result |= Require(
        spec.active_count == 1u &&
            spec.active[0].kind == ECS_UI_WAKE_SOURCE_DEADLINE &&
            ActiveLabelIs(&spec, 0u, "soon"),
        "due deadline attribution mismatch");
    result |= Require(
        spec.has_deadline && spec.deadline_ns == 1000u &&
            strcmp(spec.deadline_label, "slow") == 0,
        "future deadline should remain available while another is hot");
    result |= Require(
        EcsUiWakeDisarmDeadline(registry, soon),
        "disarm soon failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 600u, &spec),
        "post-disarm deadline spec failed");
    result |= Require(
        !spec.hot && spec.has_deadline && spec.deadline_ns == 1000u,
        "disarmed deadline should not stay active");

    EcsUiWakeRegistryDestroy(registry);
    return result;
}

#if !defined(_WIN32)
static short PollEventsForInterests(uint32_t interests)
{
    short events = 0;
    if ((interests & ECS_UI_WAKE_FD_INTEREST_READ) != 0u) {
        events = (short)(events | POLLIN);
    }
    if ((interests & ECS_UI_WAKE_FD_INTEREST_WRITE) != 0u) {
        events = (short)(events | POLLOUT);
    }
    return events;
}

static uint32_t InterestsForPollRevents(short revents)
{
    uint32_t interests = ECS_UI_WAKE_FD_INTEREST_NONE;
    if ((revents & POLLIN) != 0) {
        interests |= ECS_UI_WAKE_FD_INTEREST_READ;
    }
    if ((revents & POLLOUT) != 0) {
        interests |= ECS_UI_WAKE_FD_INTEREST_WRITE;
    }
    return interests;
}

static bool MockPollFindReady(
    const EcsUiWakeWaitSpec *spec,
    EcsUiWakeHandle handle,
    uint32_t interest,
    const char *label)
{
    if (spec == NULL || spec->fd_count == 0u) {
        return false;
    }

    struct pollfd fds[ECS_UI_WAKE_FD_MAX];
    for (uint32_t i = 0u; i < spec->fd_count; i += 1u) {
        fds[i] = (struct pollfd){
            .fd = spec->fds[i].fd,
            .events = PollEventsForInterests(spec->fds[i].interests),
            .revents = 0,
        };
    }

    const int ready = poll(fds, (nfds_t)spec->fd_count, 0);
    if (ready <= 0) {
        return false;
    }

    for (uint32_t i = 0u; i < spec->fd_count; i += 1u) {
        if (!HandleEquals(spec->fds[i].handle, handle)) {
            continue;
        }
        const uint32_t ready_interests = InterestsForPollRevents(fds[i].revents);
        return (ready_interests & interest) != 0u &&
            strcmp(spec->fds[i].label, label) == 0;
    }
    return false;
}

static int TestFdReadWriteAndDynamicRemove(void)
{
    int result = 0;
    int pipe_fds[2] = {-1, -1};
    result |= Require(pipe(pipe_fds) == 0, "pipe creation failed");
    if (result != 0) {
        return result;
    }

    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle reader =
        EcsUiWakeRegisterPosixFd(
            registry,
            "reader",
            pipe_fds[0],
            ECS_UI_WAKE_FD_INTEREST_READ);
    EcsUiWakeHandle writer =
        EcsUiWakeRegisterPosixFd(
            registry,
            "writer",
            pipe_fds[1],
            ECS_UI_WAKE_FD_INTEREST_WRITE);
    EcsUiWakeWaitSpec spec = {0};
    const unsigned char byte = 42u;

    result |= Require(
        write(pipe_fds[1], &byte, sizeof(byte)) == (ssize_t)sizeof(byte),
        "pipe write failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "fd wait spec failed");
    result |= Require(spec.fd_count == 2u, "fd count mismatch");
    result |= Require(
        MockPollFindReady(
            &spec,
            reader,
            ECS_UI_WAKE_FD_INTEREST_READ,
            "reader"),
        "fd read readiness attribution mismatch");
    result |= Require(
        MockPollFindReady(
            &spec,
            writer,
            ECS_UI_WAKE_FD_INTEREST_WRITE,
            "writer"),
        "fd write readiness attribution mismatch");

    result |= Require(
        EcsUiWakeSetFdInterests(
            registry,
            reader,
            ECS_UI_WAKE_FD_INTEREST_NONE),
        "clear fd interests failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "fd interest clear spec failed");
    result |= Require(spec.fd_count == 1u, "cleared fd interest should hide fd");
    result |= Require(
        EcsUiWakeSetFdInterests(
            registry,
            reader,
            ECS_UI_WAKE_FD_INTEREST_READ),
        "restore fd interests failed");
    result |= Require(
        EcsUiWakeRemove(registry, reader),
        "remove reader failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "fd remove spec failed");
    result |= Require(spec.fd_count == 1u, "removed fd should leave one fd");
    result |= Require(
        spec.fds[0].fd == pipe_fds[1] &&
            strcmp(spec.fds[0].label, "writer") == 0,
        "remaining fd should be writer");
    result |= Require(
        EcsUiWakeRemove(registry, writer),
        "remove writer failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "fd empty spec failed");
    result |= Require(spec.fd_count == 0u, "all fds should be removed");

    EcsUiWakeRegistryDestroy(registry);
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    return result;
}

typedef struct ThreadPostCtx {
    EcsUiWakeRegistry *registry;
    EcsUiWakeHandle handle;
    bool ok;
} ThreadPostCtx;

static void *ThreadPostMain(void *arg)
{
    ThreadPostCtx *ctx = (ThreadPostCtx *)arg;
    ctx->ok = EcsUiWakePost(ctx->registry, ctx->handle);
    return NULL;
}

static int TestPostFromThreadExactlyOnce(void)
{
    int result = 0;
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle handle =
        EcsUiWakeRegisterPending(registry, "thread-post");
    ThreadPostCtx ctx = {
        .registry = registry,
        .handle = handle,
        .ok = false,
    };
    pthread_t thread;
    EcsUiWakeWaitSpec spec = {0};

    result |= Require(
        pthread_create(&thread, NULL, ThreadPostMain, &ctx) == 0,
        "pthread create failed");
    result |= Require(pthread_join(thread, NULL) == 0, "pthread join failed");
    result |= Require(ctx.ok, "thread post returned false");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "thread post spec failed");
    result |= Require(spec.hot, "thread post should make source hot");
    result |= Require(
        spec.active_count == 1u && ActiveLabelIs(&spec, 0u, "thread-post"),
        "thread post attribution mismatch");
    result |= Require(
        EcsUiWakeConsumePosts(registry) == 1u,
        "thread post should consume once");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "thread post after consume spec failed");
    result |= Require(!spec.hot, "thread post should be one-shot");
    result |= Require(
        EcsUiWakeConsumePosts(registry) == 0u,
        "thread post should not consume twice");

    EcsUiWakeRegistryDestroy(registry);
    return result;
}
#endif

static int TestStaleHandleRemovalRejected(void)
{
    int result = 0;
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle old_handle =
        EcsUiWakeRegisterPending(registry, "old");
    EcsUiWakeHandle new_handle = {0};
    EcsUiWakeWaitSpec spec = {0};

    result |= Require(
        EcsUiWakeRemove(registry, old_handle),
        "initial remove failed");
    new_handle = EcsUiWakeRegisterPending(registry, "new");
    result |= Require(
        EcsUiWakeHandleIsValid(new_handle),
        "new handle should be valid");
    result |= Require(
        old_handle.index == new_handle.index &&
            old_handle.generation != new_handle.generation,
        "slot reuse should advance generation");
    result |= Require(
        !EcsUiWakeRemove(registry, old_handle),
        "stale removal should be rejected");
    result |= Require(
        !EcsUiWakePost(registry, old_handle),
        "stale post should be rejected");
    result |= Require(
        EcsUiWakeSetPending(registry, new_handle, true),
        "new pending set failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "stale removal spec failed");
    result |= Require(
        spec.hot && spec.active_count == 1u &&
            ActiveLabelIs(&spec, 0u, "new"),
        "stale removal should not tear down reused slot");

    EcsUiWakeRegistryDestroy(registry);
    return result;
}

static int TestAnimationDeadlineIntegration(void)
{
    int result = 0;
    ecs_world_t *world = ecs_init();
    EcsUiWakeRegistry *registry = EcsUiWakeRegistryCreate();
    EcsUiWakeHandle deadline =
        EcsUiWakeRegisterDeadline(registry, "animation");
    EcsUiWakeWaitSpec spec = {0};

    result |= Require(world != NULL, "animation world missing");
    EcsUiAnimationImport(world);
    result |= Require(
        !EcsUiAnimationHasActive(world),
        "empty animation world should not be active");
    result |= Require(
        EcsUiAnimationArmNextFrameDeadline(
            world,
            registry,
            deadline,
            1000u),
        "inactive animation arm helper failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "inactive animation spec failed");
    result |= Require(
        !spec.hot && !spec.has_deadline,
        "inactive animation should not arm deadline");

    ecs_entity_t entity = ecs_new(world);
    EcsUiAnimationStartLinear1f(world, entity, 0.0f, 1.0f, 1.0f);
    result |= Require(
        EcsUiAnimationHasActive(world),
        "started animation should be active");
    result |= Require(
        EcsUiAnimationArmNextFrameDeadline(
            world,
            registry,
            deadline,
            1000u),
        "active animation arm helper failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 0u, &spec),
        "active animation spec failed");
    result |= Require(
        !spec.hot && spec.has_deadline && spec.deadline_ns == 1000u &&
            strcmp(spec.deadline_label, "animation") == 0,
        "active animation should arm next-frame deadline");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 1000u, &spec),
        "due animation deadline spec failed");
    result |= Require(
        spec.hot && spec.active_count == 1u &&
            ActiveLabelIs(&spec, 0u, "animation"),
        "due animation deadline attribution mismatch");

    (void)ecs_progress(world, 1.1f);
    result |= Require(
        !EcsUiAnimationHasActive(world),
        "completed animation should not be active");
    result |= Require(
        EcsUiAnimationArmNextFrameDeadline(
            world,
            registry,
            deadline,
            2000u),
        "completed animation disarm helper failed");
    result |= Require(
        EcsUiWakeBuildWaitSpec(registry, 1500u, &spec),
        "completed animation spec failed");
    result |= Require(
        !spec.hot && !spec.has_deadline,
        "completed animation should clear deadline");

    EcsUiWakeRegistryDestroy(registry);
    ecs_fini(world);
    return result;
}

int main(void)
{
    int result = 0;
    result |= TestPendingPostCoalescing();
    result |= TestDeadlineOrdering();
    result |= TestStaleHandleRemovalRejected();
    result |= TestAnimationDeadlineIntegration();
#if !defined(_WIN32)
    result |= TestFdReadWriteAndDynamicRemove();
    result |= TestPostFromThreadExactlyOnce();
#endif
    return result;
}
