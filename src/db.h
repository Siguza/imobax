/* Copyright (c) 2019 Siguza
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Source Code Form is "Incompatible With Secondary Licenses", as
 * defined by the Mozilla Public License, v. 2.0.
**/

#ifndef DB_H
#define DB_H

#define DBFILE_SQLITE   "Manifest.db"
#define DBFILE_MBDB     "Manifest.mbdb"

typedef struct db_ent
{
    struct db_ent *next;
    const char *fileID,
               *domain,
               *relativePath;
} db_ent_t;

db_ent_t* db_getlist_sqlite3(int srcdir);
db_ent_t* db_getlist_mbdb(int srcdir);
void db_cleanup(db_ent_t *head);

#endif
