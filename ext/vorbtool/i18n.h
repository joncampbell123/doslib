#ifndef VORBIS_TOOLS_I18N_H
#define VORBIS_TOOLS_I18N_H

#ifdef ENABLE_NLS
# error NO
#define _(X) gettext(X)
#else
#define _(X) (X)
#define textdomain(X)
#define bindtextdomain(X, Y)
#endif
#ifdef gettext_noop
#define N_(X) gettext_noop(X)
#else
#define N_(X) (X)
#endif

#endif
