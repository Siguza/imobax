/* Copyright (c) 2019 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#include <copyfile.h>
#include <fcntl.h>              // open, O_*
#include <stdbool.h>
#include <stdio.h>              // [as|f]printf, stderr, fputs
#include <stdlib.h>             // free
#include <string.h>             // strcmp, strlen
#include <unistd.h>             // close, access, faccessat, F_OK

#include "common.h"
#include "db.h"

static void usage(const char *self)
{
    fprintf(stderr, "Usage:\n"
                    "    %s [-f] [-i] [-l] source-dir [target-dir]\n"
                    "\n"
                    "Options:\n"
                    "    -f  Force overwriting of existing files\n"
                    "    -i  Ignore missing files in backup\n"
                    "    -l  List contents only, write nothing\n"
            , self);
}

int main(int argc, const char **argv)
{
    int retval = -1;
    bool force  = false,
         ignore = false,
         list   = false;
    const char *srcstr = NULL,
               *dststr = NULL;
    char *topath = NULL;
    int srcdir = -1,
        dstdir = -1,
        auxfd  = -1,
        fromfd = -1,
        tofd   = -1;
    db_ent_t *head = NULL;
    int aoff = 1;
    for(; aoff < argc; ++aoff)
    {
        if(argv[aoff][0] != '-') break;
        if(strcmp(argv[aoff], "-f") == 0)
        {
            force = true;
        }
        else if(strcmp(argv[aoff], "-i") == 0)
        {
            ignore = true;
        }else if(strcmp(argv[aoff], "-l") == 0)
        {
            list = true;
        }
        else
        {
            LOG("Unrecognised option: %s", argv[aoff]);
            fputs("\n", stderr);
            usage(argv[0]);
            goto out;
        }
    }
    int wantargs = list ? 1 : 2;
    if(argc - aoff != wantargs)
    {
        if(argc > 1)
        {
            LOG("Too %s arguments.", (argc - aoff < wantargs) ? "few" : "many");
            fputs("\n", stderr);
        }
        else
        {
            fprintf(stderr, "imobax"
#ifdef VERSION
                            " v" STRINGIFY(VERSION)
#endif
#ifdef TIMESTAMP
                            ", compiled on " STRINGIFY(TIMESTAMP)
#endif
                            "\n\n"
            );
        }
        usage(argv[0]);
        goto out;
    }
    srcstr = argv[aoff++];
    if(!list)
    {
        dststr = argv[aoff++];
    }

    srcdir = open(srcstr, O_RDONLY);
    if(srcdir == -1)
    {
        ERRNO("open(%s)", srcstr);
        goto out;
    }

    bool two_level = false;
    if(faccessat(srcdir, DBFILE_SQLITE, F_OK, 0) == 0)
    {
        head = db_getlist_sqlite3(srcdir);
        two_level = true;
    }
    else if(faccessat(srcdir, DBFILE_MBDB, F_OK, 0) == 0)
    {
        head = db_getlist_mbdb(srcdir);
    }
    else
    {
        LOG("Found neither " DBFILE_SQLITE " nor " DBFILE_MBDB ".");
        goto out;
    }
    if(!head)
    {
        goto out;
    }

    if(list)
    {
        size_t len = 0;
        for(db_ent_t *ent = head; ent; ent = ent->next)
        {
            size_t l = strlen(ent->domain);
            if(l > len) len = l;
        }
        for(db_ent_t *ent = head; ent; ent = ent->next)
        {
            printf("%-40s %-*s %s\n", ent->fileID, (int)len, ent->domain, ent->relativePath);
        }
    }
    else
    {
        int crflags = O_CREAT | (force ? 0 : O_EXCL);

        if(mkdirat_recursive(AT_FDCWD, dststr, false) != 0)
        {
            goto out;
        }
        dstdir = open(dststr, O_RDONLY);
        if(dstdir == -1)
        {
            ERRNO("open(%s)", dststr);
            goto out;
        }

        for(db_ent_t *ent = head; ent; ent = ent->next)
        {
            asprintf(&topath, "%s/%s", ent->domain, ent->relativePath);
            if(!topath)
            {
                ERRNO("asprintf");
                goto out;
            }
            // Turn first dash in domain part into a slash
            for(size_t i = 0, len = strlen(ent->domain); i < len && topath[i] != '\0'; ++i)
            {
                if(topath[i] == '-')
                {
                    topath[i] = '/';
                    break;
                }
            }

            if(two_level)
            {
                char aux[3] = { ent->fileID[0], ent->fileID[1], '\0' };
                auxfd = openat(srcdir, aux, O_RDONLY);
                if(auxfd == -1)
                {
                    ERRNO("open(%s)", aux);
                    if(ignore)
                    {
                        goto next;
                    }
                    goto out;
                }
            }
            fromfd = openat(auxfd == -1 ? srcdir : auxfd, ent->fileID, O_RDONLY);
            if(fromfd == -1)
            {
                ERRNO("open(%s)", ent->fileID);
                if(ignore)
                {
                    goto next;
                }
                goto out;
            }
            if(mkdirat_recursive(dstdir, topath, true) != 0)
            {
                goto out;
            }
            tofd = openat(dstdir, topath, O_WRONLY | crflags);
            if(tofd == -1)
            {
                ERRNO("open(%s)", topath);
                goto out;
            }
            if(fcopyfile(fromfd, tofd, NULL, COPYFILE_ALL) != 0)
            {
                ERRNO("copyfile(%s)", topath);
                goto out;
            }

        next:;
            if(tofd != -1)
            {
                close(tofd);
                tofd   = -1;
            }
            if(fromfd != -1)
            {
                close(fromfd);
                fromfd = -1;
            }
            if(auxfd != -1)
            {
                close(auxfd);
                auxfd  = -1;
            }
            if(topath)
            {
                free(topath);
                topath = NULL;
            }
        }
    }

    retval = 0;
out:;
    if(tofd   != -1) close(tofd);
    if(fromfd != -1) close(fromfd);
    if(auxfd  != -1) close(auxfd);
    if(topath) free(topath);
    if(head) db_cleanup(head);
    if(dstdir != -1) close(dstdir);
    if(srcdir != -1) close(srcdir);
    return retval;
}
