namespace ExpidusOSShell {
	[DBus(name = "com.expidus.shell.Monitor")]
	public class Monitor {
		private Shell _shell;
		private int _index;
		private uint dbus_own_id;
		private Desktop _desktop;

		[DBus(visible = false)]
		public Shell shell {
			get {
				return this._shell;
			}
		}

		[DBus(visible = false)]
		public Desktop desktop {
			get {
				return this._desktop;
			}
		}

		[DBus(visible = false)]
		public Gdk.Rectangle geometry {
			get {
				var monitor = this.shell.disp.get_monitor(this.index);
				assert(monitor != null);
				return monitor.get_geometry();
			}
		}

		public bool is_mobile {
			get {
				return this.geometry.width < this.geometry.height;
			}
		}

		public double dpi {
			get {
				return Utils.get_dpi(this.shell, this.index);
			}
		}

		public int index {
			get {
				return this._index;
			}
		}

		public string? model {
			get {
				var monitor = this.shell.disp.get_monitor(this.index);
				assert(monitor != null);
				return monitor.model == null ? "(Unknown)" : monitor.model;
			}
		}

		public string? manufacturer {
			get {
				var monitor = this.shell.disp.get_monitor(this.index);
				assert(monitor != null);
				return monitor.manufacturer == null ? "(Unknown)" : monitor.manufacturer;
			}
		}

		public Monitor(Shell shell, int index) throws GLib.IOError {
			this._shell = shell;
			this._index = index;

			this.dbus_own_id = this.shell.conn.register_object("/com/expidus/shell/monitor/" + this.index.to_string(), this);
			this._desktop = new Desktop(shell, this, index);
		}

		~Monitor() {
			this.shell.conn.unregister_object(this.dbus_own_id);
		}

		public void toggle_appboard() throws GLib.Error {
			if (this.desktop.appboard_panel.mode == SidePanelMode.OPEN) {
				this.desktop.appboard_panel.mode = SidePanelMode.CLOSED;
			} else {
				this.desktop.appboard_panel.mode = SidePanelMode.OPEN;
			}
		}

		public void toggle_dashboard() throws GLib.Error {
			if (this.desktop.dashboard_panel.mode == SidePanelMode.OPEN) {
				this.desktop.dashboard_panel.mode = SidePanelMode.CLOSED;
			} else {
				this.desktop.dashboard_panel.mode = SidePanelMode.OPEN;
			}
		}

		public void update() throws GLib.Error {
			this.desktop.sync();
		}
	}
}
