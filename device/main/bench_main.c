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
static result res[768];         /* more than case_count (checked in app_main) */
static unsigned stack_free, stack_base, base_max;

/* The calculator's own call (CAS 004): op and texts as the case has them, each read as its stamp. */
static casim_status run(const bench_case *k, size_t arena_cap)
{
    casim_req rq = {(casim_op)k->op, k->expr, k->model, k->var, k->repl, k->model, k->angle};
    return casim_run(&rq, out, OUT_MAX, arena, arena_cap);
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
    int tries = 0;
    /* the idle task frees the last worker's stack: wait for it rather than report nonsense */
    while (xTaskCreate(worker, "casim_w", WSTACK, &j, 5, NULL) != pdPASS)
        if (++tries > 50) return WSTACK + 1;    /* shown as more than the stack: a failed measurement */
        else vTaskDelay(pdMS_TO_TICKS(20));
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
    /* the idle worker's own use, the least of several: one sample moved 520-720 bytes between builds, and
       the least gives the larger, safer figure for casim */
    stack_base = base_max = stack_used(NULL);
    for (int i = 0; i < 7; i++) {
        unsigned b = stack_used(NULL);
        if (b < stack_base) stack_base = b;
        if (b > base_max) base_max = b;
    }
    for (int i = 0; i < case_count; i++) {
        unsigned u = stack_used(&cases[i]);
        res[i].stack = u > stack_base ? u - stack_base : 0;    /* a loop-only case uses less than idle's noise */
    }
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
            printf("%-26s %2c %5d %8lld %8lld %6u %6u %5d %s\n", k->name, "dsecuv"[k->op], k->chars,
                   (long long)res[i].us_min,
                   (long long)res[i].us_med, (unsigned)res[i].arena, res[i].stack,
                   k->status ? -k->status : (int)strlen(k->want),
                   res[i].ok ? "same" : "DIFFERENT");
        }
        printf("stack: %u of %u bytes never touched by the bench task; an idle worker takes %u (8 samples, %u to "
               "%u); a case's stack is its worker's use less %u\n",
               stack_free, (unsigned)STACK, stack_base, stack_base, base_max, stack_base);
        printf(pass ? "DEVICE PASS\n" : "DEVICE FAIL\n");
        printf("DEVICE DONE\n");
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1500));
    if (case_count > (int)(sizeof res / sizeof res[0])) {
        printf("DEVICE FAIL: %d cases, room for %d\n", case_count, (int)(sizeof res / sizeof res[0]));
        printf("DEVICE DONE\n");
        return;
    }
    if (xTaskCreate(bench_task, "casim_bench", STACK, NULL, 5, NULL) != pdPASS) printf("DEVICE FAIL: no task\n");
}
