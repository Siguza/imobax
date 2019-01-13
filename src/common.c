/* Copyright (c) 2019 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>             // free
#include <string.h>             // strdup
#include <unistd.h>             // faccessat, F_OK
#include <sys/stat.h>           // mkdirat

#include "common.h"

int mkdirat_recursive(int fd, const char *path, bool only_parent)
{
    int retval = -1;
    char *str = NULL;

    if(path[0] == '\0')
    {
        LOG("mkdirat_recursive: bad argument");
        goto out;
    }

    str = strdup(path);
    if(!str)
    {
        ERRNO("strdup(%s)", path);
        goto out;
    }

    for(size_t i = 1; 1; ++i)
    {
        bool doit = false,
             flip = false,
             end  = false;
        if(str[i] == '/')
        {
            doit = true;
            flip = true;
        }
        else if(str[i] == '\0')
        {
            if(!only_parent)
            {
                doit = true;
            }
            end = true;
        }

        if(doit)
        {
            if(flip)
            {
                str[i] = '\0';
            }

            if(faccessat(fd, str, F_OK, 0) != 0)
            {
                if(errno != ENOENT)
                {
                    ERRNO("access(%s)", str);
                    break;
                }
                else
                {
                    if(mkdirat(fd, str, 0755) != 0)
                    {
                        ERRNO("mkdir(%s)", str);
                        break;
                    }
                }
            }

            if(flip)
            {
                str[i] = '/';
            }
        }

        if(end)
        {
            break;
        }
    }

    retval = 0;
out:;
    if(str) free(str);
    return retval;
}
