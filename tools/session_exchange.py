#! /usr/bin/python

# Session Exchange
# By Taybin Rutkin
# Copyright 2004-2005, under the GPL

VERSION='0.1.2'

#twisted libraries
from twisted.internet import gtk2reactor
gtk2reactor.install()
from twisted.internet import reactor, protocol
import twisted.internet.error

#pygtk libraries
import gobject
import gtk

#standard python2.2 libraries
import getopt
import os
import os.path
import re
import shelve
import string
import sys
import xml.dom.minidom

v2paths = True

def get_header_size(filename):
        size = 0
        file = open(filename, 'r')
        while True:
                chunk = file.read(4)
                size += 4
                if chunk == "data":
                        file.close()
                        return size + 4        #include the size chunk after "data"
                if not chunk:
                        file.close()
                        return None

def append_empty_data(self, filename, size):
        file = open(filename, 'a')
        file.seek(size-1)
        file.write('\x00')
        file.close()
        
def get_sound_list(snapshot):
        doc = xml.dom.minidom.parse(snapshot)
        
        regionlist = []
        playlists_tag = doc.getElementsByTagName('Playlists')
        playlists = playlists_tag[0].getElementsByTagName('Playlist')
        for play in playlists:
                regions = play.getElementsByTagName('Region')
                for region in regions:
                        regionlist.append(region.getAttribute('source-0'))
                        regionlist.append(region.getAttribute('source-1'))
                        regionlist.append(region.getAttribute('source-2'))
                        regionlist.append(region.getAttribute('source-3'))
                        regionlist.append(region.getAttribute('source-4'))
                        regionlist.append(region.getAttribute('source-5'))
        
        sourcelist = {}
        sources = doc.getElementsByTagName('Source')
        for source in sources:
                sourcelist[source.getAttribute('id')] = str(source.getAttribute('name'))

        soundlist = []
        for id in regionlist:
                if sourcelist.has_key(id):
                        soundlist.append(sourcelist[id])
        
        return soundlist

def raise_error(string, parent):
        dialog = gtk.MessageDialog(parent, gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        gtk.MESSAGE_WARNING, gtk.BUTTONS_OK, string)
        
        dialog.run()
        dialog.destroy()

class Data(object):
        def delete_snap(self, session, collab, snap):
                sessions = self._data['sessions']
                sessions[session]['collabs'][collab]['snaps'].remove(snap)
                self._data['sessions'] = sessions
        
        def delete_collab(self,session, collab):
                sessions = self._data['sessions']
                del sessions[session]['collabs'][collab]
                self._data['sessions'] = sessions
        
        def delete_session(self, session):
                sessions = self._data['sessions']
                del sessions[session]
                self._data['sessions'] = sessions
        
        def add_snap(self, session_name, collab_name, snap_name):
                sessions = self._data['sessions']
                sessions[session_name]['collabs'][collab_name]['snaps'].append(snap_name)
                sessions[session_name]['collabs'][collab_name]['snaps'].sort()
                self._data['sessions'] = sessions
                
                g_display.update_snap_view()
        
        def add_collab(self, session_name, collab_name, ip_address, port):
                sessions = self._data['sessions']
                sessions[session_name]['collabs'][collab_name] = {}
                sessions[session_name]['collabs'][collab_name]['snaps'] = []
                sessions[session_name]['collabs'][collab_name]['sounds'] = []
                sessions[session_name]['collabs'][collab_name]['ip'] = ip_address
                sessions[session_name]['collabs'][collab_name]['port'] = port
                sessions[session_name]['collabs'][collab_name]['v2paths'] = True
                self._data['sessions'] = sessions
                
                client = ExchangeClientFactory(session_name, collab_name, None, self.debug_mode)
                reactor.connectTCP(ip_address, port, client)
                g_display.show_status("connecting")
                
                g_display.update_collab_view()
        
        def add_session(self, session_path):
                sessions = self._data['sessions']
                
                session_name = session_path[session_path.rfind('/', 0, len(session_path)-2)+1: -1]
                sessions[session_name] = {}
                sessions[session_name]['path'] = session_path 
                sessions[session_name]['collabs'] = {}
                sessions[session_name]['collabs'][self._data['user']] = {}
                sessions[session_name]['collabs'][self._data['user']]['snaps'] = []
                sessions[session_name]['collabs'][self._data['user']]['sounds'] = []
                if os.path.isdir (os.path.join (session_path,'sounds')):
                        sessions[session_name]['collabs'][self._data['user']]['v2paths'] = False
                        v2paths = False
                else:
                        sessions[session_name]['collabs'][self._data['user']]['v2paths'] = True
                
                self._data['sessions'] = sessions
                
                self.rescan_session(session_name)

        def rescan_session(self, session_name):
                sessions = self._data['sessions']
                
                session_path = sessions[session_name]['path']
                sessions[session_name]['collabs'][self._data['user']]['snaps'] = self._scan_snapshots(session_path)
                sessions[session_name]['collabs'][self._data['user']]['sounds'] = self._scan_sounds(session_name)
                
                self._data['sessions'] = sessions
                
                g_display.update_snap_view()
                
                print self._data['sessions']
        
        def create_session(self, session_path):
                sessions = self._data['sessions']

                session_name = session_path[session_path.rfind('/', 0, len(session_path)-2)+1: ]
                try:
                        os.mkdir(session_path)
                        os.mkdir(os.path.join (session_path,'interchange'))
                        os.mkdir(os.path.join (session_path,'interchange',session_name))
                        os.mkdir(os.path.join (session_path,'interchange',session_name,'audiofiles'))
                except OSError:
                        raise_error("Could not create session directory", g_display.window)
                        return
                
                sessions[session_name] = {}
                sessions[session_name]['path'] = session_path
                sessions[session_name]['collabs'] = {}
                sessions[session_name]['collabs'][self._data['user']] = {}
                sessions[session_name]['collabs'][self._data['user']]['snaps'] = []
                sessions[session_name]['collabs'][self._data['user']]['sounds'] = []
                
                self._data['sessions'] = sessions
                print self._data['sessions']
        
        def get_session_path(self, session):
                sessions = self._data['sessions']
                return sessions[session]['path']
        
        def get_user(self):
                return self._data['user']
        
        def set_user(self, username):
                self._data['user'] = username
        
        def get_collab_ip(self, session, collab):
                sessions = self._data['sessions']
                return sessions[session]['collabs'][collab]['ip']
        
        def close(self):
                self._data.close()
        
        def get_sessions(self):
                sessions = self._data['sessions']
                sess = sessions.keys()
                sess.sort()
                return sess
        
        def get_collabs(self, session):
                if session:
                        sessions = self._data['sessions']
                        collabs = sessions[session]['collabs'].keys()
                        collabs.sort()
                        return collabs
                else:
                        return []
        
        def get_snaps(self, session, collab):
                if session and collab:
                        sessions = self._data['sessions']
                        snaps = sessions[session]['collabs'][collab]['snaps']
                        snaps.sort()
                        return snaps
                else:
                        return []
        
        def get_sounds(self, session, collab):
                if session and collab:
                        sessions = self._data['sessions']
                        sounds = sessions[session]['collabs'][self._data['user']]['sounds']
                        sounds.sort()
                        return sounds
                else:
                        return []
                
        def _scan_snapshots(self, session):
                snaps = []
                files = os.listdir(session)
                pattern = re.compile(r'\.ardour$')
                for file in files:
                        if pattern.search(file):
                                snaps.append(file[0:-7])
                                print file[0:-7]
                return snaps
        
        def _scan_sounds(self, session):
                sessions = self._data['sessions']

                sounds = []
                if v2paths:
                        print session
                        print os.path.join (sessions[session]['path'],'interchange', session, 'audiofiles')
                        files = os.listdir(os.path.join (sessions[session]['path'],'interchange', session, 'audiofiles'))
                else:
                        files = os.listdir(os.path.join (session,'sounds'))
                pattern = re.compile(r'\.peak$')
                for file in files:
                        if not pattern.search(file):
                                sounds.append(file)
                return sounds
        
        def __init__(self, *args):
                self._data = shelve.open(os.path.expanduser('~/.session_exchange'), 'c')
                self.port = 8970
                self.debug_mode = False
                if len(self._data.keys()) < 1:
                        self._data['sessions'] = {}
                        self._data['user'] = ''
                
                self._collabs = {}

from twisted.protocols.basic import FileSender
class FileSenderLimited(FileSender):
        def beginFileTransfer(self, file, consumer, limit, transform = None):
                self.file = file
                self.consumer = consumer
                self.CHUNK_SIZE = limit
                self.transform = transform
                
                self.consumer.registerProducer(self, False)
                self.deferred = defer.Deferred()
                return self.deferred
        
        def resumeProducing(self):
                chunk = ''
                chunk = self.file.read(self.CHUNK_SIZE)
                
                if self.transform:
                        chunk = self.transform(chunk)

                self.consumer.write(chunk)
                self.lastSent = chunk[-1]
                self.file = None
                self.consumer.unregisterProducer()
                self.deferred.callback(self.lastSent)
                self.deferred = None

from twisted.protocols.basic import LineReceiver
class ExchangeServer (LineReceiver):
        def __init__(self):
                self.state = "IDLE"
        
        def error(self, message):
                self.sendLine("ERROR")
                self.sendLine(message)
                self.transport.loseConnection()
        
        def connectionLost(self, reason):
                print "server: connection lost: ", reason
        
        def connectionMade(self):
                print "server: connection made"
        
        def lineReceived(self, data):
                print "server: ", data
                
                if self.state == "SESSION":
                        if g_data.get_sessions().count(data):
                                self.session_name = data
                                self.state = "IDLE"
                                self.sendLine("OK")
                        else:
                                self.error(data + " doesn't exist on server")
                elif self.state == "SNAPSHOT":
                        if g_data.get_snaps(self.session_name, g_data.get_user()).count(data):
                                filename = g_data.get_session_path(self.session_name)+data+'.ardour'
                                print filename
                                self.sendLine(str(os.stat(filename).st_size))
                                self.sendLine("OK")
                                self.file = open(filename, 'r')
                                file_sender = FileSender()
                                cb = file_sender.beginFileTransfer(self.file, self.transport)
                                cb.addCallback(self.file_done)
                        else:
                                self.error("snapshot: " + data + " doesn't exist on server")
                elif self.state == "SOUNDFILE" or self.state == "SOUNDFILE_HEADER":
                        if g_data.get_sounds(self.session_name, g_data.get_user()).count(data):
                                filename = g_data.get_session_path(self.session_name)+"/interchange/"+self.session_name+"/audiofiles/"+data
                                print filename
                                if self.state == "SOUNDFILE":
                                        self.sendLine(str(os.stat(filename).st_size))
                                else:        #SOUNDFILE_HEADER
                                        header_size = get_header_size(filename)
                                        if header_size:
                                                self.sendLine(str(header_size))
                                        else:
                                                self.error('soundfile: ' + data + 'doesn\'t have "data" chunk')
                                self.sendLine("OK")
                                self.file = open(filename, 'r')
                                if self.state == "SOUNDFILE":
                                        file_sender = FileSender()
                                        cb = file_sender.beginFileTransfer(self.file, self.transport)
                                else:        # SOUNDFILE_HEADER
                                        file_sender = FileSenderLimited()
                                        cb = file_sender.beginFileTransfer(self.file, self.transport, header_size)
                                cb.addCallback(self.file_done)
                        else:
                                self.error("soundfile: " + data + "doesn't exist on server")
                elif self.state == "SOUNDFILE_SIZE":
                        if g_data.get_sounds(self.session_name, g_data.get_user()).count(data):
                                filename = g_data.get_session_path(self.session_name)+"/sounds/"+data
                                print filename
                                self.sendLine(str(os.stat(filename).st_size))
                                self.state = "IDLE"
                elif data == "SESSION":
                        self.state = "SESSION"
                elif data == "SNAPS":
                        self.state = "SNAPS"
                        for snap in g_data.get_snaps(self.session_name, g_data.get_user()):
                                self.sendLine(snap)
                        self.sendLine("OK")
                        self.state = "IDLE"
                elif data == "SNAPSHOT":
                        self.state = "SNAPSHOT"
                elif data == "SOUNDFILE":
                        self.state = "SOUNDFILE"
                elif data == "SOUNDFILE_HEADER":
                        self.state = "SOUNDFILE_HEADER"
                elif data == "SOUNDFILE_SIZE":
                        self.state = "SOUNDFILE_SIZE"
        
        def file_done(self, data):
                print "server: file done"
                self.file.close()
                self.state = "IDLE"
        
class ExchangeServerFactory(protocol.ServerFactory):
        protocol = ExchangeServer
        
        def __init__(self):
                pass

class ExchangeClient (LineReceiver):
        def __init__(self, session_name, collab_name, snap_name, debug_mode):
                self.session_name = session_name
                self.collab_name = collab_name
                self.snap_name = snap_name
                self.debug_mode = debug_mode
                self.state = "IDLE"
        
        def connectionLost(self, reason):
                g_display.show_status("Connection lost")
        
        def connectionMade(self):
                g_display.show_status("Connection made")
                self.state = "SESSION"
                self.sendLine("SESSION")
                self.sendLine(self.session_name)
        
        def rawDataReceived(self, data):
                self.file.write(data)
                self.received += len(data)
                print self.received, self.filesize
                if self.received >= self.filesize:
                        self.setLineMode()
                        self.file.close()
                        g_data.rescan_session(self.session_name)
                        if self.state == "SNAPSHOT":
                                self.sounds = get_sound_list(self.filename)
                                if len(self.sounds):
                                        self.sound_index = 0
                                        if self.debug_mode:
                                                self.state = "SOUNDFILE_HEADER"
                                                self.sendLine("SOUNDFILE_HEADER")
                                        else:
                                                self.state = "SOUNDFILE"
                                                self.sendLine("SOUNDFILE")
                                        self.sendLine(self.sounds[self.sound_index])
                                else:
                                        self.transport.loseConnection()
                        elif self.state == "SOUNDFILE":
                                self.sound_index += 1
                                if self.sound_index > len(self.sounds)-1:
                                        self.transport.loseConnection()
                                else:
                                        self.sendLine("SOUNDFILE")
                                        self.sendLine(self.sounds[self.sound_index])
                        elif self.state == "SOUNDFILE_HEADER":
                                self.state = "SOUNDFILE_SIZE"
                                self.sendLine("SOUNDFILE_SIZE")
                                self.sendLine(self.sounds[self.sound_index])
        
        def lineReceived(self, data):
                print "client: ", data
                
                if data == "ERROR":
                        self.state = "ERROR"
                elif data == "OK":
                        if self.state == "SESSION":
                                if self.snap_name:
                                        self.state = "SNAPSHOT"
                                        self.sendLine("SNAPSHOT")
                                        self.sendLine(self.snap_name)
                                else:
                                        self.state = "SNAPS"
                                        self.sendLine("SNAPS")
                        elif self.state == "SNAPS":
                                self.transport.loseConnection()
                        elif self.state == "SNAPSHOT":
                                self.setRawMode()
                                self.filename = g_data.get_session_path(self.session_name)+'/'+self.snap_name+'.ardour'
                                self.file = open(self.filename, 'w')
                                self.received = 0
                        elif self.state == "SOUNDFILE" or self.state == "SOUNDFILE_HEADER":
                                self.setRawMode()
                                self.filename = g_data.get_session_path(self.session_name)+"/interchange/"+self.session_name+"/audiofiles/"+self.sounds[self.sound_index]
                                self.file = open(self.filename, 'w')
                                self.received = 0
                elif self.state == "ERROR":
                        raise_error(data, g_display.window)
                elif self.state == "SNAPS":
                        g_data.add_snap(self.session_name, self.collab_name, data)
                elif self.state == "SNAPSHOT":
                        self.filesize = int(data)
                elif self.state == "SOUNDFILE":
                        self.filesize = int(data)
                elif self.state == "SOUNDFILE_HEADER":
                        self.filesize = int(data)
                elif self.state == "SOUNDFILE_SIZE":
                        append_empty_data(self.filename, int(data))
                        self.sound_index += 1
                        if self.sound_index > len(self.sounds)-1:
                                self.transport.loseConnection()
                        else:
                                self.state = "SOUNDFILE_HEADER"
                                self.sendLine("SOUNDFILE_HEADER")
                                self.sendLine(self.sounds[self.sound_index])

class ExchangeClientFactory(protocol.ClientFactory):
        def buildProtocol(self, addr):
                return ExchangeClient(self.session_name, self.collab_name, self.snap_name, self.debug_mode)
        
        def clientConnectionFailed(self, connector, reason):
                raise_error('Connection failed: ' + reason.getErrorMessage(), g_display.window)
                g_display.show_status('Connection failed')
        
        def __init__(self, session_name, collab_name, snap_name, debug_mode):
                self.session_name = session_name
                self.collab_name = collab_name
                self.snap_name = snap_name
                self.debug_mode = debug_mode

class HelperWin(object):
        def delete_me(self, window):
                self = 0

class Preferences(HelperWin):
        def __init__(self):
                self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
                self.window.set_title('Preferences')
                self.window.connect('destroy', self.delete_me)
                self.window.set_position(gtk.WIN_POS_MOUSE)
                
                main_box = gtk.VBox()
                self.window.add(main_box)
                
                hbox1 = gtk.HBox()
                label1 = gtk.Label("User")
                self.user = gtk.Entry()
                self.user.set_text(g_data.get_user())
                hbox1.pack_start(label1)
                hbox1.pack_start(self.user)
                main_box.pack_start(hbox1)
                
                ok_btn = gtk.Button("Ok")
                ok_btn.connect('clicked', self.ok_clicked)
                main_box.pack_start(ok_btn)
                
                self.window.show_all()
                
        def ok_clicked(self, btn):
                g_data.set_user(self.user.get_text())
                self.window.hide_all()
                
        def show_all(self):
                self.window.show_all()

class AddCollaborator(HelperWin):
        def __init__(self, session):
                self.session_name = session
                
                self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
                self.window.set_title('Fetch Session')
                self.window.connect('destroy', self.delete_me)
                self.window.set_position(gtk.WIN_POS_MOUSE)
                
                main_box = gtk.VBox()
                self.window.add(main_box)
                
                hbox0 = gtk.HBox()
                label0 = gtk.Label("Collaborator")
                self.collab = gtk.Entry()
                self.collab.connect('key-release-event', self.key_press)
                hbox0.pack_start(label0)
                hbox0.pack_start(self.collab)
                main_box.pack_start(hbox0)
                
                hbox1 = gtk.HBox()
                label1 = gtk.Label("IP Address")
                self.address = gtk.Entry()
                self.address.connect('key-release-event', self.key_press)
                hbox1.pack_start(label1)
                hbox1.pack_start(self.address)
                main_box.pack_start(hbox1)
                
                hbox2 = gtk.HBox()
                label2 = gtk.Label("Port Number")
                self.port = gtk.Entry()
                self.port.connect('key-release-event', self.key_press)
                self.port.set_text(str(g_data.port))
                hbox2.pack_start(label2)
                hbox2.pack_start(self.port)
                main_box.pack_start(hbox2)
                
                hbox3 = gtk.HBox()
                label3 = gtk.Label("Username")
                label3.set_sensitive(False)
                self.username = gtk.Entry()
                self.username.set_sensitive(False)
                hbox3.pack_start(label3)
                hbox3.pack_start(self.username)
                main_box.pack_start(hbox3)
                
                hbox4 = gtk.HBox()
                label4 = gtk.Label("Password")
                label4.set_sensitive(False)
                self.password = gtk.Entry()
                self.password.set_sensitive(False)
                hbox4.pack_start(label4)
                hbox4.pack_start(self.password)
                main_box.pack_start(hbox4)
                
                self.ok_btn = gtk.Button(gtk.STOCK_OK)
                self.ok_btn.set_use_stock(True)
                self.ok_btn.connect('clicked', self.ok_clicked)
                self.ok_btn.set_sensitive(False)
                main_box.pack_start(self.ok_btn)
                
                self.window.show_all()
        
        def key_press(self, event, data):
                if self.collab.get_text() and self.address.get_text() and self.port.get_text():
                        self.ok_btn.set_sensitive(True)
                else:
                        self.ok_btn.set_sensitive(False)
                return True
        
        def ok_clicked(self, btn):
                self.window.hide_all()
                g_data.add_collab(self.session_name, self.collab.get_text(), self.address.get_text(), int(self.port.get_text()))
                self.collab.set_text('')
                self.address.set_text('')
                self.port.set_text('')
                self.username.set_text('')
                self.password.set_text('')
        
        def show_all(self):
                self.window.show_all()

class ArdourShareWindow(object):
        def menuitem_cb(self, window, action, widget):
                print self, window, action, widget
        
        def add_collaborator_cb(self, window, action, widget):
                if self.session:
                        self.add_session = AddCollaborator(self.session)
        
        def fetch_snapshot_cb(self, window, action, widget):
                if self.session and self.collab and self.collab != g_data.get_user():
                        client = ExchangeClientFactory(self.session, self.collab, self.snap, g_data.debug_mode)
                        reactor.connectTCP(g_data.get_collab_ip(self.session, self.collab), g_data.port, client)
        
        def preferences_cb(self, window, action, widget):
                self.preferences = Preferences()
        
        def add_session_ok_file_btn_clicked(self, w):
                filename = self.file_sel.get_filename()
                if filename.endswith(".ardour"):
                        g_data.add_session(filename[0:filename.rfind("/")+1])
                        self.update_session_view()
                else:
                        raise_error("Not an Ardour session", self.window)
                self.file_sel.destroy()
        
        def add_session_cb(self, window, action, widget):
                if g_data.get_user():
                        self.file_sel = gtk.FileSelection("Add Session...")
                        self.file_sel.ok_button.connect("clicked", self.add_session_ok_file_btn_clicked)
                        self.file_sel.cancel_button.connect("clicked", lambda w: self.file_sel.destroy())
                        self.file_sel.connect("destroy", lambda w: self.file_sel.destroy())
                        self.file_sel.show()
                else:
                        raise_error("Set the user name in the preferences first", self.window)
        
        def create_session_cb(self, window, action, widget):
                if g_data.get_user():
                        self.file_sel = gtk.FileSelection("Create Session...")
                        self.file_sel.ok_button.connect("clicked", self.create_file_ok_btn_clicked)
                        self.file_sel.cancel_button.connect("clicked", lambda w: self.file_sel.destroy())
                        self.file_sel.connect("destroy", lambda w: self.file_sel.destroy())
                        self.file_sel.show()
                else:
                        raise_error("Set the user name in the preferences first", self.window)
        
        def create_file_ok_btn_clicked(self, w):
                filename = self.file_sel.get_filename()
                if len(filename) > 0:
                        g_data.create_session(filename)
                        self.update_session_view()
                else:
                        raise_error("Not an Ardour session", self.window)
                self.file_sel.destroy()
        
        def update_session_view(self):
                self.session_model.clear()
                for session in g_data.get_sessions():
                        self.session_model.set(self.session_model.append(), 0, session)
        
        def update_collab_view(self):
                self.collab_model.clear()
                for collab in g_data.get_collabs(self.session):
                        self.collab_model.set(self.collab_model.append(), 0, collab)
        
        def update_snap_view(self):
                self.snap_model.clear()
                for snap in g_data.get_snaps(self.session, self.collab):
                        self.snap_model.set(self.snap_model.append(), 0, snap)
        
        def cb_session_selection_changed(self, selection_object):
                selected = []
                selection_object.selected_foreach(lambda model, path, iter, sel = selected: sel.append(path))
                for x in selected:
                        self.session = self.session_model[x][0]
                self.selected_type = "session"
                self.update_collab_view()
        
        def cb_collab_selection_changed(self, selection_object):
                selected = []
                selection_object.selected_foreach(lambda model, path, iter, sel = selected: sel.append(path))
                for x in selected:
                        self.collab = self.collab_model[x][0]
                self.selected_type = "collab"
                self.update_snap_view()
        
        def cb_snap_selection_changed(self, selection_object):
                selected = []
                selection_object.selected_foreach(lambda model, path, iter, sel = selected: sel.append(path))
                for x in selected:
                        self.snap = self.snap_model[x][0]
                self.selected_type = "snap"
        
        def delete_cb(self, window, action, widget):
                if self.selected_type == "session":
                        g_data.delete_session(self.session)
                        self.session = ""
                        self.collab = ""
                        self.snap = ""
                elif self.selected_type == "collab":
                        g_data.delete_collab(self.session, self.collab)
                        self.collab = ""
                        self.snap = ""
                elif self.selected_type == "snap":
                        g_data.delete_snap(self.session, self.collab, self.snap)
                        self.snap = ""
                
                self.update_session_view()
                self.update_collab_view()
                self.update_snap_view()
                self.selected_type = ""
                
        def show_status(self, text):
                mid = self.status_bar.push(self._status_cid, text)
                if self._status_mid:
                        self.status_bar.remove(self._status_cid, self._status_mid)
                self._status_mid = mid
        
        def __init__(self):
                self.selected_type = ""
                self.session = ""
                self.collab = g_data.get_user()
                self.snap = ""
                
                self.preferences = 0
                self.add_collab = 0
                self.add_session = 0
                
                self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
                self.window.set_title('Session Exchange')
                self.window.set_size_request(400, 200)
                self.window.connect('destroy', lambda win: gtk.main_quit())
                self.window.set_position(gtk.WIN_POS_MOUSE)
                
                accel_group = gtk.AccelGroup()
                self.window.add_accel_group(accel_group)
                
                main_box = gtk.VBox()
                self.window.add(main_box)
                
                menu_items = (
                        ('/_File',            None,         None,             0, '<Branch>'),
                        ('/File/_Add Session...','<control>A', self.add_session_cb, 0, ''),
                        ('/File/Create _Session...', '<control>S', self.create_session_cb, 0, ''),
                        ('/File/sep1',        None,         None,             0, '<Separator>'),
                        ('/File/_Quit',       '<control>Q', gtk.main_quit,     0, '<StockItem>', gtk.STOCK_QUIT),
                        ('/_Edit',            None,         None,             0, '<Branch>' ),
                        ('/Edit/Cu_t',        '<control>X', self.menuitem_cb, 0, '<StockItem>', gtk.STOCK_CUT),
                        ('/Edit/_Copy',       '<control>C', self.menuitem_cb, 0, '<StockItem>', gtk.STOCK_COPY),
                        ('/Edit/_Paste',      '<control>V', self.menuitem_cb, 0, '<StockItem>', gtk.STOCK_PASTE),
                        ('/Edit/_Delete',     None,         self.delete_cb, 0, '<StockItem>', gtk.STOCK_DELETE),
                        ('/Edit/sep1',        None,         None,             0, '<Separator>'),
                        ('/Edit/Add Colla_borator...','<control>B', self.add_collaborator_cb,0,''),
                        ('/Edit/_Fetch Snapshot','<control>F', self.fetch_snapshot_cb,0,''),
                        ('/Edit/sep1',        None,         None,             0, '<Separator>'),
                        ('/Edit/_Preferences...','<control>P', self.preferences_cb, 0, '')
                )
                
                #need to hold a reference to the item_factory or the menubar will disappear.
                self.item_factory = gtk.ItemFactory(gtk.MenuBar, '<main>', accel_group)
                self.item_factory.create_items(menu_items, self.window)
                main_box.pack_start(self.item_factory.get_widget('<main>'), False)
                
                pane1 = gtk.HPaned()
                pane2 = gtk.HPaned()
                pane1.pack2(pane2, True, False)
                
                scroll1 = gtk.ScrolledWindow()
                scroll1.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
                pane1.pack1(scroll1, True, False)
                scroll2 = gtk.ScrolledWindow()
                scroll2.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
                pane2.pack1(scroll2, True, False)
                scroll3 = gtk.ScrolledWindow()
                scroll3.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
                pane2.pack2(scroll3, True, False)
                
                self.session_model = gtk.ListStore(gobject.TYPE_STRING)
                view1 = gtk.TreeView(self.session_model)
                column1 = gtk.TreeViewColumn('Sessions', gtk.CellRendererText(), text=0)
                view1.append_column(column1)
                self.session_selection = view1.get_selection()
                self.session_selection.connect("changed", self.cb_session_selection_changed)
                scroll1.add(view1)
                
                self.update_session_view()
                
                self.collab_model = gtk.ListStore(gobject.TYPE_STRING)
                view2 = gtk.TreeView(self.collab_model)
                column2 = gtk.TreeViewColumn('Collaborators', gtk.CellRendererText(), text=0)
                view2.append_column(column2)
                self.collab_selection = view2.get_selection()
                self.collab_selection.connect("changed", self.cb_collab_selection_changed)
                scroll2.add(view2)
                
                self.snap_model = gtk.ListStore(gobject.TYPE_STRING)
                view3 = gtk.TreeView(self.snap_model)
                column3 = gtk.TreeViewColumn('Snapshots', gtk.CellRendererText(), text=0)
                view3.append_column(column3)
                self.snap_selection = view3.get_selection()
                self.snap_selection.connect("changed", self.cb_snap_selection_changed)
                scroll3.add(view3)
                
                main_box.pack_start(pane1, True, True)
                
                self.status_bar = gtk.Statusbar()
                main_box.pack_start(self.status_bar, False)
                self._status_cid = self.status_bar.get_context_id('display')
                self._status_mid = ''
                
                self.window.show_all()

def print_help():
        print """
        -h, --help
        -n, --no-server          Only act as a client
        -p, --port <port number> Defaults to 8970
        -d, --debug              Infers audio files.  For debugging Ardour.
        -v, --version            Version
        """
        sys.exit(2)

def main():
        try:
                opts, args = getopt.getopt(sys.argv[1:], "hp:ndv", ["help", "port=", "no-server", "debug", "version"])
        except getopt.GetoptError:
                print_help()
        
        server = True
        for o, a in opts:
                if o in ("-h", "--help"):
                        print_help()
                if o in ("-d", "--debug"):
                        g_display.window.set_title('Session Exchange: Debug Mode')
                        g_data.debug_mode = True
                if o in ("-p", "--port"):
                        g_data.port = int(a)
                if o in ("-n", "--no-server"):
                        server = False
                if o in ("-v", "--version"):
                        print VERSION
                        sys.exit(2)
        
        if (server):
                try:
                        reactor.listenTCP(g_data.port, ExchangeServerFactory())
                except twisted.internet.error.CannotListenError:
                        print "Can not listen on a port number under 1024 unless run as root"
                        sys.exit(2)

        reactor.run()

        g_data.close()

# global objects
g_data = Data()
g_display = ArdourShareWindow()

if __name__ == '__main__':
        main()
