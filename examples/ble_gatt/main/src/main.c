/*
 * BLE GATT peripheral on the board's AIC8800D80, through BlueZ over D-Bus.
 *
 *   ble_gatt
 *
 * Advertises as "COVE610" with one service holding one readable
 * characteristic whose value is the device name. Prints when a phone
 * connects or disconnects. Start BlueZ first with "ble-service start".
 * BlueZ drops the advertisement and the service when the program exits.
 */
#include <stdio.h>
#include <string.h>

#include <gio/gio.h>
#include <glib-unix.h>

#define BLUEZ "org.bluez"
#define ADAPTER_PATH "/org/bluez/hci0"
#define APP_PATH "/com/v610/ble_gatt"
#define SERVICE_PATH APP_PATH "/service0"
#define CHAR_PATH SERVICE_PATH "/char0"
#define ADV_PATH APP_PATH "/adv0"

#define SERVICE_UUID "a6f1c000-04f1-4d9b-a002-c06100000001"
#define CHAR_UUID "a6f1c000-04f1-4d9b-a002-c06100000002"
#define DEVICE_NAME "COVE610"

/* Interface order: object manager, service, characteristic, advertisement. */
static const char g_xml[] =
    "<node>"
    " <interface name='org.freedesktop.DBus.ObjectManager'>"
    "  <method name='GetManagedObjects'>"
    "   <arg name='objects' type='a{oa{sa{sv}}}' direction='out'/>"
    "  </method>"
    " </interface>"
    " <interface name='org.bluez.GattService1'>"
    "  <property name='UUID' type='s' access='read'/>"
    "  <property name='Primary' type='b' access='read'/>"
    " </interface>"
    " <interface name='org.bluez.GattCharacteristic1'>"
    "  <method name='ReadValue'>"
    "   <arg name='options' type='a{sv}' direction='in'/>"
    "   <arg name='value' type='ay' direction='out'/>"
    "  </method>"
    "  <property name='UUID' type='s' access='read'/>"
    "  <property name='Service' type='o' access='read'/>"
    "  <property name='Flags' type='as' access='read'/>"
    " </interface>"
    " <interface name='org.bluez.LEAdvertisement1'>"
    "  <method name='Release'/>"
    "  <property name='Type' type='s' access='read'/>"
    "  <property name='ServiceUUIDs' type='as' access='read'/>"
    "  <property name='LocalName' type='s' access='read'/>"
    "  <property name='Discoverable' type='b' access='read'/>"
    " </interface>"
    "</node>";

static GMainLoop *g_loop;
static GDBusConnection *g_bus;

static GVariant *get_property(GDBusConnection *connection, const char *sender,
    const char *path, const char *interface, const char *name, GError **error,
    gpointer user_data)
{
    static const char *const flags[] = {"read", NULL};
    static const char *const uuids[] = {SERVICE_UUID, NULL};

    (void)connection;
    (void)sender;
    (void)path;
    (void)error;
    (void)user_data;
    if (strcmp(interface, "org.bluez.GattService1") == 0) {
        if (strcmp(name, "UUID") == 0) {
            return g_variant_new_string(SERVICE_UUID);
        }
        if (strcmp(name, "Primary") == 0) {
            return g_variant_new_boolean(TRUE);
        }
    } else if (strcmp(interface, "org.bluez.GattCharacteristic1") == 0) {
        if (strcmp(name, "UUID") == 0) {
            return g_variant_new_string(CHAR_UUID);
        }
        if (strcmp(name, "Service") == 0) {
            return g_variant_new_object_path(SERVICE_PATH);
        }
        if (strcmp(name, "Flags") == 0) {
            return g_variant_new_strv(flags, -1);
        }
    } else {
        if (strcmp(name, "Type") == 0) {
            return g_variant_new_string("peripheral");
        }
        if (strcmp(name, "ServiceUUIDs") == 0) {
            return g_variant_new_strv(uuids, -1);
        }
        if (strcmp(name, "LocalName") == 0) {
            return g_variant_new_string(DEVICE_NAME);
        }
        if (strcmp(name, "Discoverable") == 0) {
            return g_variant_new_boolean(TRUE);
        }
    }
    return NULL;
}

/* GetManagedObjects: the service and characteristic with all their properties. */
static GVariant *managed_objects(void)
{
    static const char *const flags[] = {"read", NULL};
    GVariantBuilder objects;

    g_variant_builder_init(&objects, G_VARIANT_TYPE("a{oa{sa{sv}}}"));
    g_variant_builder_add_parsed(&objects,
        "{%o, {'org.bluez.GattService1': {'UUID': <%s>, 'Primary': <true>}}}",
        SERVICE_PATH, SERVICE_UUID);
    g_variant_builder_add_parsed(&objects,
        "{%o, {'org.bluez.GattCharacteristic1': "
        "{'UUID': <%s>, 'Service': <%o>, 'Flags': <%^as>}}}",
        CHAR_PATH, CHAR_UUID, SERVICE_PATH, flags);
    return g_variant_new("(@a{oa{sa{sv}}})", g_variant_builder_end(&objects));
}

static void method_call(GDBusConnection *connection, const char *sender,
    const char *path, const char *interface, const char *method,
    GVariant *parameters, GDBusMethodInvocation *invocation, gpointer user_data)
{
    (void)connection;
    (void)sender;
    (void)path;
    (void)interface;
    (void)parameters;
    (void)user_data;
    if (strcmp(method, "GetManagedObjects") == 0) {
        g_dbus_method_invocation_return_value(invocation, managed_objects());
    } else if (strcmp(method, "ReadValue") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(@ay)",
            g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, DEVICE_NAME,
                strlen(DEVICE_NAME), 1)));
    } else {
        /* Release: BlueZ dropped the advertisement. */
        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static const GDBusInterfaceVTable g_vtable = {
    .method_call = method_call,
    .get_property = get_property,
};

static void device_changed(GDBusConnection *connection, const char *sender,
    const char *path, const char *interface, const char *signal,
    GVariant *parameters, gpointer user_data)
{
    const char *changed_interface;
    GVariant *changed;
    gboolean connected;

    (void)connection;
    (void)sender;
    (void)interface;
    (void)signal;
    (void)user_data;
    g_variant_get(parameters, "(&s@a{sv}*)", &changed_interface, &changed, NULL);
    if (strcmp(changed_interface, "org.bluez.Device1") == 0 &&
        g_variant_lookup(changed, "Connected", "b", &connected)) {
        printf("%s %s\n", path, connected ? "connected" : "disconnected");
        fflush(stdout);
    }
    g_variant_unref(changed);
}

static void advertisement_registered(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);

    (void)user_data;
    if (reply == NULL) {
        fprintf(stderr, "RegisterAdvertisement: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(g_loop);
        return;
    }
    g_variant_unref(reply);
    printf("advertising as %s, scan and connect with nRF Connect or LightBlue, Ctrl-C to stop\n",
        DEVICE_NAME);
    fflush(stdout);
}

static void application_registered(GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &error);

    (void)user_data;
    if (reply == NULL) {
        fprintf(stderr, "RegisterApplication: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(g_loop);
        return;
    }
    g_variant_unref(reply);
    g_dbus_connection_call(g_bus, BLUEZ, ADAPTER_PATH, "org.bluez.LEAdvertisingManager1",
        "RegisterAdvertisement", g_variant_new("(o@a{sv})", ADV_PATH,
            g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)),
        NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, advertisement_registered, NULL);
}

static gboolean on_signal(gpointer user_data)
{
    (void)user_data;
    g_main_loop_quit(g_loop);
    return G_SOURCE_REMOVE;
}

int main(void)
{
    static const char *const paths[] = {APP_PATH, SERVICE_PATH, CHAR_PATH, ADV_PATH};
    GError *error = NULL;
    GDBusNodeInfo *node;
    GVariant *reply;

    g_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (g_bus == NULL) {
        fprintf(stderr, "system D-Bus: %s (run \"ble-service start\" first)\n", error->message);
        return 1;
    }

    /* Power on the adapter; this also fails when bluetoothd is not running. */
    reply = g_dbus_connection_call_sync(g_bus, BLUEZ, ADAPTER_PATH,
        "org.freedesktop.DBus.Properties", "Set",
        g_variant_new("(ssv)", "org.bluez.Adapter1", "Powered", g_variant_new_boolean(TRUE)),
        NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    if (reply == NULL) {
        fprintf(stderr, "power on %s: %s (run \"ble-service start\" first)\n",
            ADAPTER_PATH, error->message);
        return 1;
    }
    g_variant_unref(reply);

    node = g_dbus_node_info_new_for_xml(g_xml, NULL);
    for (int i = 0; i < 4; i++) {
        if (g_dbus_connection_register_object(g_bus, paths[i], node->interfaces[i],
            &g_vtable, NULL, NULL, &error) == 0) {
            fprintf(stderr, "register %s: %s\n", paths[i], error->message);
            return 1;
        }
    }
    g_dbus_connection_signal_subscribe(g_bus, BLUEZ, "org.freedesktop.DBus.Properties",
        "PropertiesChanged", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, device_changed, NULL, NULL);

    /*
     * Asynchronous, because BlueZ calls GetManagedObjects back on this
     * connection before it replies, and that call needs the main loop.
     */
    g_dbus_connection_call(g_bus, BLUEZ, ADAPTER_PATH, "org.bluez.GattManager1",
        "RegisterApplication", g_variant_new("(o@a{sv})", APP_PATH,
            g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)),
        NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, application_registered, NULL);

    g_loop = g_main_loop_new(NULL, FALSE);
    g_unix_signal_add(SIGINT, on_signal, NULL);
    g_unix_signal_add(SIGTERM, on_signal, NULL);
    g_main_loop_run(g_loop);
    puts("stopped");
    return 0;
}
