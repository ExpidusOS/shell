#include <expidus-shell/desktop.h>
#include <expidus-shell/overlay.h>
#include <expidus-shell/plugin.h>
#include <meta/meta-background.h>
#include <meta/meta-background-actor.h>
#include <meta/meta-background-content.h>
#include <meta/meta-background-group.h>
#include <meta/meta-monitor-manager.h>
#include <meta/meta-workspace-manager.h>

#define ACTOR_DATA_KEY "ExpidusOSSHell-Default-actor-data"

#define MAP_TIMEOUT 250

typedef struct {
  ClutterActor* bg_group;
  GSettings* settings;
  GSList* desktops;
} ExpidusShellPluginPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(ExpidusShellPlugin, expidus_shell_plugin, META_TYPE_PLUGIN);

typedef struct {
  ClutterActor* actor;
  MetaPlugin* plugin;
} EffectCompleteData;

typedef struct {
  ClutterActor* orig_parent;

  ClutterTimeline* tml_minimize;
  ClutterTimeline* tml_destroy;
  ClutterTimeline* tml_map;
} ActorPrivate;

static GQuark actor_data_quark = 0;

static void destroy_desktop(gpointer data) {
  // FIXME: Issues when data is null
  //g_clear_object(&data);
}

static gpointer copy_struts(gconstpointer src, gpointer data) {
  return g_slice_dup(MetaStrut, src);
}

static void strut_free(gpointer data) {
  g_slice_free(MetaStrut, data);
}

static void free_actor_private(gpointer data) {
  if (G_LIKELY(data != NULL)) g_slice_free(ActorPrivate, data);
}

static ActorPrivate* get_actor_private(MetaWindowActor* actor) {
  ActorPrivate* priv = g_object_get_qdata(G_OBJECT(actor), actor_data_quark);

  if (G_UNLIKELY(actor_data_quark == 0))
    actor_data_quark = g_quark_from_static_string(ACTOR_DATA_KEY);

  if (G_UNLIKELY(!priv)) {
    priv = g_slice_new0(ActorPrivate);
    g_object_set_qdata_full(G_OBJECT(actor), actor_data_quark, priv, free_actor_private);
  }

  return priv;
}

static ClutterTimeline* actor_animate(ClutterActor* actor, ClutterAnimationMode mode, guint dur, const gchar* first_prop, ...) {
  clutter_actor_save_easing_state(actor);
  clutter_actor_set_easing_mode(actor, mode);
  clutter_actor_set_easing_duration(actor, dur);

  va_list args;
  va_start(args, first_prop);
  g_object_set_valist(G_OBJECT(actor), first_prop, args);
  va_end(args);

  ClutterTransition* transition = clutter_actor_get_transition(actor, first_prop);
  clutter_actor_restore_easing_state(actor);
  return CLUTTER_TIMELINE(transition);
}

static void on_monitors_changed(MetaMonitorManager* mmngr, MetaPlugin* plugin) {
  ExpidusShellPlugin* self = EXPIDUS_SHELL_PLUGIN(plugin);
  ExpidusShellPluginPrivate* priv = expidus_shell_plugin_get_instance_private(self);

  MetaDisplay* disp = meta_plugin_get_display(plugin);
  clutter_actor_destroy_all_children(priv->bg_group);
  g_clear_slist(&priv->desktops, destroy_desktop);

  GSList* struts = NULL;
  for (int i = 0; i < meta_display_get_n_monitors(disp); i++) {
    MetaRectangle rect;
    meta_display_get_monitor_geometry(disp, i, &rect);

    ClutterActor* bg_actor = meta_background_actor_new(disp, i);
    ClutterContent* content = clutter_actor_get_content(bg_actor);
    MetaBackgroundContent* bg_content = META_BACKGROUND_CONTENT(content);

    clutter_actor_set_position(bg_actor, rect.x, rect.y);
    clutter_actor_set_size(bg_actor, rect.width, rect.height);

    MetaBackground* bg = meta_background_new(disp);
    
    ClutterColor color;
    clutter_color_init(&color, 0x1a, 0x1b, 0x26, 0xff);
    meta_background_set_color(bg, &color);
    meta_background_content_set_background(bg_content, bg);
    g_object_unref(bg);

    clutter_actor_add_child(priv->bg_group, bg_actor);

    ExpidusShellDesktop* desktop = g_object_new(EXPIDUS_SHELL_TYPE_DESKTOP, "plugin", self, "monitor-index", i, NULL);
    GtkWindow* desktop_win = GTK_WINDOW(desktop);
    gtk_widget_show_all(GTK_WIDGET(desktop));

    MetaStrut* strut = g_slice_new0(MetaStrut);
    g_assert(strut);
    strut->side = META_SIDE_TOP;

    int width;
    int height;
    gtk_window_get_size(desktop_win, &width, &height);

    strut->rect = meta_rect(rect.x, rect.y, width, 30);
    struts = g_slist_append(struts, strut);

    priv->desktops = g_slist_append(priv->desktops, desktop);
  }

  expidus_shell_plugin_update_struts(self, struts);
  g_clear_slist(&struts, strut_free);
}

static void expidus_shell_plugin_start(MetaPlugin* plugin) {
  ExpidusShellPlugin* self = EXPIDUS_SHELL_PLUGIN(plugin);
  ExpidusShellPluginPrivate* priv = expidus_shell_plugin_get_instance_private(self);

  MetaDisplay* disp = meta_plugin_get_display(plugin);
  MetaMonitorManager* mmngr = meta_monitor_manager_get();

  priv->bg_group = meta_background_group_new();
  clutter_actor_insert_child_below(meta_get_window_group_for_display(disp), priv->bg_group, NULL);

  g_signal_connect(mmngr, "monitors-changed", G_CALLBACK(on_monitors_changed), plugin);
  on_monitors_changed(mmngr, plugin);

  clutter_actor_show(meta_get_stage_for_display(disp));

  GtkCssProvider* css_provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(css_provider, "/com/expidus/shell/style.css");
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void on_map_effect_complete(ClutterTimeline* timeline, EffectCompleteData* data) {
  MetaPlugin* plugin = data->plugin;
  MetaWindowActor* win_actor = META_WINDOW_ACTOR(data->actor);
  ActorPrivate* priv = get_actor_private(win_actor);

  priv->tml_map = NULL;

  meta_plugin_map_completed(plugin, win_actor);
  g_free(data);
}

static void expidus_shell_plugin_map(MetaPlugin* plugin, MetaWindowActor* win_actor) {
  ClutterActor* actor = CLUTTER_ACTOR(win_actor);
  MetaWindow* win = meta_window_actor_get_meta_window(win_actor);
  MetaWindowType type = meta_window_get_window_type(win);

  if (type == META_WINDOW_NORMAL) {
    EffectCompleteData* data = g_new0(EffectCompleteData, 1);
    ActorPrivate* priv = get_actor_private(win_actor);

    clutter_actor_set_pivot_point(actor, 0.5, 0.5);
    clutter_actor_set_opacity(actor, 0);
    clutter_actor_set_scale(actor, 0.5, 0.5);
    clutter_actor_show(actor);

    priv->tml_map = actor_animate(actor, CLUTTER_EASE_OUT_QUAD, MAP_TIMEOUT, "opacity", 255, "scale-x", 1.0, "scale-y", 1.0, NULL);

    data->actor = actor;
    data->plugin = plugin;
    g_signal_connect(priv->tml_map, "completed", G_CALLBACK(on_map_effect_complete), data);
  } else meta_plugin_map_completed(plugin, win_actor);
}

static void expidus_shell_plugin_constructed(GObject* obj) {
  G_OBJECT_CLASS(expidus_shell_plugin_parent_class)->constructed(obj);

  ExpidusShellPlugin* self = EXPIDUS_SHELL_PLUGIN(obj);
  ExpidusShellPluginPrivate* priv = expidus_shell_plugin_get_instance_private(self);

  priv->desktops = NULL;
  priv->settings = g_settings_new("com.expidus.shell");
}

static void expidus_shell_plugin_dispose(GObject* obj) {
  G_OBJECT_CLASS(expidus_shell_plugin_parent_class)->dispose(obj);
}

static void expidus_shell_plugin_finalize(GObject* obj) {
  ExpidusShellPlugin* self = EXPIDUS_SHELL_PLUGIN(obj);
  ExpidusShellPluginPrivate* priv = expidus_shell_plugin_get_instance_private(self);

  g_clear_object(&priv->settings);
  g_clear_slist(&priv->desktops, destroy_desktop);

  G_OBJECT_CLASS(expidus_shell_plugin_parent_class)->finalize(obj);
}

static void expidus_shell_plugin_class_init(ExpidusShellPluginClass* klass) {
  GObjectClass* obj_class = G_OBJECT_CLASS(klass);
  MetaPluginClass* plugin_class = META_PLUGIN_CLASS(klass);

  obj_class->constructed = expidus_shell_plugin_constructed;
  obj_class->dispose = expidus_shell_plugin_dispose;
  obj_class->finalize = expidus_shell_plugin_finalize;

  plugin_class->start = expidus_shell_plugin_start;
  plugin_class->map = expidus_shell_plugin_map;
}

static void expidus_shell_plugin_init(ExpidusShellPlugin* self) {}

void expidus_shell_plugin_update_struts(ExpidusShellPlugin* self, GSList* struts) {
  ExpidusShellPluginPrivate* priv = expidus_shell_plugin_get_instance_private(self);

  for (GSList* desktop_item = priv->desktops; desktop_item != NULL; desktop_item = g_slist_next(desktop_item)) {
    ExpidusShellDesktop* desktop = desktop_item->data;
    ExpidusShellOverlay* overlay;
    g_object_get(desktop, "overlay", &overlay, NULL);

    GSList* desktop_struts = g_slist_copy_deep(expidus_shell_desktop_get_struts(desktop), copy_struts, NULL);
    GSList* overlay_struts = g_slist_copy_deep(expidus_shell_overlay_get_struts(overlay), copy_struts, NULL);
    struts = g_slist_concat(struts, desktop_struts);
    struts = g_slist_concat(struts, overlay_struts);
  }

  MetaDisplay* disp = meta_plugin_get_display(META_PLUGIN(self));
  MetaWorkspaceManager* wsmngr = meta_display_get_workspace_manager(disp);
  for (int i = 0; i < meta_workspace_manager_get_n_workspaces(wsmngr); i++) {
    MetaWorkspace* ws = meta_workspace_manager_get_workspace_by_index(wsmngr, i);
    meta_workspace_set_builtin_struts(ws, struts);
  }
}