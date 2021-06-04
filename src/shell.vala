namespace ExpidusOSShell {
	public errordomain ShellErrors {
		INVALID_COMPOSITOR
	}

	[DBus(name = "com.expidus.Shell")]
	public class Shell {
		private DBusConnection _conn;
		private uint dbus_own_id;
		private Pid xfwm_pid;
		private XfconfDaemon xfconf;

		private Gdk.Display _disp;
		private GLib.List<Monitor> monitors;
		private GLib.Settings _settings;

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

		public Shell() throws ShellErrors, GLib.IOError, GLib.SpawnError, GLib.Error {
			this.xfconf = new XfconfDaemon();

			{
				string args[] = {"xfwm4", "--replace"};
				GLib.Process.spawn_async(null, args, GLib.Environ.get(), GLib.SpawnFlags.STDERR_TO_DEV_NULL | GLib.SpawnFlags.STDOUT_TO_DEV_NULL | GLib.SpawnFlags.SEARCH_PATH, null, out this.xfwm_pid);
				GLib.ChildWatch.add(this.xfwm_pid, (pid, status) => {
					GLib.Process.close_pid(pid);
					GLib.Process.exit(status);
				});
			}

			this._conn = GLib.Bus.get_sync(BusType.SESSION);
			this.dbus_own_id = GLib.Bus.own_name_on_connection(this.conn, "com.expidus.Shell", GLib.BusNameOwnerFlags.NONE);
			this.conn.register_object("/com/expidus/shell", this);

			this.monitors = new GLib.List<Monitor>();
			this._settings = new GLib.Settings("com.expidus.shell");

			this._disp = Gdk.Display.get_default();
			assert(this.disp != null);

			var screen = this.disp.get_default_screen();
			assert(screen != null);

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
		}

		~Shell() {
			GLib.Bus.unown_name(this.dbus_own_id);
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
				if (mon.geometry.equal(geo)) return mon;
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
