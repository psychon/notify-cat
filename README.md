notify-cat
==========

Use a pipe for getting notifications. That's it.

No message sources defined, no arguments given?Usage: ./notify-cat [source specifications]
Each source specifications is a comma separated list of options.

Supported options are:
 - file: Name of a file that should be read or - for stdin
 - urgency: One of 'low', 'normal' or 'critical' which specifies the urgency of the generated notifactions
 - timeout: Timeout in seconds for the notifications
 - icon: The icon to use for the notifications
 - prefix: Prefix to prepend to generated messages

Example
-------

	tail --follow=name --lines=0 --quiet /var/log/syslog | notify-cat file:-,timeout:10,icon:/home/$USER/foo.png,urgency:low,prefix:Beware:\ 
