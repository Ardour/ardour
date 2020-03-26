/*
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

CrossThreadChannel::CrossThreadChannel (bool non_blocking)
	: receive_channel (0)
	, receive_source (0)
	, receive_slot ()
	, send_socket()
	, receive_socket()
	, recv_address()
{
	struct sockaddr_in send_address;

	// Create Send Socket
	send_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	send_address.sin_family = AF_INET;
	send_address.sin_addr.s_addr = inet_addr("127.0.0.1");
	send_address.sin_port = htons(0);
	int status = ::bind(send_socket, (SOCKADDR*)&send_address,
			  sizeof(send_address));

	if (status != 0) {
		std::cerr << "CrossThreadChannel::CrossThreadChannel() Send socket binding failed with error: " << WSAGetLastError() << std::endl;
		return;
	}

	// make the socket non-blockable if required
	u_long mode = (u_long)non_blocking;
	int otp_result = 0;

	otp_result = ioctlsocket(send_socket, FIONBIO, &mode);
	if (otp_result != NO_ERROR) {
		std::cerr << "CrossThreadChannel::CrossThreadChannel() Send socket cannot be set to non blocking mode with error: " << WSAGetLastError() << std::endl;
	}

	// Create Receive Socket, this socket will be set to unblockable mode by IO channel
	receive_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	recv_address.sin_family = AF_INET;
	recv_address.sin_addr.s_addr = inet_addr("127.0.0.1");
	recv_address.sin_port = htons(0);
	status = ::bind(receive_socket, (SOCKADDR*)&recv_address,
		      sizeof(recv_address));

	if (status != 0) {
		std::cerr << "CrossThreadChannel::CrossThreadChannel() Receive socket binding failed with error: " << WSAGetLastError() << std::endl;
		return;
	}

	// recieve socket will be made non-blocking by GSource which will use it

	// get assigned port number for Receive Socket
	int recv_addr_len = sizeof(recv_address);
	status = getsockname(receive_socket, (SOCKADDR*)&recv_address, &recv_addr_len);

	if (status != 0) {
		std::cerr << "CrossThreadChannel::CrossThreadChannel() Setting receive socket address to local failed with error: " << WSAGetLastError() << std::endl;
		return;
	}

	// construct IOChannel
	receive_channel = g_io_channel_win32_new_socket((gint)receive_socket);

	// set binary data type
	GIOStatus g_status = g_io_channel_set_encoding (receive_channel, NULL, NULL);
	if (G_IO_STATUS_NORMAL != g_status ) {
		std::cerr << "CrossThreadChannel::CrossThreadChannel() Cannot set flag for IOChannel. " << g_status << std::endl;
		return;
	}

	// disable channel buffering
	g_io_channel_set_buffered (receive_channel, false);
}

CrossThreadChannel::~CrossThreadChannel ()
{
	if (receive_source) {
		/* this disconnects it from any main context it was attached in
		   in ::attach(), this prevent its callback from being invoked
		   after the destructor has finished.
		*/
		g_source_destroy (receive_source);
	}

	/* glibmm hack */

	if (receive_channel) {
		g_io_channel_unref (receive_channel);
	}

	closesocket(send_socket);
	closesocket(receive_socket);
}

void
CrossThreadChannel::wakeup ()
{
	char c = 0;

	// write one byte to wake up a thread which is listening our IOS
	sendto(send_socket, &c, sizeof(c), 0, (SOCKADDR*)&recv_address, sizeof(recv_address) );
}

void
CrossThreadChannel::drain ()
{
	/* flush the buffer - empty the channel from all requests */
	GError *g_error = 0;
	gchar buffer[512];
	gsize read = 0;

	while (1) {
		GIOStatus g_status = g_io_channel_read_chars (receive_channel, buffer, sizeof(buffer), &read, &g_error);

		if (G_IO_STATUS_AGAIN == g_status) {
			break;
		}

		if (G_IO_STATUS_NORMAL != g_status) {
			std::cerr << "CrossThreadChannel::CrossThreadChannel() Cannot drain from read buffer! " << g_status << std::endl;

			if (g_error) {
				std::cerr << "Error is Domain: " << g_error->domain << " Code: " << g_error->code << std::endl;
				g_clear_error(&g_error);
			} else {
				std::cerr << "No error provided\n";
			}
			break;
		}
	}
}


int
CrossThreadChannel::deliver (char msg)
{

	// write one particular byte to wake up the thread which is listening our IOS
	int status = sendto(send_socket, &msg, sizeof(msg), 0, (SOCKADDR*)&recv_address, sizeof(recv_address) );

	if (SOCKET_ERROR  == status) {
		return -1;
	}

	return status;
}

bool
CrossThreadChannel::poll_for_request()
{
	// windows before Vista has no poll
	while(true) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(receive_socket, &rfds);
		if ((select(receive_socket+1, &rfds, NULL, NULL, NULL)) < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if(FD_ISSET(receive_socket, &rfds)) {
			return true;
		}
	}
	return false;
}

int
CrossThreadChannel::receive (char& msg, bool wait)
{
	gsize read = 0;
	GError *g_error = 0;

	if (wait) {
		if (!poll_for_request ()) {
			return -1;
		}
	}

	// fetch the message from the channel.
	GIOStatus g_status = g_io_channel_read_chars (receive_channel, &msg, sizeof(msg), &read, &g_error);

	if (G_IO_STATUS_NORMAL != g_status) {
		read = -1;
	}

	return read;
}
