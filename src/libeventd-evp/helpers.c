/*
 * libeventd-evp - Library to send EVENT protocol messages
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
 *
 * This file is part of eventd.
 *
 * eventd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * eventd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <glib.h>
#include <gio/gio.h>
#if HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif /* HAVE_GIO_UNIX */

#include <libeventd-event.h>

#include <libeventd-evp.h>


GSocketConnectable *
libeventd_evp_get_address(const gchar *hostname, guint16 port)
{
    GSocketConnectable *address = NULL;

#if HAVE_GIO_UNIX
    gchar *path = NULL;
    if ( g_strcmp0(hostname, "localhost") == 0 )
        path = g_build_filename(g_get_user_runtime_dir(), PACKAGE_NAME, UNIX_SOCKET, NULL);
    else if ( g_path_is_absolute(hostname) )
        path = g_strdup(hostname);
    if ( ( path != NULL ) && g_file_test(path, G_FILE_TEST_EXISTS) && ( ! g_file_test(path, G_FILE_TEST_IS_DIR|G_FILE_TEST_IS_REGULAR) ) )
        address = G_SOCKET_CONNECTABLE(g_unix_socket_address_new(path));
    g_free(path);
#endif /* HAVE_GIO_UNIX */

    if ( address == NULL )
        address = g_network_address_new(hostname, ( port > 0 ) ? port : DEFAULT_BIND_PORT);

    return address;
}
