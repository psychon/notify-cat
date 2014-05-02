/* (C) 2011 Uli Schlachter
 *
 * This code is covered by the following license:
 *
 * 0. You just DO WHAT THE FUCK YOU WANT TO.
 */

#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libnotify/notify.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define DEFAULT_TIMEOUT 10000

struct message_source {
	GDataInputStream *lines;
	NotifyUrgency urgency;
	gint timeout;
	char *icon;
	char *prefix;
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
		const gchar *notify_message = line;
		
		if (!g_utf8_validate(line, -1, NULL)) {
			utf8 = g_locale_to_utf8(line, -1, NULL, NULL, NULL);
			if (utf8)
				notify_message = utf8;
			else
				notify_message = "ERROR: Read invalid input line'";
		}

		if (source->prefix) {
			gchar *tmp = g_strconcat(source->prefix, notify_message, NULL);
			g_free(utf8);
			notify_message = utf8 = tmp;
		}

		notify = notify_notification_new(notify_message, NULL, source->icon);
		notify_notification_set_urgency(notify, source->urgency);
		notify_notification_set_timeout(notify, source->timeout);
		notify_notification_show(notify, NULL);
		g_object_unref(G_OBJECT(notify));
		g_free(utf8);
	}

	if (line != NULL && error == NULL)
		message_source_start_read(source);
	else {
		/* TODO: print errors */
		g_object_unref(stream);
		g_free(source->icon);
		g_free(source->prefix);
		g_free(source);
		if (--message_sources_alive == 0)
			g_main_loop_quit(main_loop);
	}

	if (error != NULL)
		g_error_free(error);
	g_free(line);
}

static struct message_source *message_source_new(void)
{
	struct message_source *source = g_malloc(sizeof(*source));

	if (!source)
		return NULL;

	/* Apply default settings */
	source->urgency = NOTIFY_URGENCY_LOW;
	source->timeout = DEFAULT_TIMEOUT;
	source->icon = NULL;
	source->prefix = NULL;
	source->lines = NULL;

	message_sources_alive++;

	return source;

}
static void message_source_set_fd(struct message_source *source, int fd, gboolean close_fd)
{
	GInputStream *in;

	/* Set up the stream */
	in = g_unix_input_stream_new(fd, close_fd);
	source->lines = g_data_input_stream_new(in);
	g_object_unref(G_OBJECT(in));
}

static gboolean parse_argument(const char *arg)
{
	gchar **options;
	size_t i;
	gchar *filename = NULL;
	struct message_source *source;

	source = message_source_new();
	options = g_strsplit(arg, ",", 0);
	if (!options || !source) {
		fputs("Internal error in argument parsing?! (out of memory?)", stderr);
		goto error;
	}

	i = 0;
	while (options[i] != NULL) {
		size_t len;
		const char *pos = strchr(options[i], ':');

		if (pos == NULL) {
			fprintf(stderr, "No value found in option '%s'\n", options[i]);
			goto error;
		}

		len = pos - options[i];
		pos++;
		if (strncmp(options[i], "file", len) == 0) {
			g_free(filename);
			filename = g_strdup(pos);
		} else if (strncmp(options[i], "urgency", len) == 0) {
			if (strcmp(pos, "low") == 0)
				source->urgency = NOTIFY_URGENCY_LOW;
			else if (strcmp(pos, "normal") == 0)
				source->urgency = NOTIFY_URGENCY_NORMAL;
			else if (strcmp(pos, "critical") == 0)
				source->urgency = NOTIFY_URGENCY_CRITICAL;
			else {
				fprintf(stderr, "Unknown parameter '%s'\n", options[i]);
				goto error;
			}
		} else if (strncmp(options[i], "timeout", len) == 0) {
			source->timeout = g_ascii_strtod(pos, NULL) * 1000;
		} else if (strncmp(options[i], "icon", len) == 0) {
			source->icon = g_strdup(pos);
		} else if (strncmp(options[i], "prefix", len) == 0) {
			source->prefix = g_strdup(pos);
		} else {
			fprintf(stderr, "Unknown option '%s'\n", options[i]);
			goto error;
		}
		i++;
	}

	if (filename == NULL || strcmp(filename, "-") == 0) {
		message_source_set_fd(source, 0, FALSE);
	} else {
		int fd = open(filename, O_RDONLY);
		if (fd < 0) {
			perror("Failed to open file");
			goto error;
		}
		message_source_set_fd(source, fd, TRUE);
	}

	/* And get it going */
	message_source_start_read(source);

	g_strfreev(options);
	return TRUE;

error:
	g_strfreev(options);
	return FALSE;
}

static gboolean parse_arguments(int argc, char * const argv[])
{
	int i;

	for (i = 0; i < argc; i++)
		if (!parse_argument(argv[i]))
			return FALSE;

	if (message_sources_alive == 0) {
		fputs("No message sources defined, no arguments given?", stderr);
		return FALSE;
	}
	return TRUE;
}

int main(int argc, char *argv[])
{
	notify_init("notify-cat");

	if (!parse_arguments(argc - 1, argv + 1)) {
		printf("Usage: %s [source specifications]\n"
				"Each source specifications is a comma separated list of options.\n\n"
				"Supported options are:\n"
				" - file: Name of a file that should be read or - for stdin\n"
				" - urgency: One of 'low', 'normal' or 'critical' which specifies the urgency of the generated notifactions\n"
				" - timeout: Timeout in seconds for the notifications\n"
				" - icon: The icon to use for the notifications\n"
				" - prefix: Prefix to prepend to generated messages\n"
				"\nUsage example:\n"
				"%s file:-,timeout:10,icon:/home/$USER/foo.png,urgency:low,prefix:Beware:\\ \n",
				argv[0], argv[0]);
		return 1;
	}
	
	main_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(main_loop);
	g_main_loop_unref(main_loop);

	notify_uninit();

	return 0;
}
