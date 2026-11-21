/* version.h - the one place digitalClock's version is set; digitalClock -v reports it.
 * DIGITALCLOCK_GIT, from the Makefile, adds the commit built: +DATE.TIME.HASH. */
#ifndef DIGITALCLOCK_VERSION_H
#define DIGITALCLOCK_VERSION_H

#include "build/gitversion.h"
#define DIGITALCLOCK_VERSION "0.1.0" DIGITALCLOCK_GIT

#endif
