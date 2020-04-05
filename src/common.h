/* Copyright (c) 2019 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#ifndef COMMON_H
#define COMMON_H

#ifndef __APPLE__
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define LOG(str, args...) do { fprintf(stderr, str "\n", ##args); } while(0)
#define ERRNO(str, args...) LOG(str ": %s", ##args, strerror(errno))

#define STRINGIFX(x) #x
#define STRINGIFY(x) STRINGIFX(x)

int mkdirat_recursive(int fd, const char *path, bool only_parent);

#endif
