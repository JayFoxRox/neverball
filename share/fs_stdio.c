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
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "fs.h"
#include "dir.h"
#include "array.h"
#include "list.h"
#include "common.h"

/*
 * This file implements the low-level virtual file system routines
 * using stdio.
 */

/*---------------------------------------------------------------------------*/

struct fs_file_s
{
    FILE *handle;
};

static char *fs_dir_base;
static char *fs_dir_write;
static List  fs_path;

int fs_init(const char *argv0)
{
#ifdef NXDK
    fs_dir_base = "";
#else
    fs_dir_base  = strdup(argv0 && *argv0 ? dir_name(argv0) : ".");
#endif
    fs_dir_write = NULL;
    fs_path      = NULL;

    return 1;
}

int fs_quit(void)
{
    if (fs_dir_base)
    {
        free(fs_dir_base);
        fs_dir_base = NULL;
    }

    if (fs_dir_write)
    {
        free(fs_dir_write);
        fs_dir_write = NULL;
    }

    while (fs_path)
    {
        free(fs_path->data);
        fs_path = list_rest(fs_path);
    }

    return 1;
}

const char *fs_error(void)
{
    return strerror(errno);
}

/*---------------------------------------------------------------------------*/

const char *fs_base_dir(void)
{
    return fs_dir_base;
}

int fs_add_path(const char *path)
{
    /* TODO: ZIP archive support. */

    if (dir_exists(path))
    {
        fs_path = list_cons(strdup(path), fs_path);
        return 1;
    }
    return 0;
}

int fs_set_write_dir(const char *path)
{
    if (dir_exists(path))
    {
        if (fs_dir_write)
        {
            free(fs_dir_write);
            fs_dir_write = NULL;
        }

        fs_dir_write = strdup(path);
        return 1;
    }
    return 0;
}

const char *fs_get_write_dir(void)
{
    return fs_dir_write;
}

/*---------------------------------------------------------------------------*/

static void add_files(List *items, const char *real)
{
    List files, file;

    if ((files = dir_list_files(real)))
    {
        for (file = files; file; file = file->next)
        {
            int skip = 0;
            List p, l;

            /* "Inspired" by PhysicsFS file enumeration code. */

            for (p = NULL, l = *items; l; p = l, l = l->next)
            {
                int cmp;

                if ((cmp = strcmp(l->data, file->data)) >= 0)
                {
                    skip = (cmp == 0);
                    break;
                }
            }

            if (!skip)
            {
                if (p)
                    p->next = list_cons(file->data, p->next);
                else
                    *items = list_cons(file->data, *items);

                /* Take over memory management duties. */

                file->data = NULL;
            }
        }

        dir_list_free(files);
    }
}

static List list_files(const char *path)
{
    List files = NULL;
    List p;

    for (p = fs_path; p; p = p->next)
    {
        char *real = path_join(p->data, path);
        add_files(&files, real);
        free(real);
    }

    return files;
}

static void free_files(List files)
{
    while (files)
    {
        free(files->data);
        files = list_rest(files);
    }
}

Array fs_dir_scan(const char *path, int (*filter)(struct dir_item *))
{
    return dir_scan(path, filter, list_files, free_files);
}

void fs_dir_free(Array items)
{
    dir_free(items);
}

/*---------------------------------------------------------------------------*/
#include <assert.h>
static char *real_path(const char *path)
{
    char *real = NULL;
    List p;

    for (p = fs_path; p; p = p->next)
    {
        real = path_join(p->data, path);

        if (file_exists(real)) {
#ifdef NXDK
debugPrint("found '%s'\n", real);
#else
printf("found '%s'\n", real);
#endif
            break;
        }

        free(real);
        real = NULL;
    }

    return real;
}

/*---------------------------------------------------------------------------*/

fs_file fs_open_read(const char *path)
{
    fs_file fh;

    if ((fh = calloc(1, sizeof (*fh))))
    {
        char *real;

#ifdef NXDK
debugPrint("Path: %s\n", path);
#endif
        if ((real = real_path(path)))
        {
#ifdef NXDK
debugPrint("Trying to open '%s' (read)\n", real);
#endif
            fh->handle = fopen(real, "rb");

assert(fh->handle
|| !strcmp(real, "./Neverball/neverballrc")
|| !strcmp(real, "./Neverball/lang/.txt")
);

            free(real);
        }

        if (!fh->handle)
        {
            free(fh);
            fh = NULL;
        }
    }
    return fh;
}

static fs_file fs_open_write_flags(const char *path, int append)
{
    fs_file fh = NULL;

    if (fs_dir_write && path && *path)
    {
        if ((fh = calloc(1, sizeof (*fh))))
        {
            char *real;

            if ((real = path_join(fs_dir_write, path)))
            {
#ifdef NXDK
debugPrint("Trying to open '%s' (write)\n", real);
#endif
                fh->handle = fopen(real, append ? "ab" : "wb");

assert(fh->handle
|| !strcmp(real, "./Neverball/neverball.log")
|| !strcmp(real, "./Neverball/Replays/Last.nbr")
);

                free(real);
            }

            if (!fh->handle)
            {
                free(fh);
                fh = NULL;
            }
        }
    }
    return fh;
}

fs_file fs_open_write(const char *path)
{
    return fs_open_write_flags(path, 0);
}

fs_file fs_open_append(const char *path)
{
    return fs_open_write_flags(path, 1);
}

int fs_close(fs_file fh)
{
    if (fclose(fh->handle))
    {
        free(fh);
        return 1;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/

int fs_mkdir(const char *path)
{
    int success = 0;

    if (fs_dir_write)
    {
        char *real = path_join(fs_dir_write, path);
        success = dir_make(real) == 0;
        free((void *) real);
    }

    return success;
}

int fs_exists(const char *path)
{
    char *real;

    if ((real = real_path(path)))
    {
        free(real);
        return 1;
    }
    return 0;
}

int fs_remove(const char *path)
{
    int success = 0;

    if (fs_dir_write)
    {
        char *real = path_join(fs_dir_write, path);
        success = (remove(real) == 0);
        free(real);
    }

    return success;
}

/*---------------------------------------------------------------------------*/

int fs_read(void *data, int size, int count, fs_file fh)
{
    return fread(data, size, count, fh->handle);
}

int fs_write(const void *data, int size, int count, fs_file fh)
{
    return fwrite(data, size, count, fh->handle);
}

int fs_flush(fs_file fh)
{
    return fflush(fh->handle);
}

long fs_tell(fs_file fh)
{
    return ftell(fh->handle);
}

int fs_seek(fs_file fh, long offset, int whence)
{
    return fseek(fh->handle, offset, whence);
}

int fs_eof(fs_file fh)
{
    /*
     * Unlike PhysicsFS, stdio does not register EOF unless we have
     * actually attempted to read past the end of the file.  Nothing
     * is done to mitigate this: instead, code that relies on
     * PhysicsFS behavior should be fixed not to.
     */
    return feof(fh->handle);
}

int fs_size(const char *path)
{
    int size = 0;
    char *real;

    if ((real = real_path(path)))
    {
        size = file_size(real);
        free(real);
    }
    return size;
}

/*---------------------------------------------------------------------------*/
