#ifndef __CONFIG_H
#define __CONFIG_H


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"


#ifndef HAVE_SYS_SOCKET_H
#define HAVE_SYS_SOCKET_H
#endif // HAVE_SYS_SOCKET_H

#ifndef HAVE_ARPA_INET_H
#define HAVE_ARPA_INET_H
#endif // HAVE_ARPA_INET_H

#ifndef HAVE_NETINET_IN_H
#define HAVE_NETINET_IN_H
#endif // HAVE_NETINET_IN_H

#ifndef HAVE_NETINET_TCP_H
#define HAVE_NETINET_TCP_H
#endif // HAVE_NETINET_TCP_H

#ifndef HAVE_TERMIOS_H
#define HAVE_TERMIOS_H
#endif // HAVE_TERMIOS_H

#ifndef HAVE_NETDB_H
#define HAVE_NETDB_H
#endif // HAVE_NETDB_H

#ifndef HAVE_FCNTL_H
#define HAVE_FCNTL_H
#endif // HAVE_FCNTL_H

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H
#endif // HAVE_UNISTD_H

#ifndef HAVE_SYS_UN_H
#define HAVE_SYS_UN_H
#endif // HAVE_SYS_UN_H

#ifndef HAVE_ERRNO_H
#define HAVE_ERRNO_H
#endif // HAVE_ERRNO_H


#pragma GCC diagnostic pop

#define remmina			"remmina"
#define REMMINA_APP_ID		"org.remmina.Remmina"
#define VERSION			"1.4.12"
#define REMMINA_GIT_REVISION	"eafc42035"
#define RMNEWS_ENABLE_NEWS	1

#define GETTEXT_PACKAGE		remmina

#define REMMINA_RUNTIME_DATADIR			"/usr/local/share"
#define REMMINA_RUNTIME_LOCALEDIR		"/usr/local/share/locale"
#define REMMINA_RUNTIME_PLUGINDIR		"/usr/local/lib/remmina/plugins"
#define REMMINA_RUNTIME_UIDIR			"/usr/local/share/remmina/ui"
#define REMMINA_RUNTIME_THEMEDIR		"/usr/local/share/remmina/theme"
#define REMMINA_RUNTIME_EXTERNAL_TOOLS_DIR	"/usr/local/share/remmina/external_tools"
#define REMMINA_RUNTIME_TERM_CS_DIR		"/usr/local/share/remmina/theme"

#endif
