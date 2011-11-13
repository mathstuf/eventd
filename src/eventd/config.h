/*
 * eventd - Small daemon to act on remote or local events
 *
 * Copyright © 2011 Quentin "Sardem FF7" Glidic
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

#ifndef __EVENTD_EVENTS_H__
#define __EVENTD_EVENTS_H__

typedef struct _EventdConfig EventdConfig;
typedef struct _EventdConfigEvent EventdConfigEvent;

EventdConfig *eventd_config_parser(EventdConfig *config);
void eventd_config_clean(EventdConfig *config);

gint64 eventd_config_get_max_clients(EventdConfig *config);

void eventd_config_event_get_disable_and_timeout(EventdConfig *config, EventdClient *client, EventdEvent *event, gboolean *disable, gint64 *timeout);

#endif /* __EVENTD_EVENTS_H__ */
