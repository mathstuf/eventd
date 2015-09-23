/*
 * libeventc - Library to communicate with eventd
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

namespace Eventc
{
	[CCode (cheader_filename = "libeventc.h")]
	public static unowned string get_version();

	[CCode (cheader_filename = "libeventc.h")]
	public errordomain Error {
		HOSTNAME,
		CONNECTION,
		ALREADY_CONNECTED,
		NOT_CONNECTED,
		RECEIVE,
		EVENT,
		END,
		BYE
	}

	[CCode (cheader_filename = "libeventc.h")]
	public class Connection : GLib.Object
	{
		public bool passive { get; set; }
		public bool enable_proxy { get; set; }
		public string host { private get; set; }

		public Connection(string host);
		public bool close() throws Eventc.Error;
		public new async void connect() throws Eventc.Error;
		public new void connect_sync() throws Eventc.Error;
		public bool event(Eventd.Event event) throws Eventc.Error;
		public bool is_connected() throws Eventc.Error;
	}
}
