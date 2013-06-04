/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Ovdapp : OVD applications RDP channel
 *
 * Copyright 2012 Ulteo SAS
 * http://www.ulteo.com
 * Author Alexandre CONFIANT-LATOUR <a.confiant@ulteo.com> 2012
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/svc_plugin.h>

typedef struct ovdapp_plugin
{
	rdpSvcPlugin plugin;
} ovdappPlugin;

static void ovdapp_process_connect(rdpSvcPlugin* plugin)
{
	/* Vchannel connection callback */
	printf("OvdApp connect\n");
}

static void ovdapp_process_receive(rdpSvcPlugin* plugin, STREAM* data_in)
{
	/* Vchannel receive proc (receive data FROM server) */
	/* - Get data as a stream
		 - Parse it
		 - Generate an event
		 - Send it to the main process "with svc_plugin_send_event"
		 - Free stream
	*/

	/* stream_get_size gives the data size in bytes */
	int i;
	int len = stream_get_size(data_in);
	char *data = (char*) stream_get_data(data_in);
	RDP_EVENT *event = malloc(sizeof(RDP_EVENT));

	/*
	printf("OvdApp input data(%d) : ", len);
	for(i=0 ; i<len ; ++i) printf("0x%X ", data[i]);
	printf("\n");
	*/

	/* Create a new event and copy data from stream */
	event->event_class = RDP_EVENT_CLASS_OVDAPP;
	event->event_type = 0;
	event->on_event_free_callback = NULL;
	event->user_data = malloc((len*2)+1);

	for(i=0 ; i<len ; ++i) {
		snprintf(((char*)(event->user_data))+(2*i), 3, "%02x", data[i]);
	}

	((char*)(event->user_data))[len*2] = '\0';

	/*printf("OvdApp input data(%d) : %s\n", strlen((char*)(event->user_data)), ((char*)(event->user_data)));*/

	/* Send the event to the main program */
	svc_plugin_send_event(plugin, event);

	stream_free(data_in);
}

static void ovdapp_process_event(rdpSvcPlugin* plugin, RDP_EVENT* event)
{
	/* Vchannel send proc (send data to server) */
	/* - Get data as an event
		 - Convert-it to a stream
		 - Send it to the server with "svc_plugin_send"
		 - Free event
	*/

	int i;
	char buffer[3];
	unsigned int tmp;
	int len = strlen(event->user_data);
	STREAM *stream = stream_new((len/2)+1);

	/* Copy event data to stream */
	for(i=0 ; i<len ; i+=2) {
		buffer[0] = ((char*)(event->user_data))[i];
		buffer[1] = ((char*)(event->user_data))[i+1];
		buffer[2] = '\0';

		sscanf(buffer, "%x", &tmp);
		stream_write_uint8(stream, tmp);
	}
	stream_write_uint8(stream, 0);

	/*printf("OvdApp output data(%d) : %s\n", strlen((char*)(stream_get_data(stream))), ((char*)(stream_get_data(stream))));*/

	/* Send the stream to the server */
	svc_plugin_send(plugin, stream);
}

static void ovdapp_process_terminate(rdpSvcPlugin* plugin)
{
	/* Vchannel close callback */
	printf("OvdApp terminate\n");
}

DEFINE_SVC_PLUGIN(ovdapp, "ovdapp", CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP)
