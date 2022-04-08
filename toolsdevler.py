import sys

if not hasattr(sys, 'argv'):
    sys.argv = ['']

import remmina
import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
import psutil


class HelloPlugin:
    def __init__(self):
        print("__init__!")
        self.name = "PyHello"
        self.type = "protocol"
        self.description = "Hello World!"
        self.version  = "1.0"
        self.icon_name = "org.remmina.Remmina-tool-symbolic"
        self.icon_name_ssh = "org.remmina.Remmina-tool-symbolic"
        self.btn = Gtk.Button(label="Hello!")
        self.btn.connect("clicked", self.callback_add, "hello")
        self.ssh_setting = remmina.PROTOCOL_SSH_SETTING_TUNNEL
        self.features = []
        self.basic_settings = []
        self.advanced_settings = []

    def callback_add(self, widget, data):
        print("Click :)")

    def init(self, gp):
        print("Init!")
        remmina.show_dialog(remmina.MESSAGE_INFO, remmina.BUTTONS_CLOSE, "This plugin is brought to you by Python :)")
        return True

    def open_connection(self, gp):
        print("open_connection!")

        viewport = gp.get_viewport()
        def foreach_child(child):
            child.add(self.btn)
            self.btn.show()

        viewport.foreach(foreach_child)
        remmina.protocol_plugin_signal_connection_opened(gp);
        print("Connected!")

        remmina.log_print("[%s]: Plugin open connection\n" % self.name)
        return True

    def draw(self, widget, cr, color):
        cr.rectangle(0, 0, 100, 100)
        cr.set_source_rgb(color[0], color[1], color[2])
        cr.fill()
        cr.queue_draw_area(0, 0, 100, 100)

        return True

    def close_connection(self, gp):
        print("close_connection!")
        remmina.log_print("[%s]: Plugin close connection\n" % self.name)
        remmina.protocol_plugin_signal_connection_closed(gp);
        return False

    def map_event(self, gp):
        print("[PyVNC.map_event]: Called!")
        return True

    def unmap_event(self, gp):
        print("[PyVNC.unmap_event]: Called!")
        return True

    def query_feature(self):
        pass

    def call_feature(self):
        pass

    def send_keystrokes(self):
        pass

    def get_plugin_screenshot(self):
        pass


myPlugin = HelloPlugin()
remmina.register_plugin(myPlugin)
