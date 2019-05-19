/* Copyright (c) 2019 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#include <fcntl.h>              // fcntl, F_GETPATH, openat, O_*
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>             // malloc, free
#include <string.h>             // strcat, strdup, memcmp
#include <sqlite3.h>
#include <unistd.h>             // close
#include <sys/mman.h>           // mmap, munmap
#include <sys/param.h>          // MAXPATHLEN
#include <sys/stat.h>           // fstat
#include <CommonCrypto/CommonDigest.h> // CC_SHA1

#include "common.h"
#include "db.h"

static int db_getlist_cb(void *arg, int num, char **data, char **cols)
{
    int retval = -1;
    db_ent_t **head = arg;
    char *fileID       = NULL,
         *domain       = NULL,
         *relativePath = NULL;
    if(!data[0] || !data[1] || !data[2])
    {
        LOG("NULL in result");
        goto out;
    }
    fileID       = strdup(data[0]),
    domain       = strdup(data[1]),
    relativePath = strdup(data[2]);
    if(!fileID || !domain || !relativePath)
    {
        ERRNO("strdup");
        goto out;
    }
    db_ent_t *ent = malloc(sizeof(db_ent_t));
    if(!ent)
    {
        ERRNO("malloc");
        goto out;
    }
    ent->next         = *head;
    ent->fileID       = fileID;
    ent->domain       = domain;
    ent->relativePath = relativePath;
    *head = ent;
    // Prevent freeing
    fileID = NULL;
    domain = NULL;
    relativePath = NULL;

    retval = SQLITE_OK;
out:;
    if(fileID) free(fileID);
    if(domain) free(domain);
    if(relativePath) free(relativePath);
    return retval;
}

static void db_debug_log(void *arg, int code, const char *msg)
{
    LOG("[DBG] (%u) %s", code, msg);
}

static char* db_make_uri(const char *path)
{
    if(path[0] != '/')
    {
        LOG("db_make_uri: not an absolute path");
        return NULL;
    }
    size_t len =  7  // file://
               + 12  // ?immutable=1
               +  1; // NUL
    for(size_t i = 0; path[i] != '\0'; ++i)
    {
        char c = path[i];
        if
        (
            (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_' || c == '-' || c == '.' || c == '~' || c == '/'
        )
        {
            len += 1;
        }
        else
        {
            len += 3; // %xx
        }
    }
    char *uri = malloc(len);
    if(!uri)
    {
        ERRNO("malloc(uri)");
        return NULL;
    }
    uri[0] = '\0';
    strcat(uri, "file://");
    size_t u = 7;
    for(size_t i = 0; path[i] != '\0'; ++i)
    {
        char c = path[i];
        if
        (
            (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_' || c == '-' || c == '.' || c == '~' || c == '/'
        )
        {
            uri[u++] = c;
        }
        else
        {
            const char hex[] = "0123456789ABCDEF";
            uri[u++] = '%';
            uri[u++] = hex[(c >> 4) & 0xf];
            uri[u++] = hex[(c     ) & 0xf];
        }
    }
    uri[u] = '\0';
    strcat(uri, "?immutable=1");
    return uri;
}

db_ent_t* db_getlist_sqlite3(int srcdir)
{
    // Ideally we'd want to pass a file handle to SQLite,
    // to avoid race conditions when folders are moved around.
    // But since SQLite only accepts path names for opening,
    // we have to get a path from our already open fd.
    // This actually avoids the case where the folder has been moved
    // between having been opened and now, but there's still a race window
    // between the invocations of fcntl() and sqlite3_open_v2().
    // But if the API can't do better, what are we supposed to do?
    char buf[MAXPATHLEN];
    int fd = -1;
    char *uri = NULL;
    sqlite3 *db = NULL;
    char *err = NULL;
    db_ent_t *retval = NULL;

    fd = openat(srcdir, DBFILE_SQLITE, O_RDONLY);
    if(fd == -1)
    {
        ERRNO("open(" DBFILE_SQLITE ")");
        goto out;
    }
    if(fcntl(fd, F_GETPATH, buf) != 0)
    {
        ERRNO("fcntl(" DBFILE_SQLITE ")");
        goto out;
    }
    uri = db_make_uri(buf);
    if(!uri)
    {
        goto out;
    }

    int r = sqlite3_config(SQLITE_CONFIG_LOG, &db_debug_log, NULL);
    if(r != SQLITE_OK)
    {
        LOG("sqlite3_config(SQLITE_CONFIG_LOG): %s", sqlite3_errstr(r));
        goto out;
    }

    r = sqlite3_open_v2(uri, &db, SQLITE_OPEN_URI | SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL); // yolo
    if(r != SQLITE_OK)
    {
        LOG("sqlite3_open: %s", sqlite3_errstr(r));
        goto out;
    }

    r = sqlite3_exec(db, "SELECT `fileID`, `domain`, `relativePath` FROM `Files` WHERE `flags` = 1", &db_getlist_cb, &retval, &err);
    if(r != SQLITE_OK)
    {
        LOG("sqlite3_exec: %s", err);
        db_cleanup(retval);
        retval = NULL;
        goto out;
    }

out:;
    if(err) sqlite3_free(err);
    if(db) sqlite3_close(db);
    if(uri) free(uri);
    if(fd != -1) close(fd);
    return retval;
}

#define MBDB_UINT16(var) \
do \
{ \
    if(data + 2 > end) \
    { \
        LOG("mbdb: truncated at uint16"); \
        goto out; \
    } \
    var = ((uint16_t)data[0] << 8) | (uint16_t)data[1]; \
    data += 2; \
} while(0)

#define MBDB_STR(str, slen) \
do \
{ \
    MBDB_UINT16(slen); \
    if(slen == 0xffff) \
    { \
        str = NULL; \
    } \
    else \
    { \
        if(data + slen > end) \
        { \
            LOG("mbdb: truncated at string data"); \
            goto out; \
        } \
        str = (char*)data; \
        data += slen; \
    } \
} while(0)

db_ent_t* db_getlist_mbdb(int srcdir)
{
    bool success = false;
    db_ent_t *retval = NULL;
    int fd = -1;
    void *addr = MAP_FAILED;
    char *fileID       = NULL,
         *domain       = NULL,
         *relativePath = NULL,
         *fullPath     = NULL;

    fd = openat(srcdir, DBFILE_MBDB, O_RDONLY);
    if(fd == -1)
    {
        ERRNO("open(" DBFILE_MBDB ")");
        goto out;
    }
    struct stat s;
    if(fstat(fd, &s) != 0)
    {
        ERRNO("stat(" DBFILE_MBDB ")");
        goto out;
    }
    size_t flen = s.st_size;
    addr = mmap(NULL, flen, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0);
    if(addr == MAP_FAILED)
    {
        ERRNO("mmap(" DBFILE_MBDB ")");
        goto out;
    }

    uint8_t *data = addr;
    uint8_t * const end = data + flen;
    if(data + 6 > end)
    {
        LOG("mbdb: truncated at magic");
        goto out;
    }
    if(memcmp(data, "mbdb\x05\x00", 6) != 0)
    {
        LOG("mbdb: bad magic");
        goto out;
    }
    data += 6;

    while(data < end)
    {
        char *dummy  = NULL,
             *dom    = NULL,
             *path   = NULL;
        size_t dummyLen = 0,
               domLen   = 0,
               pathLen  = 0;
        uint16_t mode = 0;

        MBDB_STR(dom, domLen);
        MBDB_STR(path, pathLen);
        MBDB_STR(dummy, dummyLen);
        MBDB_STR(dummy, dummyLen);
        MBDB_STR(dummy, dummyLen);
        MBDB_UINT16(mode);
        data += 38;
        if(data > end)
        {
            LOG("mbdb: truncated at attributes");
            goto out;
        }
        size_t nprops = (uint8_t)data[-1];
        for(size_t i = 0; i < nprops; ++i)
        {
            MBDB_STR(dummy, dummyLen);
            MBDB_STR(dummy, dummyLen);
        }

        if((mode & S_IFMT) != S_IFREG)
        {
            continue;
        }
        fileID       = malloc(41),
        domain       = strndup(dom, domLen),
        relativePath = strndup(path, pathLen);
        if(!fileID || !domain || !relativePath)
        {
            ERRNO("strndup");
            goto out;
        }
        asprintf(&fullPath, "%s-%s", domain, relativePath);
        if(!fullPath)
        {
            ERRNO("asprintf");
            goto out;
        }
        CC_SHA1(fullPath, strlen(fullPath), (unsigned char*)(fileID + 20));
        for(size_t i = 0; i < 20; ++i)
        {
            uint8_t val = (uint8_t)fileID[20 + i],
                    hi  = (val & 0xf0) >> 4,
                    lo  = (val & 0x0f);
            fileID[2*i    ] = hi >= 10 ? 'a' + (hi - 10) : '0' + hi;
            fileID[2*i + 1] = lo >= 10 ? 'a' + (lo - 10) : '0' + lo;
        }
        fileID[40] = '\0';
        db_ent_t *ent = malloc(sizeof(db_ent_t));
        if(!ent)
        {
            ERRNO("malloc");
            goto out;
        }
        ent->next         = retval;
        ent->fileID       = fileID;
        ent->domain       = domain;
        ent->relativePath = relativePath;
        retval = ent;
        // Prevent freeing
        fileID = NULL;
        domain = NULL;
        relativePath = NULL;
        // Actually free
        free(fullPath);
        fullPath = NULL;
    }

    success = true;
out:;
    if(fileID) free(fileID);
    if(domain) free(domain);
    if(relativePath) free(relativePath);
    if(fullPath) free(fullPath);
    if(addr != MAP_FAILED) munmap(addr, flen);
    if(fd != -1) close(fd);
    if(!success)
    {
        db_cleanup(retval);
        retval = NULL;
    }
    return retval;
}

void db_cleanup(db_ent_t *head)
{
    while(head)
    {
        db_ent_t *next = head->next;
        free((void*)head->fileID);
        free((void*)head->domain);
        free((void*)head->relativePath);
        free(head);
        head = next;
    }
}
