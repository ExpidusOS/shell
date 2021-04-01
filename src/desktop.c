#include <expidus-shell/desktop.h>
#include <expidus-shell/shell.h>
#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1
#include <expidus-shell/utils.h>
#include <flutter_linux/flutter_linux.h>
#include <meta/display.h>
#include <meta/meta-plugin.h>
#include <libwnck/libwnck.h>
#include <expidus-build.h>
#include <flutter.h>

typedef struct {
	FlDartProject* proj;
  FlView* view;
  FlMethodChannel* channel_dart;
  FlMethodChannel* channel;

	int monitor_index;
	ExpidusShell* shell;
  GSList* struts;

  WnckScreen* screen;
  gulong sig_win_changed;
  gulong sig_app_closed;
  gulong sig_app_opened;
} ExpidusShellDesktopPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(ExpidusShellDesktop, expidus_shell_desktop, GTK_TYPE_WINDOW);

enum {
	PROP_0,
	PROP_SHELL,
	PROP_MONITOR_INDEX,
	N_PROPS
};

static GParamSpec* obj_props[N_PROPS] = { NULL };

static void expidus_shell_desktop_set_current_app_cb(GObject* obj, GAsyncResult* result, gpointer data) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(data);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  GError* error = NULL;
  FlMethodResponse* resp = fl_method_channel_invoke_method_finish(priv->channel_dart, result, &error);

  if (resp == NULL) {
    g_warning("Failed to call method: %s", error->message);
    return;
  }

  if (fl_method_response_get_result(resp, &error) == NULL) {
    g_warning("Method returned error: %s", error->message);
    return;
  }
}

static void expidus_shell_desktop_app_closed(WnckScreen* screen, WnckApplication* app, gpointer data) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(data);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  FlValue* args = fl_value_new_list();
  fl_value_append(args, fl_value_new_bool(FALSE));

  fl_method_channel_invoke_method(priv->channel_dart, "setCurrentApplication", args, NULL, expidus_shell_desktop_set_current_app_cb, self);
}

static gchar* cache_appicon(WnckApplication* app) {
  const gchar* name = wnck_application_get_name(app);
  if (wnck_application_get_icon(app) != NULL) {
    gchar* path = g_build_filename(g_get_tmp_dir(), g_strdup_printf("expidus-shell-appcache-%s-icon.png", name), NULL);
    GError* error = NULL;
    if (!gdk_pixbuf_save(wnck_application_get_icon(app), path, "png", &error, NULL)) {
      g_warning("Failed to cache icon: %s", error->message);
      return NULL;
    } else {
      return path;
    }
  }
  if (wnck_application_get_mini_icon(app) != NULL) {
    gchar* path = g_build_filename(g_get_tmp_dir(), g_strdup_printf("expidus-shell-appcache-%s-mini-icon.png", name), NULL);
    GError* error = NULL;
    if (!gdk_pixbuf_save(wnck_application_get_mini_icon(app), path, "png", &error, NULL)) {
      g_warning("Failed to cache icon: %s", error->message);
      return NULL;
    } else {
      return path;
    }
  }
  return NULL;
}

static void expidus_shell_desktop_method_handler(FlMethodChannel* channel, FlMethodCall* call, gpointer data) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(data);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  GError* error = NULL;
  if (!g_strcmp0(fl_method_call_get_name(call), "toggleActionButton")) {
    WnckWindow* win = wnck_screen_get_active_window(priv->screen);
    if (win != NULL) {
      char* unique_bus_name = wnck_window_get_property_string(win, "_GTK_UNIQUE_BUS_NAME");
      char* menubar_obj_path = wnck_window_get_property_string(win, "_GTK_MENUBAR_OBJECT_PATH");

      GDBusConnection* session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);

      GtkMenu* menu = NULL;
      if (unique_bus_name == NULL && menubar_obj_path == NULL) {
        menu = GTK_MENU(wnck_action_menu_new(win));
      } else {
        GDBusMenuModel* menu_model = g_dbus_menu_model_get(session, unique_bus_name, menubar_obj_path);
        menu = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(menu_model)));
      }

      GdkRectangle rect = { .x = 4, .y = 30 };
      gtk_menu_popup_at_rect(menu, gtk_widget_get_window(GTK_WIDGET(self)), &rect, GDK_GRAVITY_SOUTH, GDK_GRAVITY_SOUTH, NULL);
      g_object_unref(menu);
    }

    if (!fl_method_call_respond_success(call, fl_value_new_null(), &error)) {
      g_error("Failed to respond to call: %s", error->message);
      g_clear_error(&error);
    }
  } else {
    if (!fl_method_call_respond(call, FL_METHOD_RESPONSE(fl_method_not_implemented_response_new()), &error)) {
      g_error("Failed to respond to call: %s", error->message);
      g_clear_error(&error);
    }
  }
}

static void expidus_shell_desktop_app_opened(WnckScreen* screen, WnckApplication* app, gpointer data) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(data);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  FlValue* args = fl_value_new_list();
  const gchar* name = wnck_application_get_name(app);
  if (!g_strcmp0(name, "expidus-shell.bin")) {
    fl_value_append(args, fl_value_new_bool(FALSE));
  } else {
    fl_value_append(args, fl_value_new_bool(TRUE));
    fl_value_append(args, fl_value_new_string(name));
    const gchar* icon_name = wnck_application_get_icon_name(app);
    if (icon_name != NULL) {
      GtkIconTheme* icon_theme = gtk_icon_theme_get_default();
      GtkIconInfo* icon_info = gtk_icon_theme_lookup_icon(icon_theme, icon_name, 32, GTK_ICON_LOOKUP_NO_SVG);
      if (icon_info != NULL) {
        g_debug("Loaded icon info (non-svg image): %s", icon_name);
        fl_value_append(args, fl_value_new_string(gtk_icon_info_get_filename(icon_info)));
        g_object_unref(icon_info);
      } else {
        gchar* icon_path = cache_appicon(app);
        if (icon_path != NULL) fl_value_append(args, fl_value_new_string(icon_path));
      }
    } else {
      gchar* icon_path = cache_appicon(app);
      if (icon_path != NULL) fl_value_append(args, fl_value_new_string(icon_path));
    }
  }

  fl_method_channel_invoke_method(priv->channel_dart, "setCurrentApplication", args, NULL, expidus_shell_desktop_set_current_app_cb, self);
}

static void expidus_shell_desktop_win_changed(WnckScreen* screen, WnckWindow* prev_win, gpointer data) {
  WnckWindow* win = wnck_screen_get_active_window(screen);
  WnckApplication* app = win != NULL ? wnck_window_get_application(win) : NULL;
  if (win == NULL) {
    expidus_shell_desktop_app_closed(screen, NULL, data);
  } else {
    expidus_shell_desktop_app_opened(screen, app, data);
  }
}

static void expidus_shell_desktop_set_property(GObject* obj, guint prop_id, const GValue* value, GParamSpec* pspec) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(obj);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  switch (prop_id) {
    case PROP_SHELL:
      priv->shell = g_value_get_object(value);
      break;
    case PROP_MONITOR_INDEX:
      priv->monitor_index = g_value_get_uint(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void expidus_shell_desktop_get_property(GObject* obj, guint prop_id, GValue* value, GParamSpec* pspec) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(obj);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  switch (prop_id) {
    case PROP_SHELL:
      g_value_set_object(value, priv->shell);
      break;
    case PROP_MONITOR_INDEX:
      g_value_set_uint(value, priv->monitor_index);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
      break;
  }
}

static void expidus_shell_desktop_constructed(GObject* obj) {
  G_OBJECT_CLASS(expidus_shell_desktop_parent_class)->constructed(obj);

  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(obj);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  GtkWindow* win = GTK_WINDOW(self);
  gtk_window_set_role(win, "expidus-shell-desktop");
  gtk_window_set_decorated(win, FALSE);
  gtk_window_set_type_hint(win, GDK_WINDOW_TYPE_HINT_DESKTOP);
  gtk_window_set_skip_taskbar_hint(win, TRUE);
  gtk_window_set_skip_pager_hint(win, TRUE);
  gtk_window_set_focus_on_map(win, FALSE);

	MetaPlugin* plugin;
	g_object_get(priv->shell, "plugin", &plugin, NULL);
	g_assert(plugin);

	MetaDisplay* disp = meta_plugin_get_display(plugin);
  MetaRectangle rect;
  meta_display_get_monitor_geometry(disp, priv->monitor_index, &rect);
  gtk_window_move(win, rect.x, rect.y);
  gtk_window_set_default_size(win, rect.width, rect.height);
  gtk_window_set_resizable(win, FALSE);
  gtk_widget_set_size_request(GTK_WIDGET(win), rect.width, rect.height);
  gtk_window_set_accept_focus(win, FALSE);

	priv->proj = fl_dart_project_new();

	g_clear_pointer(&priv->proj->aot_library_path, g_free);
  g_clear_pointer(&priv->proj->assets_path, g_free);
  g_clear_pointer(&priv->proj->icu_data_path, g_free);

	priv->proj->aot_library_path = g_build_filename(EXPIDUS_SHELL_LIBDIR, "libapp.so", NULL);
	priv->proj->assets_path = g_build_filename(EXPIDUS_SHELL_LIBDIR, "assets", NULL);
	priv->proj->icu_data_path = g_build_filename(EXPIDUS_SHELL_LIBDIR, "icudtl.dat", NULL);

  char* argv[] = { g_strdup_printf("%d", priv->monitor_index), "desktop", NULL };
  fl_dart_project_set_dart_entrypoint_arguments(priv->proj, argv);

  priv->view = fl_view_new(priv->proj);

  gtk_container_add(GTK_CONTAINER(win), GTK_WIDGET(priv->view));
	gtk_widget_show_all(GTK_WIDGET(self));

  MetaStrut* strut = g_slice_new0(MetaStrut);
  g_assert(strut);
  strut->side = META_SIDE_TOP;
  strut->rect = meta_rect(rect.x, rect.y, rect.height, 30);
  priv->struts = g_slist_append(priv->struts, strut);

  FlEngine* engine = fl_view_get_engine(priv->view);
  FlBinaryMessenger* binmsg = fl_engine_get_binary_messenger(engine);
  priv->channel_dart = fl_method_channel_new(binmsg, "com.expidus.shell/desktop.dart", FL_METHOD_CODEC(fl_standard_method_codec_new()));

  priv->channel = fl_method_channel_new(binmsg, "com.expidus.shell/desktop", FL_METHOD_CODEC(fl_standard_method_codec_new()));
  fl_method_channel_set_method_call_handler(priv->channel, expidus_shell_desktop_method_handler, self, NULL);

  priv->screen = wnck_screen_get_default();
  g_assert(priv->screen);
  priv->sig_win_changed = g_signal_connect(priv->screen, "active-window-changed", G_CALLBACK(expidus_shell_desktop_win_changed), self);
  priv->sig_app_closed = g_signal_connect(priv->screen, "application-closed", G_CALLBACK(expidus_shell_desktop_app_closed), self);
  priv->sig_app_opened = g_signal_connect(priv->screen, "application-opened", G_CALLBACK(expidus_shell_desktop_app_opened), self);
}

static void expidus_shell_desktop_dispose(GObject* obj) {
  ExpidusShellDesktop* self = EXPIDUS_SHELL_DESKTOP(obj);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);

  g_clear_object(&priv->view);
  g_clear_object(&priv->proj);
  g_clear_object(&priv->channel_dart);
  g_clear_object(&priv->channel);

  if (priv->sig_app_closed > 0) {
    g_signal_handler_disconnect(priv->screen, priv->sig_app_closed);
    priv->sig_app_closed = 0;
  }

  if (priv->sig_app_opened > 0) {
    g_signal_handler_disconnect(priv->screen, priv->sig_app_opened);
    priv->sig_app_opened = 0;
  }

  G_OBJECT_CLASS(expidus_shell_desktop_parent_class)->dispose(obj);
}

static void expidus_shell_desktop_class_init(ExpidusShellDesktopClass* klass) {
	GObjectClass* obj_class = G_OBJECT_CLASS(klass);

  obj_class->set_property = expidus_shell_desktop_set_property;
  obj_class->get_property = expidus_shell_desktop_get_property;
  obj_class->constructed = expidus_shell_desktop_constructed;
  obj_class->dispose = expidus_shell_desktop_dispose;

  obj_props[PROP_SHELL] = g_param_spec_object("shell", "Shell", "The ExpidusOS Shell instance to connect to.", EXPIDUS_TYPE_SHELL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  obj_props[PROP_MONITOR_INDEX] = g_param_spec_uint("monitor-index", "Monitor Index", "The monitor's index to render and use.", 0, 255, 0, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_properties(obj_class, N_PROPS, obj_props);
}

static void expidus_shell_desktop_init(ExpidusShellDesktop* self) {}

GSList* expidus_shell_desktop_get_struts(ExpidusShellDesktop* self) {
  g_return_val_if_fail(EXPIDUS_SHELL_IS_DESKTOP(self), NULL);
  ExpidusShellDesktopPrivate* priv = expidus_shell_desktop_get_instance_private(self);
  g_return_val_if_fail(priv, NULL);
  return priv->struts;
}