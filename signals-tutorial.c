#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <string.h> /*Required for strtok function*/
#include <unistd.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

void print_network_manager_state(guint state)
{
    switch (state) {
        case 2:
            g_message("NetworkManager Connecting");
            break;
        case 3:
            g_message("NetworkManager Connected");
            break;
        case 4:
            g_message("NetworkManager Disconnected");
            break;
    }
}

void read_network_manager_state_change(DBusMessage *msg)
{
    DBusError error;
    dbus_error_init(&error);

    guint32 state = 0;

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_UINT32, &state,
                DBUS_TYPE_INVALID)) {
        g_error("Cannot read NetworkManager state change message, cause: %s", error.message);
        dbus_error_free(&error);
        return;
    }
    print_network_manager_state(state);
}

DBusHandlerResult signal_filter(DBusConnection *connection, DBusMessage *msg,
        void *user_data)
{
    /* You can use Message API to get other bus informations */
    /*
       g_message("Sender='%s' path='%s' interface='%s' member='%s'",
       dbus_message_get_sender(msg),
       dbus_message_get_path(msg),
       dbus_message_get_interface(msg),
       dbus_message_get_member(msg));
     */

    if (dbus_message_is_signal(msg, "org.freedesktop.NetworkManager",
                "StateChange")) {
        read_network_manager_state_change(msg);
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int filter_example()
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    DBusError error;

    dbus_error_init(&error);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);

    if (dbus_error_is_set(&error)) {
        g_error("Cannot get System BUS connection: %s", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }
    dbus_connection_setup_with_g_main(conn, NULL);

    char *rule = "type='signal',interface='org.freedesktop.NetworkManager'";
    g_message("Signal match rule: %s", rule);
    dbus_bus_add_match(conn, rule, &error);

    if (dbus_error_is_set(&error)) {
        g_error("Cannot add D-BUS match rule, cause: %s", error.message);
        dbus_error_free(&error);
        return EXIT_FAILURE;
    }

    g_message("Listening to D-BUS signals using a connection filter");
    dbus_connection_add_filter(conn, signal_filter, NULL, NULL);

    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}

void state_changed_callback(DBusGProxy *proxy, guint32 state,
        gpointer user_data)
{
    print_network_manager_state(state);
}

int proxy_example()
{
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    g_type_init();

    GError *error = NULL;
    DBusGConnection *conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

    if (error != NULL) {
        g_error("D-BUS Connection error: %s", error->message);
        g_error_free(error);
    }

    if (!conn) {
        g_error("D-BUS connection cannot be created");
        return EXIT_FAILURE;
    }

    DBusGProxy *proxy = dbus_g_proxy_new_for_name(conn,
            "org.freedesktop.NetworkManager",
            "/org/freedesktop/NetworkManager",
            "org.freedesktop.NetworkManager");

    if (!proxy) {
        g_error("Cannot create proxy");
        return EXIT_FAILURE;
    }

    dbus_g_proxy_add_signal(proxy, "StateChange", G_TYPE_UINT,
            G_TYPE_INVALID);

    dbus_g_proxy_connect_signal(proxy, "StateChange",
            G_CALLBACK(state_changed_callback), NULL, NULL);

    g_message("Waiting D-BUS proxy callback for signal");
    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}

/* 
   A network struct:
   contains the name (SSID?) of the network
   and the commands which should be executed on (dis)connection
 */
struct network_struct {
    gchar* network_name;
    GSList* connect_cmd;
    GSList* disconnect_cmd;
};

typedef struct network_struct network_t;

/*
   Returns a hashmap woth networks
   Each networks has a hashmap with the keys "connect" and "disconnect"
   Each keys has a list with commands to be executed
 */
int read_config_file()
{
    static const char filename[] = "/home/bottiger/.networkchange";
    FILE *file = fopen ( filename, "r" );

    // main hash where you can lookup a network_t by network name
    GHashTable* network_hash = g_hash_table_new_full(g_str_hash, g_direct_equal, NULL, g_free);
    network_t* current_network;

    if ( file != NULL )
    {
        char line [ 256 ]; /* or other suitable maximum line size */

        while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
        {
            //fputs ( line, stdout ); /* write the line */
            const char delims[] = ":";
            gchar *result;
            int i = 0;
            gchar *config_key;
            gchar *config_value;

            typedef enum { NETWORK, CONNECT, DISCONNECT, NONE } Keys;
            Keys current_key = NONE;

            result = strtok( line, delims );
            while( result != NULL ) {
                GSList* connect_list;
                GSList* disconnect_list;

                printf("%d", i);
                printf( "%s\n", g_strstrip(result) );
                printf("----------\n");
                result = strtok( NULL, delims );

                if (i == 0) {
                    config_key = g_strstrip(result);
                    if (strcmp(config_key, "network")) {
                        // create a new network network
                        current_key = NETWORK;
                    } else if (strcmp(config_key, "connect")) {
                        // connect
                        current_key = CONNECT;
                    } else if (strcmp(config_key, "disconnect")) {
                        // disconnect
                        current_key = DISCONNECT;
                    } else {
                        exit(EXIT_FAILURE);
                    }
                } else if (i == 1) {
                    config_value = g_strstrip(result);

                    switch (current_key) {
                        case NETWORK:
                            // network
                            current_network->network_name = config_value;
                            break;

                        case CONNECT:
                            // connect
                            connect_list = current_network->connect_cmd;
                            connect_list = g_slist_append(connect_list, config_value);
                            current_network->connect_cmd = connect_list;
                            break;

                        case DISCONNECT:
                            // disconnect
                            disconnect_list = current_network->disconnect_cmd;
                            disconnect_list = g_slist_append(disconnect_list, config_value);
                            current_network->disconnect_cmd = disconnect_list;
                            break;
                    }

                    current_key = NONE;
                } else {
                    exit(EXIT_FAILURE);
                }
                i++;
            }
            printf("\n%s\n", config_key);
            //g_hash_table_insert(network_hash, "hash_key", config_value);
            printf("=====\n");
            fclose ( file );
        }
    }
    else
    {
        perror ( filename ); /* why didn't the file open? */
    }

}


int main()
{
    read_config_file();
    /*
       pid_t   pid, sid;

       pid = fork();

       if (pid < 0) {
       exit(EXIT_FAILURE);
       } else if (pid > 0) {
       printf("pid: %d", pid);
       exit(EXIT_SUCCESS);
       }

       umask(0);

       sid = setsid();
       if (sid < 0) {
       exit(EXIT_FAILURE);
       }

       if ((chdir("/tmp")) < 0) {
       exit(EXIT_FAILURE);
       }

    /* Uncomment example you want to run *
    return filter_example();
    //	return proxy_example();

    exit(EXIT_SUCCESS);
     */
}
