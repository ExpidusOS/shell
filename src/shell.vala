namespace ExpidusOSShell {
	public errordomain ShellErrors {
		INVALID_COMPOSITOR
	}

	[DBus(name = "com.expidus.Shell")]
	public class Shell : GLib.Object {
		private DBusConnection _conn;
		private uint dbus_own_id;
		private GLib.Subprocess xfwm;
		private XfconfDaemon xfconf;
		private Wnck.Screen _wnck_screen;
		private XSettings xsettings;
		private Logind.Client _logind;
		private Act.UserManager _act;
		private Act.User? _user = null;

		private GLib.MainLoop _main_loop;
		private Gdk.Display _disp;
		private GLib.List<Monitor> monitors;
		private GLib.Settings _settings;
		private NotificationsDaemon _notifs;
		private NM.Client _nm;
		private PulseAudio.Context _pulse;
		private PulseAudio.GLibMainLoop pulse_loop;
		private Up.Client _upower;
		private GLib.SubprocessLauncher _internal_launcher;

		[DBus(visible = false)]
		public GLib.SubprocessLauncher internal_launcher {
			get {
				return this._internal_launcher;
			}
		}

		[DBus(visible = false)]
		public Act.User? user {
			get {
				return this._user;
			}
		}

		[DBus(visible = false)]
		public Act.UserManager act {
			get {
				return this._act;
			}
		}

		[DBus(visible = false)]
		public Logind.Client logind {
			get {
				return this._logind;
			}
		}

		[DBus(visible = false)]
		public Up.Client upower {
			get {
				return this._upower;
			}
		}

		[DBus(visible = false)]
		public PulseAudio.Context pulse {
			get {
				return this._pulse;
			}
		}

		[DBus(visible = false)]
		public NM.Client nm {
			get {
				return this._nm;
			}
		}

		[DBus(visible = false)]
		public NotificationsDaemon notifs {
			get {
				return this._notifs;
			}
		}

		[DBus(visible = false)]
		public GLib.MainLoop main_loop {
			get {
				return this._main_loop;
			}
		}

		[DBus(visible = false)]
		public DBusConnection conn {
			get {
				return this._conn;
			}
		}

		[DBus(visible = false)]
		public Gdk.Display disp {
			get {
				return this._disp;
			}
		}

		[DBus(visible = false)]
		public GLib.Settings settings {
			get {
				return this._settings;
			}
		}

		[DBus(visible = false)]
		public Wnck.Screen wnck_screen {
			get {
				return this._wnck_screen;
			}
		}

		public Shell() throws ShellErrors, GLib.IOError, GLib.SpawnError, GLib.Error {
			Object();

			this._settings = new GLib.Settings("com.expidus.shell");
			this._main_loop = new GLib.MainLoop();
			this._conn = GLib.Bus.get_sync(BusType.SESSION);
			this._notifs = new NotificationsDaemon(this);
			this.xfconf = new XfconfDaemon(this);

			this._disp = Gdk.Display.get_default();
			assert(this.disp != null);

			this._internal_launcher = new GLib.SubprocessLauncher(GLib.SubprocessFlags.STDERR_SILENCE | GLib.SubprocessFlags.STDOUT_SILENCE);
			this.xfwm = this.internal_launcher.spawnv({"xfwm4"});

			this.xsettings = new XSettings(this);
			this.settings.changed["theme"].connect(() => {
				this.xsettings.set_string("Net/ThemeName", this.settings.get_string("theme"));
				try {
					this.xsettings.update();
				} catch (GLib.Error e) {
						stderr.printf("expidus-shell: failed to update xsettings: (%s) %s\n", e.domain.to_string(), e.message);
				}
				this.xfconf.PropertyChanged("xfwm4", "/generic/theme", new Variant.string(this.settings.get_string("theme")));
			});

			this.settings.changed["icon-theme"].connect(() => {
				this.xsettings.set_string("Net/IconThemeName", this.settings.get_string("icon-theme"));
				try {
					this.xsettings.update();
				} catch (GLib.Error e) {
						stderr.printf("expidus-shell: failed to update xsettings: (%s) %s\n", e.domain.to_string(), e.message);
				}
			});

			List<StartupWindow> startup_windows = new GLib.List<StartupWindow>();
			for (var i = 0; i < this.disp.get_n_monitors(); i++) {
				startup_windows.append(new StartupWindow(this, i));
			}

			this.dbus_own_id = GLib.Bus.own_name_on_connection(this.conn, "com.expidus.Shell", GLib.BusNameOwnerFlags.NONE);
			assert(this.dbus_own_id > 0);
			this.conn.register_object("/com/expidus/shell", this);

			this.monitors = new GLib.List<Monitor>();

			this._wnck_screen = Wnck.Screen.get_default();
			var screen = this.disp.get_default_screen();
			assert(screen != null);

			var provider = new Gtk.CssProvider();
			provider.load_from_resource("/com/expidus/shell/style.css");
			Gtk.StyleContext.add_provider_for_screen(screen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

			this._nm = new NM.Client();
			this._upower = new Up.Client();
			this._logind = new Logind.Client();
			this._act = Act.UserManager.get_default();

			this._act.list_users().@foreach((user) => {
				if (user.get_uid() == Posix.getuid() && this._user == null) {
					this._user = user;
				}
			});

			this.pulse_loop = new PulseAudio.GLibMainLoop(this.main_loop.get_context());
			this._pulse = new PulseAudio.Context(this.pulse_loop.get_api(), null);
			this._pulse.set_state_callback((c) => {
				switch (c.get_state()) {
					case PulseAudio.Context.State.FAILED:
						stderr.printf("expidus-shell: failed to connect to pulseaudio\n");
						break;
					case PulseAudio.Context.State.READY:
						c.set_subscribe_callback((c, type, i) => {
							for (unowned var item = this.monitors.first(); item != null; item = item.next) {
								var monitor = item.data;
								monitor.desktop.panel.sound.update();
								monitor.desktop.dashboard_panel.update();
							}
						});

						c.subscribe(PulseAudio.Context.SubscriptionMask.SERVER | PulseAudio.Context.SubscriptionMask.CARD | PulseAudio.Context.SubscriptionMask.SINK | PulseAudio.Context.SubscriptionMask.SOURCE, (c, s) => {
							assert(s == 1);
						});

						for (unowned var item = this.monitors.first(); item != null; item = item.next) {
							var monitor = item.data;
							monitor.desktop.panel.sound.update();
							monitor.desktop.dashboard_panel.update();
						}
						break;
					case PulseAudio.Context.State.TERMINATED:
						stderr.printf("expidus-shell: disconnected from pulseaudio\n");
						break;
					default:
						break;
				}
			});
			assert(this._pulse.connect() == 0);

			for (var i = 0; i < this.disp.get_n_monitors(); i++) {
				this.add_monitor(i);
			}

			screen.size_changed.connect(() => {
				for (unowned var item = this.monitors.first(); item != null; item = item.next) {
					var monitor = item.data;
					try {
						monitor.update();
					} catch (GLib.Error e) {
						stderr.printf("expidus-shell: failed to update monitor: (%s) %s\n", e.domain.to_string(), e.message);
					}
				}
			});

			screen.monitors_changed.connect(() => {
				for (unowned var item = this.monitors.first(); item != null; item = item.next) {
					var monitor = item.data;
					try {
						monitor.update();
					} catch (GLib.Error e) {
						stderr.printf("expidus-shell: failed to update monitor: (%s) %s\n", e.domain.to_string(), e.message);
					}
				}
			});

			this.disp.monitor_added.connect((monitor) => {
				for (var i = 0; i < this.disp.get_n_monitors(); i++) {
					if (monitor.get_geometry().equal(this.disp.get_monitor(i).get_geometry())) {
						var mon = this.find_monitor(monitor.get_geometry());
						if (mon != null) {
							try {
								this.add_monitor(i);
							} catch (GLib.Error e) {
								stderr.printf("expidus-shell: failed to add monitor: (%s) %s\n", e.domain.to_string(), e.message);
							}
							break;
						}
					}
				}
			});

			this.disp.monitor_removed.connect((monitor) => {
				for (var i = 0; i < this.disp.get_n_monitors(); i++) {
					var mon = this.disp.get_monitor(i);
					if (mon.get_geometry().equal(monitor.get_geometry())) {
						this.remove_monitor(i);
						break;
					}
				}
			});

			GLib.TimeoutSource timeout = new GLib.TimeoutSource.seconds(10);
			timeout.set_callback(() => {
				timeout.destroy();
				for (unowned var item = startup_windows.first(); item != null; item = item.next) {
					var startup_win = item.data;
					startup_win.hide();
					startup_windows.remove(startup_win);
				}
				this.disp.get_default_seat().ungrab();
				return false;
			});
			timeout.attach(this.main_loop.get_context());
		}

		~Shell() {
			GLib.Bus.unown_name(this.dbus_own_id);

			this.xfwm.force_exit();
		}

		public signal void monitor_added(int i, GLib.ObjectPath path);
		public signal void monitor_removed(int i, GLib.ObjectPath path);

		[DBus(visible = false)]
		public Monitor? get_monitor(int i) {
			return this.monitors.nth_data(i);
		}

		[DBus(visible = false)]
		public Monitor? find_monitor(Gdk.Rectangle geo) {
			for (unowned var item = this.monitors.first(); item != null; item = item.next) {
				var mon = item.data;
				if (mon.geometry.x == geo.x && mon.geometry.y == geo.y) return mon;
			}
			return null;
		}

		private void add_monitor(int i) throws GLib.IOError {
			if (this.monitors.nth_data(i) == null) {
				var path = "/com/expidus/shell/monitor/" + i.to_string();
				var monitor = new Monitor(this, i);
				this.monitors.insert(monitor, i);
				this.monitor_added(i, new GLib.ObjectPath(path));
			}
		}

		private void remove_monitor(int i) {
			if (this.monitors.nth_data(i) != null) {
				var path = "/com/expidus/shell/monitor/" + i.to_string();
				this.monitors.remove_link(this.monitors.nth(i));
				this.monitor_removed(i, new GLib.ObjectPath(path));
			}
		}
	}
}
