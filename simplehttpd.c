/*
	 This file is part of libmicrohttpd
	 Copyright (C) 2007 Christian Grothoff (and other contributing authors)

	 This library is free software; you can redistribute it and/or
	 modify it under the terms of the GNU Lesser General Public
	 License as published by the Free Software Foundation; either
	 version 2.1 of the License, or (at your option) any later version.

	 This library is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 Lesser General Public License for more details.

	 You should have received a copy of the GNU Lesser General Public
	 License along with this library; if not, write to the Free Software
	 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <sys/stat.h>
#include <dirent.h>
#include <microhttpd.h>
#include <microhttpd.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define SERVER_NAME "Simple HTTP Server" 

static gint port = 8000;
static gchar *directory = ".";

static GOptionEntry entries[] =
{
	{ "port", 'p', 0, G_OPTION_ARG_INT, &port, "port", "8000"},
	{ "directory", 'd', 0, G_OPTION_ARG_STRING, &directory, "directory", ""},
	{ NULL }
};

GMainLoop *loop = NULL;
struct MHD_Daemon *daemon = NULL;


static ssize_t file_reader (void *cls, uint64_t pos, char *buf, size_t max) {
	FILE *file = cls;

	(void) fseek (file, pos, SEEK_SET);
	return fread (buf, 1, max, file);
}

static void file_free_callback (void *cls) {
	FILE *file = cls;
	fclose (file);
}

static void dir_free_callback (void *cls) {
	DIR *dir = cls;
	if (dir != NULL)
		closedir (dir);
}

static ssize_t dir_reader (void *cls, uint64_t pos, char *buf, size_t max) {
	DIR *dir = cls;
	struct dirent *e;

	if (max < 512)
		return 0;
	do {
		e = readdir (dir);
		if (e == NULL)
			return MHD_CONTENT_READER_END_OF_STREAM;
	} while (e->d_name[0] == '.');
	return snprintf (buf, max, "<a href=\"/%s\">%s</a><br>", e->d_name,
					e->d_name);
}

static int http_handler (void *cls, struct MHD_Connection *connection,
						const char *url, const char *method, const char *version,
						const char *upload_data, size_t *upload_data_size,
						void **ptr) {
	static int aptr;
	struct MHD_Response *response;
	int ret;
	FILE *file;
	int fd;
	DIR *dir;
	struct stat buf;
	char emsg[1024];

	g_chdir(cls);
	if (0 != strcmp (method, MHD_HTTP_METHOD_GET))
		return MHD_NO;              /* unexpected method */
	if (&aptr != *ptr) {
		/* do never respond on first call */
		*ptr = &aptr;
		return MHD_YES;
	}
	*ptr = NULL;                  /* reset when done */

	file = fopen (&url[1], "rb");
	if (NULL != file) {
		fd = fileno (file);
		if (-1 == fd) {
			(void) fclose (file);
			return MHD_NO; /* internal error */
		}
		if ( (0 != fstat (fd, &buf)) ||
		   (! S_ISREG (buf.st_mode)) ) {
			/* not a regular file, refuse to serve */
			fclose (file);
			file = NULL;
		}
	}

	if (NULL == file) {
		dir = opendir (".");
		if (NULL == dir) {
			/* most likely cause: more concurrent requests than
			available file descriptors / 2 */
			snprintf (emsg, sizeof (emsg), "Failed to open directory `.': %s\n",
					strerror (errno));
			response = MHD_create_response_from_buffer (strlen (emsg), emsg,
														MHD_RESPMEM_MUST_COPY);
			if (NULL == response)
				return MHD_NO;
			ret = MHD_queue_response (connection, MHD_HTTP_SERVICE_UNAVAILABLE,
								response);
			MHD_destroy_response (response);
		}
		else {
			response = MHD_create_response_from_callback (MHD_SIZE_UNKNOWN,
														32 * 1024, &dir_reader,
														dir, &dir_free_callback);
			if (NULL == response) {
				closedir (dir);
				return MHD_NO;
			}
			ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
			MHD_destroy_response (response);
		}
	}
	else {
		response = MHD_create_response_from_callback (buf.st_size, 32 * 1024,     /* 32k page size */
													&file_reader, file,
													&file_free_callback);
		if (NULL == response) {
			fclose (file);
			return MHD_NO;
		}
		ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
		MHD_destroy_response (response);
	}
	return ret;
}

static void server_handle_signal(int signum) {
	g_main_loop_quit(loop);
}

static void server_termination_handler(void) {
	MHD_stop_daemon(daemon);
}

int server_main(int argc, char *argv[]) {
	GError *error = NULL;
	GOptionContext *context = g_option_context_new("- start http server");
	g_option_context_add_main_entries (context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		g_print ("Option parsing failed: %s\n", error->message);
		exit (1);
	}

	if (g_chdir(directory)) {
		g_print("Failed to change working directory.\n");
		exit(1);
	}

	signal(SIGINT, server_handle_signal);
	signal(SIGTERM, server_handle_signal);
	atexit(server_termination_handler);

	daemon = MHD_start_daemon(
		MHD_USE_THREAD_PER_CONNECTION | MHD_USE_POLL | MHD_USE_DUAL_STACK,
		port,
		NULL,
		NULL,
		http_handler,
		directory,
		MHD_OPTION_END);
	if (!daemon) {
		g_print("failed to start http server\n");
		exit(1);
	}

	loop = g_main_loop_new(NULL, FALSE);
	if (!loop) {
		g_print("failed to start a server\n");
		exit(1);
	}
	g_main_loop_run(loop);
}

#ifdef _WIN32

SERVICE_STATUS service_status = {0};
SERVICE_STATUS_HANDLE service_status_handle = NULL;

static void service_ctrl_handler(DWORD ctrl_code) {
	switch (ctrl_code) {
		case SERVICE_CONTROL_STOP :

			if (service_status.dwCurrentState != SERVICE_RUNNING)
				break;
 
			service_status.dwCurrentState = SERVICE_STOP_PENDING;
			service_status.dwControlsAccepted = 0;
			service_status.dwWin32ExitCode = 0;
			service_status.dwCheckPoint = 4;
 
			SetServiceStatus(service_status_handle, &service_status);
 
			server_handle_signal(0);

			break;

		default:
			break;
	}
}

static void service_main(DWORD argc, LPTSTR *argv) {

	service_status_handle = RegisterServiceCtrlHandler(SERVER_NAME,
									(LPHANDLER_FUNCTION)service_ctrl_handler);
 
	if (!service_status_handle)
		return;
 
	ZeroMemory(&service_status, sizeof(service_status));
	service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	service_status.dwCurrentState = SERVICE_START_PENDING;
	service_status.dwControlsAccepted = 0;
	service_status.dwWin32ExitCode = 0;
	service_status.dwCheckPoint = 0;

	SetServiceStatus(service_status_handle, &service_status);

	service_status.dwCurrentState = SERVICE_RUNNING;
	service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	service_status.dwWin32ExitCode = 0;
	service_status.dwCheckPoint = 0;

	SetServiceStatus(service_status_handle, &service_status);

	server_main(argc, argv);

	service_status.dwCurrentState = SERVICE_STOPPED;
	service_status.dwControlsAccepted = 0;
	service_status.dwWin32ExitCode = 0;
	service_status.dwCheckPoint = 3;

	SetServiceStatus(service_status_handle, &service_status);

}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
	SERVICE_TABLE_ENTRY service_table[] = {{(LPTSTR)SERVER_NAME,
						(LPSERVICE_MAIN_FUNCTION)service_main}, {NULL, NULL}};
	StartServiceCtrlDispatcher(service_table);
#endif
	server_main(argc, argv);
	exit(0);
}
