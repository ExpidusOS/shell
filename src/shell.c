#include <expidus-shell/shell.h>
#include <meta/meta-plugin.h>

typedef struct {
	GList* desktops;
	MetaPlugin* plugin;
	GSettings* settings;

	GType desktop_type;
	GType overlay_type;
} ExpidusShellPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(ExpidusShell, expidus_shell, G_TYPE_OBJECT);

enum {
	PROP_0,
	PROP_PLUGIN,
	N_PROPS,

	SIG_0 = 0,
	SIG_DESKTOP_CREATED,
	SIG_DESKTOP_DESTROYED,
	SIG_SERVICE_LOAD,
	SIG_SERVICE_UNLOAD,
	N_SIGS,
};

static GParamSpec* obj_props[N_PROPS] = { NULL };
//static guint obj_signals[N_SIGS] = {};

static void expidus_shell_set_property(GObject* obj, guint prop_id, const GValue* value, GParamSpec* pspec) {
	ExpidusShell* self = EXPIDUS_SHELL(obj);
	ExpidusShellPrivate* priv = expidus_shell_get_instance_private(self);

	switch (prop_id) {
		case PROP_PLUGIN:
			priv->plugin = g_value_get_object(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
			break;
	}
}

static void expidus_shell_get_property(GObject* obj, guint prop_id, GValue* value, GParamSpec* pspec) {
	ExpidusShell* self = EXPIDUS_SHELL(obj);
	ExpidusShellPrivate* priv = expidus_shell_get_instance_private(self);

	switch (prop_id) {
		case PROP_PLUGIN:
			g_value_set_object(value, priv->plugin);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
			break;
	}
}

static void expidus_shell_constructed(GObject* obj) {
	G_OBJECT_CLASS(expidus_shell_parent_class)->constructed(obj);

	ExpidusShell* self = EXPIDUS_SHELL(obj);
	ExpidusShellPrivate* priv = expidus_shell_get_instance_private(self);

	priv->settings = g_settings_new("com.expidus.shell");
}

static void expidus_shell_finalize(GObject* obj) {
	ExpidusShell* self = EXPIDUS_SHELL(obj);
	ExpidusShellPrivate* priv = expidus_shell_get_instance_private(self);

	g_clear_object(&priv->settings);
	g_clear_list(&priv->desktops, g_object_unref);

	G_OBJECT_CLASS(expidus_shell_parent_class)->finalize(obj);
}

static void expidus_shell_class_init(ExpidusShellClass* klass) {
	GObjectClass* obj_class = G_OBJECT_CLASS(klass);

	obj_class->constructed = expidus_shell_constructed;
	obj_class->finalize = expidus_shell_finalize;
	obj_class->set_property = expidus_shell_set_property;
	obj_class->get_property = expidus_shell_get_property;

	obj_props[PROP_PLUGIN] = g_param_spec_object("plugin", "Plugin", "The Mutter plugin to connect with.", META_TYPE_PLUGIN, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_properties(obj_class, N_PROPS, obj_props);
}

static void expidus_shell_init(ExpidusShell* shell) {}

GList* expidus_shell_get_desktops(ExpidusShell* self) {
	g_return_val_if_fail(EXPIDUS_IS_SHELL(self), NULL);
	ExpidusShellPrivate* priv = expidus_shell_get_instance_private(self);
	g_return_val_if_fail(priv, NULL);
	return priv->desktops;
}