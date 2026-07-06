#include "ecs_ui/ecs_ui_raylib.h"

#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void TestNullAndInvalidLifecycle(void)
{
    EcsUiRaylibPresentationCacheDestroy(NULL);
    EcsUiRaylibPresentationCacheEnd(NULL);

    Require(
        !EcsUiRaylibPresentationCacheHasCachedFrame(NULL),
        "null cache must not report a frame");
    Require(
        !EcsUiRaylibPresentationCacheEnsure(NULL, 1u, 1u, 1.0f),
        "null cache ensure must fail");
    Require(
        !EcsUiRaylibPresentationCacheBegin(NULL, BLACK),
        "null cache begin must fail");
    Require(
        !EcsUiRaylibPresentationCacheBlit(NULL),
        "null cache blit must fail");

    EcsUiRaylibPresentationCache *cache =
        EcsUiRaylibPresentationCacheCreate();
    Require(cache != NULL, "cache creation must return a cache");
    Require(
        !EcsUiRaylibPresentationCacheHasCachedFrame(cache),
        "new cache must not report a frame");
    Require(
        !EcsUiRaylibPresentationCacheEnsure(cache, 0u, 1u, 1.0f),
        "zero width ensure must fail");
    Require(
        !EcsUiRaylibPresentationCacheEnsure(cache, 1u, 0u, 1.0f),
        "zero height ensure must fail");
    Require(
        !EcsUiRaylibPresentationCacheBegin(cache, BLACK),
        "begin before ensure must fail");
    Require(
        !EcsUiRaylibPresentationCacheBlit(cache),
        "blit before a cached frame must fail");

    EcsUiRaylibPresentationCacheEnd(cache);
    EcsUiRaylibPresentationCacheDestroy(cache);
}

int main(void)
{
    TestNullAndInvalidLifecycle();
    return 0;
}
