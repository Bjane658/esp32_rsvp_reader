#include "app_registry.h"
#include <string.h>

static App* s_apps[APP_REGISTRY_MAX];
static int  s_count = 0;

static App* s_stack[APP_STACK_DEPTH];
static int  s_top = -1;

void app_registry_register(App* app) {
    if (s_count < APP_REGISTRY_MAX) s_apps[s_count++] = app;
}

int app_registry_count() { return s_count; }

App* app_registry_get(int index) {
    return (index >= 0 && index < s_count) ? s_apps[index] : nullptr;
}

App* app_registry_find_by_id(const char* id) {
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_apps[i]->id, id) == 0) return s_apps[i];
    return nullptr;
}

void app_push(App* app) {
    if (s_top < APP_STACK_DEPTH - 1) s_top++;
    s_stack[s_top] = app;
    if (app && app->start) app->start();
}

void app_pop() {
    if (s_top <= 0) return;
    if (s_stack[s_top] && s_stack[s_top]->stop) s_stack[s_top]->stop();
    s_top--;
    if (s_stack[s_top] && s_stack[s_top]->show) s_stack[s_top]->show();
}

App* app_get_active() {
    if (s_top < 0) return s_count > 0 ? s_apps[0] : nullptr;
    return s_stack[s_top];
}

int app_get_active_index() {
    App* a = app_get_active();
    for (int i = 0; i < s_count; i++)
        if (s_apps[i] == a) return i;
    return 0;
}

void app_cycle() {
    if (s_count == 0) return;
    int next = (app_get_active_index() + 1) % s_count;
    if (s_top >= 0 && s_stack[s_top] && s_stack[s_top]->stop) s_stack[s_top]->stop();
    if (s_top < 0) s_top = 0;
    s_stack[s_top] = s_apps[next];
    if (s_stack[s_top] && s_stack[s_top]->start) s_stack[s_top]->start();
}
