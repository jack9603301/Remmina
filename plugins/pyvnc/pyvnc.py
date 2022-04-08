
import sys
import remmina
import enum
import gi
import inspect
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
import psutil
from PIL import Image


class VncFeature:
    PrefQuality = 1
    PrefViewonly = 2
    PrefDisableserverinput = 3
    ToolRefresh = 4
    ToolChat = 5
    ToolSendCtrlAltDel = 6
    Scale = 7
    Unfocus = 8

class VncData:
    def __init__(self):
        self.connected = False
        self.running = False
        self.auth_called = False
        self.auth_first = False
        self.drawing_area = False
        self.vnc_buffer = False
        self.rgb_buffer = False

def expose(area, context):
    context.scale(200, 200)
    context.set_source_rgb(0.5, 0.5, 0.7)
    context.fill()
    context.paint()

class Plugin:
    def __init__(self):
        self.button = None
        self.name = "PyVNC"
        self.type = "protocol"
        self.description = "VNC but in Python!"
        self.version  = "1.0"
        self.icon_name = "org.remmina.Remmina-vnc-symbolic"
        self.icon_name_ssh = "org.remmina.Remmina-vnc-ssh-symbolic"
        self.ssh_setting = remmina.PROTOCOL_SSH_SETTING_TUNNEL
        self.gpdata = VncData()
        self.qualities = ("0", "Poor Pixelmess", "1","Mhh kayy", "2","Nice", "9","hot sh*t")
        self.features = [
            remmina.Feature(
                type=remmina.PROTOCOL_FEATURE_TYPE_PREF,
                id=VncFeature.PrefQuality,
                opt1=remmina.PROTOCOL_FEATURE_PREF_RADIO,
                opt2="quality",
                opt3=self.qualities)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_PREF, VncFeature.PrefViewonly, remmina.PROTOCOL_FEATURE_PREF_CHECK, "viewonly", None)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_PREF, VncFeature.PrefDisableserverinput, remmina.PROTOCOL_SETTING_TYPE_CHECK, "disableserverinput", "Disable server input")
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_TOOL, VncFeature.ToolRefresh, "Refresh", "face-smile", None)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_TOOL, VncFeature.ToolChat, "Open Chat…", "face-smile", None)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_TOOL, VncFeature.ToolSendCtrlAltDel,     "Send Ctrl+Alt+Delete", None, None)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_SCALE, VncFeature.Scale, None, None, None)
            ,remmina.Feature(remmina.PROTOCOL_FEATURE_TYPE_UNFOCUS, VncFeature.Unfocus, None, None, None)
        ]

        colordepths = ("8", "256 colors (8 bpp)", "16", "High color (16 bpp)", "32", "True color (32 bpp)")
        self.basic_settings = [
            remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_SERVER,  name="server",    label="",             compact=False, opt1="_rfb._tcp",opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_TEXT,    name="proxy",     label="Repeater",     compact=False, opt1=None,       opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_TEXT,    name="username",  label="Username",     compact=False, opt1=None,       opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_PASSWORD,name="password",  label="User password",compact=False, opt1=None,       opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_SELECT,  name="colordepth",label="Color depth",  compact=False, opt1=colordepths,opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_SELECT,  name="quality",   label="Quality",      compact=False, opt1=self.qualities, opt2=None)
            , remmina.Setting(type=remmina.PROTOCOL_SETTING_TYPE_KEYMAP,  name="keymap",    label="",             compact=False, opt1=None,       opt2=None)
        ]
        self.advanced_settings = [
            remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "showcursor",             "Show remote cursor",       True,  None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "viewonly",               "View only",                False, None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "disableclipboard",       "Disable clipboard sync",   True,  None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "disableencryption",      "Disable encryption",       False, None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "disableserverinput",     "Disable server input",     True,  None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "disablepasswordstoring", "Disable password storing", True, None, None)
            , remmina.Setting(remmina.PROTOCOL_SETTING_TYPE_CHECK, "disablesmoothscrolling", "Disable smooth scrolling", True, None, None)
        ]

    def init(self, gp):
        #print("[PyVNC.init]: Called!")
        cfile = gp.get_file()
        self.gpdata.disable_smooth_scrolling = cfile.get_setting(key="disablesmoothscrolling", default=False)
        self.gpdata.drawing_area = gp.get_viewport()
        return True

    def open_connection(self, gp):
        #print("[PyVNC.open_connection]: Called!")
        print("Conect to %s" % remmina.pref_get_value("password"))
        connection_file = gp.get_file()
        print(connection_file.get_setting("disablepasswordstoring", False))
        password = None

        dont_save_passwords = connection_file.get_setting("disablepasswordstoring", False)
        ret = remmina.protocol_plugin_init_auth(widget=gp,
                                                flags=0, #if dont_save_passwords else remmina.REMMINA_MESSAGE_PANEL_FLAG_SAVEPASSWORD,
                                                title="Enter VNC password",
                                                default_username="",
                                                default_password="", #connection_file.get_setting("password", None),
                                                default_domain="",
                                                password_prompt="Enter VNC password")

        if ret == Gtk.ResponseType.CANCEL:
            print("Cancelled password prompt. Exiting...")
            return False
        elif ret == Gtk.ResponseType.OK:
            print("Password: %s" % gp.get_password())
            remmina.protocol_plugin_signal_connection_opened(gp)

        return True

    def close_connection(self, gp):
        print("[PyVNC.close_connection]: Called!")
        remmina.protocol_plugin_signal_connection_closed(gp)

    def query_feature(self, gp, feature):
        print("[PyVNC.query_feature]: Called!")
        return True

    def map_event(self, gp):
        print("[PyVNC.map_event]: Called!")
        return True

    def unmap_event(self, gp):
        print("[PyVNC.unmap_event]: Called!")
        return True

    def call_feature(self, gp, feature):
        #print("[PyVNC.call_feature]: Called!")

        if feature.type == remmina.REMMINA_PROTOCOL_FEATURE_TYPE_PREF and feature.id is VncFeature.PrefQuality:
            file = gp.get_file()
            quality = file.get_setting("quality", 0)
            if quality == 9:
                print("Ramping up graphics. Enjoy!")
            if quality == 0:
                print("Squeezing image into a few pixels...")
            if quality == 1:
                print("More the average guy, eh?")
            if quality == 2:
                print("Not great, not terrible. Just good...")

    def send_keystrokes(self, gp, strokes):
        print("[PyVNC.keystroke]: Called!")
        return True

    def get_plugin_screenshot(self, gp, data):
        return False

myPlugin = Plugin()
remmina.register_plugin(myPlugin)


class PluginTool:
    def __init__(self):
        self.button = None
        self.name = "Python Tool Plugin"
        self.type = "tool"
        self.description = "Press me!"
        self.version  = "1.0"

    def exec_func(self):
        print("exec_func has been called!")

myToolPlugin = PluginTool()
remmina.register_plugin(myToolPlugin)

class Pluginpref:
    def __init__(self):
        self.button = None
        self.name = "Python pref Plugin"
        self.pref_label = "Preference Label"
        self.type = "pref"
        self.description = "Press me!"
        self.version  = "1.0"
        self.button = Gtk.Button(label="Click Here")
        self.button.connect("clicked", self.on_button_clicked)

    def get_pref_body(self):
        return self.button

    def on_button_clicked(self):
        print("Click! :)")


myprefPlugin = Pluginpref()
remmina.register_plugin(myprefPlugin)


class Pluginfile:
    def __init__(self):
        self.button = None
        self.name = "Python pref Plugin"
        self.pref_label = "Preference Label"
        self.type = "pref"
        self.description = "Press me!"
        self.version  = "1.0"
        self.export_hints = ".ttf"
        print("Pluginfile")


    def import_test_func(self, file):
        print("import_test_func! {}" % file)
        return True

    def import_func(self, file):
        print("import_func! {}" % file)
        return remmina.file_new()

    def export_test_func(self, file):
        print("export_test_func! :)")
        return True

    def export_func(self, file):
        print("export_func! :)")
        return None


myfilePlugin = Pluginfile()
remmina.register_plugin(myfilePlugin)
