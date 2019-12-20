/*
 * Copyright (C) 2003-2010 Neverball authors
 *
 * NEVERBALL is  free software; you can redistribute  it and/or modify
 * it under the  terms of the GNU General  Public License as published
 * by the Free  Software Foundation; either version 2  of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT  ANY  WARRANTY;  without   even  the  implied  warranty  of
 * MERCHANTABILITY or  FITNESS FOR A PARTICULAR PURPOSE.   See the GNU
 * General Public License for more details.
 */

#ifdef NXDK
#include <hal/debug.h>
#endif

#ifndef _WIN32
#include <dirent.h>
#else
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dir.h"
#include "common.h"

#ifdef NXDK
#include <hal/debug.h>
#include <_windows.h>
#endif

List dir_list_files(const char *path)
{
    List files = NULL;
#ifndef _WIN32
    DIR *dir;

    if ((dir = opendir(path)))
    {
        struct dirent *ent;

        while ((ent = readdir(dir)))
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
                continue;

            files = list_cons(strdup(ent->d_name), files);
        }

        closedir(dir);
    }
#else
#ifndef NXDK
    debugPrint("Asking for files in '%s'\n", path);
#endif

    HANDLE hFind;
    WIN32_FIND_DATA FindFileData;

    hFind = FindFirstFile(path, &FindFileData);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        BOOL status = TRUE;
        while(status != FALSE)
        {
            //FIXME: Handle wide strings?
            files = list_cons(strdup(FindFileData.cFileName), files);

#ifndef NXDK
            debugPrint("Found '%s'\n", FindFileData.cFileName);
#endif

            FindNextFile(hFind, &FindFileData);
        }
        FindClose(hFind);
    }
#endif

    return files;
}

void dir_list_free(List files)
{
    while (files)
    {
        free(files->data);
        files = list_rest(files);
    }
}

static struct dir_item *add_item(Array items, const char *dir, const char *name)
{
    struct dir_item *item = array_add(items);

    item->path = path_join(dir, name);
    item->data = NULL;

    return item;
}

static void del_item(Array items)
{
    struct dir_item *item = array_get(items, array_len(items) - 1);

    free((void *) item->path);
    assert(!item->data);

    array_del(items);
}

Array dir_scan(const char *path,
               int  (*filter)    (struct dir_item *),
               List (*list_files)(const char *),
               void (*free_files)(List))
{
    List files, file;
    Array items = NULL;

    assert((list_files && free_files) || (!list_files && !free_files));

    if (!list_files) list_files = dir_list_files;
    if (!free_files) free_files = dir_list_free;

    items = array_new(sizeof (struct dir_item));

    if ((files = list_files(path)))
    {
        for (file = files; file; file = file->next)
        {
            struct dir_item *item;

            item = add_item(items, path, file->data);

            if (filter && !filter(item))
                del_item(items);
        }

        free_files(files);
    }

    return items;
}

void dir_free(Array items)
{
    while (array_len(items))
        del_item(items);

    array_free(items);
}

int dir_exists(const char *path)
{
#ifndef _WIN32
    DIR *dir;

    if ((dir = opendir(path)))
    {
        closedir(dir);
        return 1;
    }
    return 0;
#else
    DWORD dwAttrib = GetFileAttributes(path);

    return ((dwAttrib != INVALID_FILE_ATTRIBUTES) &&
           (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
#endif
}
