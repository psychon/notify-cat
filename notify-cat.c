/* (C) 2011 Uli Schlachter
 *
 * This code is covered by the following license:
 *
 * 0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libnotify/notify.h>

#define TIMEOUT 10000

struct message_source {
	GDataInputStream *lines;
};

static unsigned int message_sources_alive = 0;
static GMainLoop *main_loop;

static void message_source_finish_read(GObject *, GAsyncResult *, gpointer);

static void message_source_start_read(struct message_source *source)
{
	g_data_input_stream_read_line_async(source->lines, G_PRIORITY_DEFAULT, NULL, message_source_finish_read, source);
}

static void message_source_finish_read(GObject *stream, GAsyncResult *result, gpointer source_)
{
	struct message_source *source = source_;
	GError *error = NULL;
	char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(stream), result, NULL, &error);

	if (line && *line != '\0') {
		NotifyNotification *notify;
		gchar *utf8 = NULL;
		
		if (g_utf8_validate(line, -1, NULL))
			utf8 = line;
		else {
			utf8 = g_locale_to_utf8(line, -1, NULL, NULL, NULL);
			if (!utf8)
				utf8 = g_strdup("ERROR: Read invalid input line'");
		}

		notify = notify_notification_new(utf8, NULL, NULL);
		notify_notification_set_urgency(notify, NOTIFY_URGENCY_LOW);
		notify_notification_set_timeout(notify, TIMEOUT);
		notify_notification_show(notify, NULL);
		g_object_unref(G_OBJECT(notify));
		if (utf8 != line)
			g_free(utf8);
	}

	if (line != NULL && error == NULL)
		message_source_start_read(source);
	else {
		/* TODO: print errors */
		g_object_unref(stream);
		g_free(source);
		if (--message_sources_alive == 0)
			g_main_loop_quit(main_loop);
	}

	if (error != NULL)
		g_error_free(error);
	g_free(line);
}

static struct message_source *message_source_add_from_fd(int fd, gboolean close_fd)
{
	GInputStream *in;
	struct message_source *source = g_malloc(sizeof(*source));

	if (!source)
		return NULL;

	in = g_unix_input_stream_new(fd, close_fd);
	source->lines = g_data_input_stream_new(in);
	g_object_unref(G_OBJECT(in));

	message_sources_alive++;
	message_source_start_read(source);

	return source;
}

int main(void)
{
	notify_init("notify-cat");

	message_source_add_from_fd(0, FALSE);
	
	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);
	g_main_loop_unref(main_loop);

	notify_uninit();

	return 0;
}
