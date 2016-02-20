#ifndef TILDA_DBUS_H
#define TILDA_DBUS_H
G_BEGIN_DECLS

gboolean *tilda_dbus_server_start(tilda_window *tw, gint instance);
// void tilda_dbus_server_connect_handlers(GDBusServer *server, tilda_window *tw);
GDBusConnection *tilda_dbus_peer_connect(gint instance);
gboolean *tilda_dbus_peer_send_command(GDBusConnection *connection, gchar *command);

G_END_DECLS
#endif
