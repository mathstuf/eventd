/*
 * libeventc - Library to communicate with eventd
 *
 * Copyright © 2011-2012 Quentin "Sardem FF7" Glidic
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with eventd. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include <libeventd-evp.h>
#include <libeventd-event.h>

#include <libeventc.h>
#define EVENTC_CONNECTION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), EVENTC_TYPE_CONNECTION, EventcConnectionPrivate))

EVENTD_EXPORT GType eventc_connection_get_type(void);
G_DEFINE_TYPE(EventcConnection, eventc_connection, G_TYPE_OBJECT)

struct _EventcConnectionPrivate {
    GSocketConnectable *address;
    gboolean passive;
    gboolean enable_proxy;
    GError *error;
    LibeventdEvpContext* evp;
    GHashTable* events;
};

typedef struct {
    EventdEvent *event;
    gulong answered;
    gulong ended;
} EventdConnectionEventHandlers;

typedef struct {
    EventcConnection *self;
    GAsyncReadyCallback callback;
    gpointer user_data;
} EventcConnectionCallbackData;

static void _eventc_connection_close_internal(EventcConnection *self);

/**
 * eventc_get_version:
 *
 * Retrieves the runtime-version of libeventc.
 *
 * Returns: (transfer none): the version of libeventc
 */
EVENTD_EXPORT
const gchar *
eventc_get_version(void)
{
    return PACKAGE_VERSION;
}

EVENTD_EXPORT
GQuark
eventc_error_quark(void)
{
    return g_quark_from_static_string("eventc_error-quark");
}

static void
_eventc_connection_event_answered(EventcConnection *self, const gchar *answer, EventdEvent *event)
{
    EventdConnectionEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->priv->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    libeventd_evp_context_send_answered(self->priv->evp, event, answer, ( self->priv->error == NULL ) ? &self->priv->error : NULL);
}

static void
_eventc_connection_protocol_answered(gpointer data, LibeventdEvpContext *context, EventdEvent *event, const gchar *answer)
{
    EventcConnection *self = data;
    EventdConnectionEventHandlers *handlers;
    handlers = g_hash_table_lookup(self->priv->events, event);
    g_return_if_fail(handlers != NULL);

    g_signal_handler_disconnect(event, handlers->answered);
    handlers->answered = 0;
    eventd_event_answer(event, answer);
}

static void
_eventc_connection_event_ended(EventcConnection *self, EventdEventEndReason reason, EventdEvent *event)
{
    g_hash_table_remove(self->priv->events, event);
    libeventd_evp_context_send_ended(self->priv->evp, event, reason, ( self->priv->error == NULL ) ? &self->priv->error : NULL);
}

static void
_eventc_connection_protocol_ended(gpointer data, LibeventdEvpContext *context, EventdEvent *event, EventdEventEndReason reason)
{
    EventcConnection *self = data;
    g_hash_table_remove(self->priv->events, event);
    eventd_event_end(event, reason);
}

static void
_eventc_connection_protocol_bye(gpointer data, LibeventdEvpContext *context)
{
    EventcConnection *self = data;
    _eventc_connection_close_internal(self);
}

static LibeventdEvpClientInterface _eventc_connection_client_interface = {
    .event = NULL,
    .answered = _eventc_connection_protocol_answered,
    .ended = _eventc_connection_protocol_ended,

    .bye = _eventc_connection_protocol_bye
};

static void
_eventc_connection_event_handlers_free(gpointer data)
{
    EventdConnectionEventHandlers *handlers = data;

    if ( handlers->answered > 0 )
        g_signal_handler_disconnect(handlers->event, handlers->answered);
    g_signal_handler_disconnect(handlers->event, handlers->ended);

    g_object_unref(handlers->event);

    g_slice_free(EventdConnectionEventHandlers, handlers);
}

static void _eventc_connection_finalize(GObject *object);

static void
eventc_connection_class_init(EventcConnectionClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(EventcConnectionPrivate));

    eventc_connection_parent_class = g_type_class_peek_parent(klass);

    object_class->finalize = _eventc_connection_finalize;
}

static void
eventc_connection_init(EventcConnection *self)
{
    self->priv = EVENTC_CONNECTION_GET_PRIVATE(self);

    self->priv->evp = libeventd_evp_context_new(self, &_eventc_connection_client_interface);
    self->priv->events = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, _eventc_connection_event_handlers_free);
}

static void
_eventc_connection_finalize(GObject *object)
{
    EventcConnection *self = EVENTC_CONNECTION(object);

    if ( self->priv->address != NULL )
        g_object_unref(self->priv->address);

    libeventd_evp_context_free(self->priv->evp);
    g_hash_table_unref(self->priv->events);

    G_OBJECT_CLASS(eventc_connection_parent_class)->finalize(object);
}

/**
 * eventc_connection_new:
 * @host: the host running the eventd instance to connect to
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Creates a new connection to an eventd daemon.
 *
 * Returns: (transfer full): a new connection, or %NULL if @host could not be resolved
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new(const gchar *host, GError **error)
{
    g_return_val_if_fail(host != NULL, NULL);
    g_return_val_if_fail(error == NULL || *error == NULL, NULL);

    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    if ( ! eventc_connection_set_host(self, host, error) )
    {
        g_object_unref(self);
        return NULL;
    }

    return self;
}

/**
 * eventc_connection_new_for_connectable:
 * @address: (transfer full): a #GSocketConnectable
 *
 * Creates a new connection to an eventd daemon.
 *
 * Returns: (transfer full): a new connection
 */
EVENTD_EXPORT
EventcConnection *
eventc_connection_new_for_connectable(GSocketConnectable *address)
{
    g_return_val_if_fail(G_IS_SOCKET_CONNECTABLE(address), NULL);

    EventcConnection *self;

    self = g_object_new(EVENTC_TYPE_CONNECTION, NULL);

    self->priv->address = address;

    return self;
}

/**
 * eventc_connection_is_connected:
 * @self: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Retrieves whether a given connection is actually connected to a server or
 * not.
 *
 * Returns: %TRUE if the connection was successful
 */
EVENTD_EXPORT
gboolean
eventc_connection_is_connected(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if ( self->priv->error != NULL )
    {
        g_propagate_error(error, self->priv->error);
        self->priv->error = NULL;
        return FALSE;
    }

    GError *_inner_error_ = NULL;
    if ( libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
        return TRUE;

    if ( _inner_error_ != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Connection error: %s", _inner_error_->message);
        g_error_free(_inner_error_);
    }

    return FALSE;
}

static gboolean
_eventc_connection_expect_connected(EventcConnection *self, GError **error)
{
    GError *_inner_error_ = NULL;

    if ( eventc_connection_is_connected(self, &_inner_error_) )
        return TRUE;

    if ( _inner_error_ != NULL )
        g_propagate_error(error, _inner_error_);
    else
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_NOT_CONNECTED, "Not connected, you must connect first");
    return FALSE;
}

static gboolean
_eventc_connection_expect_disconnected(EventcConnection *self, GError **error)
{
    GError *_inner_error_ = NULL;

    if ( eventc_connection_is_connected(self, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_ALREADY_CONNECTED, "Already connected, you must disconnect first");
        return FALSE;
    }

    if ( _inner_error_ != NULL )
    {
        g_propagate_error(error, _inner_error_);
        return FALSE;
    }

    return TRUE;
}

static gboolean
_eventc_connection_connect_after(EventcConnection *self, GSocketConnection *connection, GError *_inner_error_, GError **error)
{
    if ( connection == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to connect: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    libeventd_evp_context_set_connection(self->priv->evp, connection);
    if ( self->priv->passive )
    {
        if ( ! libeventd_evp_context_passive(self->priv->evp, &_inner_error_) )
        {
            g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_CONNECTION, "Failed to go into passive mode: %s", _inner_error_->message);
            g_error_free(_inner_error_);
            return FALSE;
        }
    }
    else
        libeventd_evp_context_receive_loop(self->priv->evp, G_PRIORITY_DEFAULT);

    return TRUE;
}

static void
_eventc_connection_connect_callback(GObject *obj, GAsyncResult *res, gpointer user_data)
{
    EventcConnectionCallbackData *data = user_data;
    EventcConnection *self = data->self;
    GAsyncReadyCallback callback = data->callback;
    user_data = data->user_data;
    g_slice_free(EventcConnectionCallbackData, data);

    GError *_inner_error_ = NULL;
    GError *error = NULL;
    GSocketConnection *connection;
    connection = g_socket_client_connect_finish(G_SOCKET_CLIENT(obj), res, &_inner_error_);
    if ( ! _eventc_connection_connect_after(self, connection, _inner_error_, &error) )
    {
        g_simple_async_report_take_gerror_in_idle(G_OBJECT(self), callback, user_data, error);
        return;
    }

    GSimpleAsyncResult *result;
    result = g_simple_async_result_new(G_OBJECT(self), callback, user_data, _eventc_connection_connect_callback);
    g_simple_async_result_complete_in_idle(result);
    g_object_unref(result);
}

/**
 * eventc_connection_connect:
 * @self: an #EventcConnection
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Initializes the connection to the stored host.
 */
EVENTD_EXPORT
void
eventc_connection_connect(EventcConnection *self, GAsyncReadyCallback callback, gpointer user_data)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    GError *error = NULL;


    if ( ! _eventc_connection_expect_disconnected(self, &error) )
    {
        g_simple_async_report_take_gerror_in_idle(G_OBJECT(self), callback, user_data, error);
        return;
    }

    GSocketClient *client;

    client = g_socket_client_new();
    g_socket_client_set_enable_proxy(client, self->priv->enable_proxy);

    EventcConnectionCallbackData *data;
    data = g_slice_new(EventcConnectionCallbackData);
    data->self = self;
    data->callback = callback;
    data->user_data = user_data;

    g_socket_client_connect_async(client, self->priv->address, NULL, _eventc_connection_connect_callback, data);
    g_object_unref(client);
}

/**
 * eventc_connection_connect_finish:
 * @self: an #EventcConnection
 * @result: a #GAsyncResult
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with eventc_connection_connect().
 *
 * Returns: %TRUE if the connection completed successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_connect_finish(EventcConnection *self, GAsyncResult *result, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(g_simple_async_result_is_valid(result, G_OBJECT(self), NULL), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT(result);

    if ( g_simple_async_result_propagate_error(simple, error) )
        return FALSE;

    return TRUE;
}

/**
 * eventc_connection_connect_sync:
 * @self: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Synchronously initializes the connection to the stored host.
 *
 * Returns: %TRUE if the connection completed successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_connect_sync(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    if ( ! _eventc_connection_expect_disconnected(self, error) )
        return FALSE;

    GSocketClient *client;

    client = g_socket_client_new();
    g_socket_client_set_enable_proxy(client, self->priv->enable_proxy);

    GError *_inner_error_ = NULL;
    GSocketConnection *connection;
    connection = g_socket_client_connect(client, self->priv->address, NULL, &_inner_error_);
    g_object_unref(client);

    if ( ! _eventc_connection_connect_after(self, connection, _inner_error_, error) )
        return FALSE;

    return TRUE;
}

/**
 * eventc_connection_event:
 * @self: an #EventcConnection
 * @event: an #EventdEvent to send to the server
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Sends an event across the connection.
 *
 * Returns: %TRUE if the event was sent successfully
 */
EVENTD_EXPORT
gboolean
eventc_connection_event(EventcConnection *self, EventdEvent *event, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(EVENTD_IS_EVENT(event), FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    EventdEvent *old;
    old = g_hash_table_lookup(self->priv->events, event);
    if ( old != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "This event was already sent to the server");
        return FALSE;
    }

    GError *_inner_error_ = NULL;

    if ( ! _eventc_connection_expect_connected(self, error) )
        return FALSE;

    if ( ! libeventd_evp_context_send_event(self->priv->evp, event, &_inner_error_) )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_EVENT, "Couldn't send event: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    EventdConnectionEventHandlers *handlers;
    handlers = g_slice_new(EventdConnectionEventHandlers);
    handlers->event = g_object_ref(event);

    handlers->answered = g_signal_connect_swapped(event, "answered", G_CALLBACK(_eventc_connection_event_answered), self);
    handlers->ended = g_signal_connect_swapped(event, "ended", G_CALLBACK(_eventc_connection_event_ended), self);

    g_hash_table_insert(self->priv->events, event, handlers);

    return TRUE;
}

/**
 * eventc_connection_event_close:
 * @self: an #EventcConnection
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Closes the connection.
 *
 * Returns: %TRUE if the connection was successfully closed
 */
EVENTD_EXPORT
gboolean
eventc_connection_close(EventcConnection *self, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    GError *_inner_error_ = NULL;
    if ( libeventd_evp_context_is_connected(self->priv->evp, &_inner_error_) )
        libeventd_evp_context_send_bye(self->priv->evp);
    else if ( _inner_error_ != NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_BYE, "Couldn't send bye message: %s", _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }

    _eventc_connection_close_internal(self);

    return TRUE;
}

static void
_eventc_connection_close_internal(EventcConnection *self)
{
    libeventd_evp_context_close(self->priv->evp);
    g_hash_table_remove_all(self->priv->events);
}


/**
 * eventc_connection_set_host:
 * @self: an #EventcConnection
 * @value: the host running the eventd instance to connect to
 * @error: (out) (optional): return location for error or %NULL to ignore
 *
 * Sets the host for the connection.
 * If @host cannot be resolved, the address will not change.
 *
 * Returns: %TRUE if the host was changed, %FALSE in case of error
 */
EVENTD_EXPORT
gboolean
eventc_connection_set_host(EventcConnection *self, const gchar *host, GError **error)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);
    g_return_val_if_fail(host != NULL, FALSE);
    g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *_inner_error_ = NULL;
    GSocketConnectable *address;

    address = libeventd_evp_get_address(host, &_inner_error_);
    if ( address == NULL )
    {
        g_set_error(error, EVENTC_ERROR, EVENTC_ERROR_HOSTNAME, "Couldn't resolve the hostname '%s': %s", host, _inner_error_->message);
        g_error_free(_inner_error_);
        return FALSE;
    }
    self->priv->address = address;

    return TRUE;
}

/**
 * eventc_connection_set_connectable:
 * @self: an #EventcConnection
 * @address: (transfer full): a #GSocketConnectable
 *
 * Sets the connectable for the connection.
 */
EVENTD_EXPORT
void
eventc_connection_set_connectable(EventcConnection *self, GSocketConnectable *address)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    g_object_unref(self->priv->address);
    self->priv->address = address;
}

/**
 * eventc_connection_set_passive:
 * @self: an #EventcConnection
 * @value: the passive setting
 *
 * Sets whether the connection is passive or not. A passive connection does not
 * receive events back from the server so that the client does not require an
 * event loop.
 */
EVENTD_EXPORT
void
eventc_connection_set_passive(EventcConnection *self, gboolean passive)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->passive = passive;
}

/**
 * eventc_connection_set_enable_proxy:
 * @self: an #EventcConnection
 * @value: the passive setting
 *
 * Sets whether the connection is to use a proxy or not on the underlying
 * #GSocketClient.
 */
EVENTD_EXPORT
void
eventc_connection_set_enable_proxy(EventcConnection *self, gboolean enable_proxy)
{
    g_return_if_fail(EVENTC_IS_CONNECTION(self));

    self->priv->enable_proxy = enable_proxy;
}

/**
 * eventc_connection_get_passive:
 * @self: an #EventcConnection
 *
 * Retrieves whether the connection is passive or not.
 *
 * Returns: %TRUE if the connection is passive
 */
EVENTD_EXPORT
gboolean
eventc_connection_get_passive(EventcConnection *self)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    return self->priv->passive;
}

/**
 * eventc_connection_get_enable_proxy:
 * @self: an #EventcConnection
 *
 * Retrieves whether the connection is to use a proxy or not.
 *
 * Returns: %TRUE if the connection is using a proxy
 */
EVENTD_EXPORT
gboolean
eventc_connection_get_enable_proxy(EventcConnection *self)
{
    g_return_val_if_fail(EVENTC_IS_CONNECTION(self), FALSE);

    return self->priv->enable_proxy;
}
