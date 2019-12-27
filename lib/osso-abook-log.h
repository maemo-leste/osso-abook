#ifndef __OSSO_ABOOK_LOG_H_INCLUDED__
#define __OSSO_ABOOK_LOG_H_INCLUDED__

#define OSSO_ABOOK_WARN(fmt, ...) g_warning("%s: " fmt, __FILE__ ":" G_STRINGIFY (__LINE__), ##__VA_ARGS__)

#endif /* __OSSO_ABOOK_LOG_H_INCLUDED__ */
