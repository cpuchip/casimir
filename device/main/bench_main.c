/* casim's on-chip bench: for each case (generated from the host CLI), the derivative on the S3 must
 * be byte-for-byte the host's (derive or simplify); then its time (min and median of REPS runs, esp_timer), the smallest
 * arena that gives OK (bisection), and the stack it takes: one derivative run in a fresh worker task,
 * whose high-water mark less an idle worker's is casim's own. The report repeats, so a reader that attaches late still
 * gets a whole one (BENCH BEGIN ... DEVICE DONE). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "casim.h"
#include "cases.h"

#define REPS 15
#define ARENA_MAX 32768
#define OUT_MAX 16384
#define STACK 65536
#define WSTACK 65536

typedef struct {
    int ok;                 /* the S3's text equals the host's */
    int64_t us_min, us_med;
    size_t arena;
    unsigned stack;         /* bytes of stack one derivative takes */
} result;

static unsigned char arena[ARENA_MAX] __attribute__((aligned(8)));
static char out[OUT_MAX];
static result res[256];
static unsigned stack_free, stack_base;

static casim_status run(const bench_case *k, size_t arena_cap)
{
    if (k->simplify) return casim_simplify(k->expr, k->angle, out, OUT_MAX, arena, arena_cap);
    return casim_derive(k->expr, k->var, k->angle, out, OUT_MAX, arena, arena_cap);
}

typedef struct {
    const bench_case *k;    /* NULL: the idle baseline */
    unsigned free;
    TaskHandle_t parent;
} job;

static void worker(void *arg)
{
    job *j = arg;
    if (j->k) run(j->k, ARENA_MAX);
    j->free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->parent);
    vTaskDelete(NULL);
}

/* The stack a fresh task used to run k (FreeRTOS fills a new stack with a known value). */
static unsigned stack_used(const bench_case *k)
{
    job j = {k, 0, xTaskGetCurrentTaskHandle()};
    if (xTaskCreate(worker, "casim_w", WSTACK, &j, 5, NULL) != pdPASS) return 0;
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    vTaskDelay(pdMS_TO_TICKS(20));          /* the idle task frees the worker */
    return WSTACK - j.free;
}

static int cmp64(const void *a, const void *b)
{
    int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
    return x < y ? -1 : x > y;
}

static void run_cases(void)
{
    for (int i = 0; i < case_count; i++) {
        const bench_case *k = &cases[i];
        int64_t t[REPS];
        size_t lo = 0, hi = ARENA_MAX;
        casim_status st = run(k, ARENA_MAX);
        res[i].ok = (int)st == k->status && (st != CASIM_OK || strcmp(out, k->want) == 0);
        for (int r = 0; r < REPS; r++) {
            int64_t t0 = esp_timer_get_time();
            run(k, ARENA_MAX);
            t[r] = esp_timer_get_time() - t0;
        }
        qsort(t, REPS, sizeof t[0], cmp64);
        res[i].us_min = t[0];
        res[i].us_med = t[REPS / 2];
        if (k->status != CASIM_OK) lo = hi = 0;       /* no arena gives OK: nothing to find */
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (run(k, mid) == CASIM_OK) hi = mid;
            else lo = mid + 1;
        }
        res[i].arena = lo;
    }
    stack_free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    stack_base = stack_used(NULL);
    for (int i = 0; i < case_count; i++) res[i].stack = stack_used(&cases[i]) - stack_base;
}

static void bench_task(void *arg)
{
    (void)arg;
    run_cases();
    for (;;) {
        int pass = 1;
        printf("BENCH BEGIN casim on ESP32-S3 at %d MHz, %d reps\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, REPS);
        printf("%-26s %2s %5s %8s %8s %6s %6s %5s %s\n", "case", "op", "chars", "us_min", "us_med", "arena", "stack",
               "out", "host");   /* out: the text's bytes, or -status */
        for (int i = 0; i < case_count; i++) {
            const bench_case *k = &cases[i];
            pass &= res[i].ok;
            printf("%-26s %2s %5d %8lld %8lld %6u %6u %5d %s\n", k->name, k->simplify ? "s" : "d", k->chars,
                   (long long)res[i].us_min,
                   (long long)res[i].us_med, (unsigned)res[i].arena, res[i].stack,
                   k->status ? -k->status : (int)strlen(k->want),
                   res[i].ok ? "same" : "DIFFERENT");
        }
        printf("stack: %u of %u bytes never touched by the bench task; an idle worker takes %u\n", stack_free,
               (unsigned)STACK, stack_base);
        printf(pass ? "DEVICE PASS\n" : "DEVICE FAIL\n");
        printf("DEVICE DONE\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    if (xTaskCreate(bench_task, "casim_bench", STACK, NULL, 5, NULL) != pdPASS) printf("DEVICE FAIL: no task\n");
}
