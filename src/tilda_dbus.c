#include <tilda-config.h>

#include "tilda_window.h"
#include "debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

static void tilda_dbus_handle_method_call (
    GDBusConnection       *connection,
    const gchar           *sender,
    const gchar           *object_path,
    const gchar           *interface_name,
    const gchar           *method_name,
    GVariant              *parameters,
    GDBusMethodInvocation *invocation,
    tilda_window               *tw)
{
    if (g_strcmp0 (method_name, "addtab") == 0) {
      const gchar *command;
      gint response = 0;

      g_variant_get (parameters, "(&s)", &command);
      _tilda_window_add_tab(tw, command);
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(i)", response));
      g_free (response);
      g_print ("Client send: '%s':'%s'\n", method_name, command);
    }
}

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.tilda.GDBus.TildaInterface'>"
  "    <method name='addtab'>"
  "      <arg type='s' name='command' direction='in'/>"
  "      <arg type='i' name='response' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const GDBusInterfaceVTable interface_vtable =
{
  tilda_dbus_handle_method_call,
  NULL,
  NULL,
};


static gboolean
tild_dbus_on_new_connection (GDBusServer *server,
                   GDBusConnection *connection,
                   tilda_window *tw)
{
  guint registration_id;
  GCredentials *credentials;
  gchar *s;

  credentials = g_dbus_connection_get_peer_credentials (connection);
  if (credentials == NULL)
    s = g_strdup ("(no credentials received)");
  else
    s = g_credentials_to_string (credentials);


  g_print ("Client connected.\nPeer credentials: %s\n", s);

  g_object_ref (connection);
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/tilda/GDBus/TildaInstance",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       tw,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);

  return TRUE;
}

gboolean tilda_dbus_server_start(tilda_window *tw, gint instance) {
    introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
    g_assert (introspection_data != NULL);

    gchar *opt_address = g_strdup_printf("unix:abstract=tilda_%i", instance);
    GDBusServer *server;
    gchar *guid;
    GMainLoop *loop;
    GError *error = NULL;
    GDBusServerFlags server_flags = G_DBUS_SERVER_FLAGS_NONE;

    guid = g_dbus_generate_guid ();

    server = g_dbus_server_new_sync (opt_address,
        server_flags,
        guid,
        NULL, /* GDBusAuthObserver */
        NULL, /* GCancellable */
        &error
    );

    g_dbus_server_start (server);
    g_free (guid);

    if (server == NULL) {
        g_printerr ("Error creating server at address %s: %s\n", opt_address, error->message);
        g_error_free (error);
        return FALSE;
    }

    g_print ("Server is listening at: %s\n", g_dbus_server_get_client_address (server));
    g_signal_connect (server,
        "new-connection",
        G_CALLBACK (tild_dbus_on_new_connection),
        tw
    );
    return TRUE;
}

GDBusConnection *tilda_dbus_peer_connect(gint instance) {
    GDBusConnection *connection;
    GVariant *value;
    GError *error = NULL;
    gchar *opt_address = g_strdup_printf("unix:abstract=tilda_%i", instance);

    connection = g_dbus_connection_new_for_address_sync (opt_address,
        G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
        NULL, /* GDBusAuthObserver */
        NULL, /* GCancellable */
        &error
    );
    if (connection == NULL) {
        g_printerr ("Error connecting to D-Bus address %s: %s\n", opt_address, error->message);
        g_error_free (error);
        return connection;
    }
    return connection;
}

void *tilda_dbus_peer_send_command(GDBusConnection *connection, gchar *command) {
    GVariant *ret;
    GError *error = NULL;
    ret = g_dbus_connection_call_sync (connection,
        NULL, /* bus_name */
        "/org/tilda/GDBus/TildaInstance",
        "org.tilda.GDBus.TildaInterface",
        "addtab",
        g_variant_new ("(s)", config_getstr("command")),
        G_VARIANT_TYPE ("(i)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (ret == NULL) {
        g_printerr ("Error invoking addtab(): %s\n", error->message);
        g_error_free (error);
    } else {
        g_variant_unref(ret);
    }
}
