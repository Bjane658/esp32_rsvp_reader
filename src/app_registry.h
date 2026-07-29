#pragma once

#define APP_REGISTRY_MAX  8
#define APP_STACK_DEPTH   4

struct App {
    const char* id;            // short NVS-safe ID, e.g. "rsvp" (max 14 chars)
    const char* display_name;  // shown in Settings > Mode

    void (*start)();
    void (*stop)();
    void (*loop)();
    void (*short_press)();
    void (*double_press)();
    void (*show)();            // redraw the app's screen (called by menu on exit)

    bool (*is_running)();      // nullptr = not a streaming app
    void (*set_running)(bool); // nullptr = no-op
    void (*on_chapter_changed)(); // nullptr = no-op

    bool cycleable;            // true = reading mode (RSVP/EReader): included in
                              // Settings > Mode cycle and eligible for boot
                              // restore. false = tool/utility app (Timer),
                              // launched from Apps and never persisted as boot app.
};

// Called from each app's .cpp — safe from global constructors
void app_registry_register(App* app);
int  app_registry_count();
App* app_registry_get(int index);
App* app_registry_find_by_id(const char* id);

// Stack-based dispatch: push/pop for overlays, cycle to replace top
void app_push(App* app);   // push onto stack and call start()
void app_set_active_top(App* app); // replace stack top in place, no start()/stop()
void app_pop();            // call stop() on top, pop, call show() on new top
App* app_get_active();     // top of stack (never nullptr if any app registered)
int  app_get_active_index(); // index of active app in registry
int  app_stack_depth();    // number of apps on the stack (1 = base only)
void app_cycle();          // replace top with next *cycleable* app (skips tools)
