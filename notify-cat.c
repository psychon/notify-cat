#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libnotify/notify.h>

#define TIMEOUT 10000

int main(void)
{
	GInputStream *in;
	GDataInputStream *lines;
	gboolean done = FALSE;

	notify_init("notify-cat");
	
	in = g_unix_input_stream_new(0, FALSE);
	lines = g_data_input_stream_new(in);

	while (!done) {
		GError *error = NULL;
		char *line = g_data_input_stream_read_line(lines, NULL, NULL, &error);

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

		done = (line == NULL && error == NULL);

		if (error != NULL)
			g_error_free(error);
		g_free(line);
	}

	notify_uninit();
	g_object_unref(G_OBJECT(lines));
	g_object_unref(G_OBJECT(in));

	return 0;
}
