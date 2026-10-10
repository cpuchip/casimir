/* casim's on-chip bench: for each case (generated from the host CLI), the derivative on the S3 must
 * be byte-for-byte the host's (derive or simplify); then its time (min and median of REPS runs, esp_timer), the smallest
 * arena that gives OK (bisection), and the stack it takes: one derivative run in a fresh worker task,
 * whose high-water mark less an idle worker's is casim's own. Then show steps (work/steps-derive.md, steps-integrate.md):
 * for each derive and integrate that is OK, every page timed once and the last (the root's result) REPS times, the
 * stack of one page, and the last page must end in the host's answer. The report repeats, so a reader that attaches late still
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
/* The calculator's budget for casim's stack (CAS 002b, 12 KB), and what it must leave (abacus #6224): the worst case
   plus one interrupt frame, which a tick saves on the task stack at any point (192 bytes, measured on this bench by
   a delay sweep across one tick, #6222; IDF's portasm.S switches to the interrupt stack after the first frame). */
#define STACK_BUDGET 12288
#define IRQ_FRAME 192
#define STACK_FLOOR 256

typedef struct {
    int ok;                 /* the S3's text equals the host's */
    int64_t us_min, us_med;
    size_t arena;
    unsigned stack;         /* bytes of stack one derivative takes */
} result;

static unsigned char arena[ARENA_MAX] __attribute__((aligned(8)));
static char out[OUT_MAX];
static result res[768];         /* more than case_count (checked in app_main) */

typedef struct {
    int count, ok;          /* the pages; the last ends in the host's answer */
    int64_t us_max, us_med; /* the slowest page, once each; the last page's median */
    unsigned stack;         /* bytes of stack the last page takes */
} paged;

static paged pg[768];
static unsigned stack_free, stack_base, base_max;

/* The calculator's own call (CAS 004): op and texts as the case has them, each read as its stamp. */
static casim_status run(const bench_case *k, size_t arena_cap)
{
    casim_req rq = {(casim_op)k->op, k->expr, k->model, k->var, k->repl, k->model, k->angle, k->repl2};
    return casim_run(&rq, out, OUT_MAX, arena, arena_cap);
}

/* show steps' page k of k's derivative */
static casim_status page(const bench_case *k, int at, casim_step_info *info)
{
    casim_req rq = {(casim_op)k->op, k->expr, k->model, k->var, k->repl, k->model, k->angle, k->repl2};
    return casim_step(&rq, at, 0, info, out, OUT_MAX, arena, ARENA_MAX);
}

typedef struct {
    const bench_case *k;    /* NULL: the idle baseline */
    unsigned free;
    TaskHandle_t parent;
    int at;                 /* >= 0: show steps' page at (page_worker) */
} job;

/* The run's worker, as it was before show steps: a local of its own here would move casim's stack by its size and
   not the idle baseline's (the baseline never touches it), so pages have their own worker below. */
static void worker(void *arg)
{
    job *j = arg;
    if (j->k) run(j->k, ARENA_MAX);
    j->free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->parent);
    vTaskDelete(NULL);
}

static void page_worker(void *arg)
{
    job *j = arg;
    casim_step_info info;
    page(j->k, j->at, &info);
    j->free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    xTaskNotifyGive(j->parent);
    vTaskDelete(NULL);
}

/* The stack a fresh task used to run k, or page at of it (FreeRTOS fills a new stack with a known value). */
static unsigned stack_used(const bench_case *k, int at)
{
    job j = {k, 0, xTaskGetCurrentTaskHandle(), at};
    int tries = 0;
    /* the idle task frees the last worker's stack: wait for it rather than report nonsense */
    while (xTaskCreate(k && at >= 0 ? page_worker : worker, "casim_w", WSTACK, &j, 5, NULL) != pdPASS)
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
        res[i].ok = (int)st == k->status && ((st != CASIM_OK && st != CASIM_PARTIAL) || strcmp(out, k->want) == 0);
        for (int r = 0; r < REPS; r++) {
            int64_t t0 = esp_timer_get_time();
            run(k, ARENA_MAX);
            t[r] = esp_timer_get_time() - t0;
        }
        qsort(t, REPS, sizeof t[0], cmp64);
        res[i].us_min = t[0];
        res[i].us_med = t[REPS / 2];
        if (k->status != CASIM_OK && k->status != CASIM_PARTIAL) lo = hi = 0;   /* no arena gives its answer */
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if ((int)run(k, mid) == k->status) hi = mid;
            else lo = mid + 1;
        }
        res[i].arena = lo;
    }
    stack_free = (unsigned)uxTaskGetStackHighWaterMark(NULL);
    /* the idle worker's own use, the least of several: one sample moved 520-720 bytes between builds, and
       the least gives the larger, safer figure for casim */
    stack_base = base_max = stack_used(NULL, -1);
    for (int i = 0; i < 7; i++) {
        unsigned b = stack_used(NULL, -1);
        if (b < stack_base) stack_base = b;
        if (b > base_max) base_max = b;
    }
    for (int i = 0; i < case_count; i++) {
        unsigned u = stack_used(&cases[i], -1);
        res[i].stack = u > stack_base ? u - stack_base : 0;    /* a loop-only case uses less than idle's noise */
    }
}

/* show steps on each derive and integrate the host answered OK */
static void run_pages(void)
{
    for (int i = 0; i < case_count; i++) {
        const bench_case *k = &cases[i];
        casim_step_info info;
        int64_t t[REPS];
        size_t lw = strlen(k->want), lo;
        pg[i].count = 0;
        if ((k->op != CASIM_DERIVE && k->op != CASIM_INTEGRATE) || k->status != CASIM_OK || page(k, -1, &info) != CASIM_OK)
            continue;
        pg[i].count = info.count;
        pg[i].us_max = 0;
        for (int at = 0; at < info.count; at++) {
            int64_t t0 = esp_timer_get_time(), dt;
            page(k, at, &info);
            dt = esp_timer_get_time() - t0;
            if (dt > pg[i].us_max) pg[i].us_max = dt;
        }
        for (int r = 0; r < REPS; r++) {
            int64_t t0 = esp_timer_get_time();
            page(k, pg[i].count - 1, &info);
            t[r] = esp_timer_get_time() - t0;
        }
        qsort(t, REPS, sizeof t[0], cmp64);
        pg[i].us_med = t[REPS / 2];
        /* the last page is the root's result: "d/dX(...) = <answer>", or the derived equation itself */
        lo = strlen(out);
        pg[i].ok = page(k, pg[i].count - 1, &info) == CASIM_OK && info.depth == 0 && info.line == CASIM_STEP_RESULT &&
                   lo >= lw && strcmp(out + lo - lw, k->want) == 0 &&
                   (lo == lw || (lo > lw + 3 && strncmp(out + lo - lw - 3, " = ", 3) == 0));
        {
            unsigned u = stack_used(k, pg[i].count - 1);
            pg[i].stack = u > stack_base ? u - stack_base : 0;
        }
    }
}

static void bench_task(void *arg)
{
    (void)arg;
    run_cases();
    run_pages();
    for (;;) {
        int pass = 1;
        printf("BENCH BEGIN casim on ESP32-S3 at %d MHz, %d reps\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ, REPS);
        printf("%-26s %2s %5s %8s %8s %6s %6s %5s %s\n", "case", "op", "chars", "us_min", "us_med", "arena", "stack",
               "out", "host");   /* out: the text's bytes, or -status */
        for (int i = 0; i < case_count; i++) {
            const bench_case *k = &cases[i];
            pass &= res[i].ok;
            printf("%-26s %2c %5d %8lld %8lld %6u %6u %5d %s\n", k->name, "dsecuvifnqgk"[k->op], k->chars,
                   (long long)res[i].us_min,
                   (long long)res[i].us_med, (unsigned)res[i].arena, res[i].stack,
                   k->status ? -k->status : (int)strlen(k->want),
                   res[i].ok ? "same" : "DIFFERENT");
        }
        {
            int n = 0, worst = -1;
            unsigned smax = 0;
            printf("STEPS %-20s %5s %8s %8s %6s %s\n", "case", "pages", "page_max", "last_med", "stack", "last");
            for (int i = 0; i < case_count; i++) {
                if (!pg[i].count) continue;
                n++;
                pass &= pg[i].ok;
                if (worst < 0 || pg[i].us_max > pg[worst].us_max) worst = i;
                if (pg[i].stack > smax) smax = pg[i].stack;
                printf("STEPS %-20s %5d %8lld %8lld %6u %s\n", cases[i].name, pg[i].count, (long long)pg[i].us_max,
                       (long long)pg[i].us_med, pg[i].stack, pg[i].ok ? "same" : "DIFFERENT");
            }
            if (worst >= 0)
                printf("steps: %d derivatives and integrals paged; the slowest page %lld us (%s), the deepest page stack %u\n", n,
                       (long long)pg[worst].us_max, cases[worst].name, smax);
        }
        {
            unsigned worst = 0;
            int wi = 0;
            for (int i = 0; i < case_count; i++) {
                if (res[i].stack > worst) worst = res[i].stack, wi = i;
                if (pg[i].count && pg[i].stack > worst) worst = pg[i].stack, wi = i;
            }
            printf("stack budget: worst %u (%s) + interrupt %u = %u of %u, leaving %d (floor %u)\n", worst,
                   cases[wi].name, IRQ_FRAME, worst + IRQ_FRAME, STACK_BUDGET,
                   (int)STACK_BUDGET - (int)(worst + IRQ_FRAME), STACK_FLOOR);
            if (worst + IRQ_FRAME + STACK_FLOOR > STACK_BUDGET) pass = 0;
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
