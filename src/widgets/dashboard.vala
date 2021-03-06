namespace ExpidusOSShell {
	public class DashboardPreview : Gtk.Bin {
		private Shell _shell;
		private Desktop _desktop;
		private Gtk.ListBox list;
		private Gtk.Box box;

		public Shell shell {
			get {
				return this._shell;
			}
		}

		public DashboardPreview(Shell shell, Desktop desktop) {
			this._shell = shell;
			this._desktop = desktop;

			this.box = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);

			this.list = new Gtk.ListBox();
			this.list.bind_model(new NotificationsListModel(shell), (item) => {
				var notif = item as Notification;
				return new NotificationIcon(notif, Gtk.IconSize.SMALL_TOOLBAR);
			});
			this.box.set_center_widget(this.list);

			this.add(this.box);
		}
	}

	public class Dashboard : Gtk.Bin {
		private Shell _shell;
		private Desktop _desktop;

		private Gtk.Box box;
		private Gtk.Box header_box;
		private Gtk.Box footer_box;

		private Gtk.Image profile_icon;
		private Gtk.Label profile_label;

		private Gtk.Scale volume_slider;

		public Shell shell {
			get {
				return this._shell;
			}
		}

		public Dashboard(Shell shell, Desktop desktop) {
			this._shell = shell;
			this._desktop = desktop;

			this.box = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);
			this.header_box = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);
			this.footer_box = new Gtk.Box(Gtk.Orientation.VERTICAL, 0);

			this.box.pack_start(this.header_box);
			this.box.pack_end(this.footer_box);
			this.add(this.box);

			{
				if (this.shell.user != null) {
					var row = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 5);

					if (this.shell.user.icon_file != null) {
						var size = Utils.dpi(this.shell, desktop.monitor.index, 100);
						try {
							this.profile_icon = new Gtk.Image.from_pixbuf(new Gdk.Pixbuf.from_file_at_scale(this.shell.user.icon_file, size, size, true));
							row.pack_start(this.profile_icon, false, false);
						} catch (GLib.Error e) {
							stderr.printf("expidus-shell: failed to load user icon (%s): %s\n", e.domain.to_string(), e.message);
						}
					}

					var name = (this.shell.user.real_name != null && this.shell.user.real_name.length > 0) ? this.shell.user.real_name : this.shell.user.get_user_name();
					this.profile_label = new Gtk.Label(name);
					row.add(this.profile_label);

					this.header_box.pack_start(row, false, false);
				}

				var volume_box = new Gtk.Box(Gtk.Orientation.HORIZONTAL, 2);
				volume_box.pack_start(new Gtk.Image.from_icon_name("audio-volume-high", Gtk.IconSize.LARGE_TOOLBAR), false, false);
				this.volume_slider = new Gtk.Scale(Gtk.Orientation.HORIZONTAL, null);
				this.volume_slider.set_range(0, 100.0);
				this.volume_slider.draw_value = false;
				this.volume_slider.value_changed.connect(() => {
					this.set_volume(this.volume_slider.get_value());
				});
				volume_box.pack_end(this.volume_slider);
				this.header_box.add(volume_box);
			}

			{
				var viewport = new Gtk.Viewport(null, null);
				var list = new Gtk.ListBox();
				list.bind_model(new NotificationsListModel(this.shell), (item) => {
					var notif = item as Notification;
					return new NotificationBox(notif);
				});
				viewport.add(list);
				this.header_box.add(viewport);
			}

			{
				var power_buttons = new Gtk.ButtonBox(Gtk.Orientation.HORIZONTAL);

				var log_out = new Gtk.Button.from_icon_name("system-log-out");
				var style = log_out.get_style_context();
				style.add_class("expidus-shell-panel-button");
				log_out.clicked.connect(() => this.shell.main_loop.quit());
				power_buttons.add(log_out);

				if (this.shell.logind.can_reboot) {
					var reboot = new Gtk.Button.from_icon_name("system-reboot");
					style = reboot.get_style_context();
					style.add_class("expidus-shell-panel-button");

					reboot.clicked.connect(() => {
						try {
							this.shell.logind.reboot();
						} catch (GLib.Error e) {}
					});
					power_buttons.add(reboot);
				}

				if (this.shell.logind.can_poweroff) {
					var shutdown = new Gtk.Button.from_icon_name("system-shutdown");
					style = shutdown.get_style_context();
					style.add_class("expidus-shell-panel-button");

					shutdown.clicked.connect(() => {
						try {
							this.shell.logind.poweroff();
						} catch (GLib.Error e) {}
					});
					power_buttons.add(shutdown);
				}

				this.footer_box.pack_end(power_buttons, false, false);
			}
		}

		private void set_volume(double v) {
			this.shell.pulse.get_server_info((c, server_info) => {
				c.get_sink_info_by_name(server_info.default_sink_name, (c, sink_info, eol) => {
					if (sink_info != null) {
						PulseAudio.CVolume vol = {};
						vol.@set(sink_info.volume.channels, (PulseAudio.Volume)((v * PulseAudio.Volume.NORM) / 100));
						c.set_sink_volume_by_name(sink_info.name, vol, (c, s) => {});
					}
				});
			});
		}

		public void update() {
			this.shell.pulse.get_server_info((c, server_info) => {
				c.get_sink_info_by_name(server_info.default_sink_name, (c, sink_info, eol) => {
						if (sink_info != null) {
							var vol = sink_info.volume.avg();
							if (vol > PulseAudio.Volume.NORM) vol = PulseAudio.Volume.NORM;

							double v = vol * 100.0 / PulseAudio.Volume.NORM;
							this.volume_slider.set_value(v);
						}
				});
			});
		}
	}

	public class DashboardPanel : SidePanel {
		public override PanelSide side {
			get {
				return this.desktop.monitor.is_mobile ? PanelSide.TOP : PanelSide.RIGHT;
			}
		}

		public DashboardPanel(Shell shell, Desktop desktop, int monitor_index) {
			Object(shell: shell, desktop: desktop, monitor_index: monitor_index, widget_preview: new DashboardPreview(shell, desktop), widget_full: new Dashboard(shell, desktop));
		}

		public void update() {
			var dashboard = this.widget_full as Dashboard;
			assert(dashboard != null);

			dashboard.update();
		}
	}
}
