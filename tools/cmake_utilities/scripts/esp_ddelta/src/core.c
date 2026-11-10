/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Python bindings for ddelta encode/decode.
 *
 * The patch bytes are the uncompressed ddelta stream (DDELTA40). Compression
 * is intentionally left to the caller.
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "ddelta.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#ifdef _MSC_VER
#define esp_open _open
#define esp_close _close
#else
#define esp_open open
#define esp_close close
#endif
#else
#include <unistd.h>
#define esp_open open
#define esp_close close
#endif

static int open_read(const char *path)
{
#ifdef _WIN32
    return esp_open(path, _O_RDONLY | _O_BINARY);
#else
    return esp_open(path, O_RDONLY);
#endif
}

static int open_write(const char *path)
{
#ifdef _WIN32
    return esp_open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, 0644);
#else
    return esp_open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif
}

static PyObject *encode_paths(PyObject *self, PyObject *args)
{
    const char *old_path;
    const char *new_path;
    const char *patch_path;
    int oldfd;
    int newfd;
    int patchfd;
    int err;

    (void)self;
    if (!PyArg_ParseTuple(args, "sss", &old_path, &new_path, &patch_path)) {
        return NULL;
    }

    oldfd = open_read(old_path);
    if (oldfd < 0) {
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, old_path);
    }
    newfd = open_read(new_path);
    if (newfd < 0) {
        esp_close(oldfd);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, new_path);
    }
    patchfd = open_write(patch_path);
    if (patchfd < 0) {
        esp_close(oldfd);
        esp_close(newfd);
        return PyErr_SetFromErrnoWithFilename(PyExc_OSError, patch_path);
    }

    Py_BEGIN_ALLOW_THREADS
    err = ddelta_generate(oldfd, newfd, patchfd);
    Py_END_ALLOW_THREADS

    if (err < 0) {
        PyErr_Format(PyExc_RuntimeError, "ddelta encode failed (%d): %s", -err, strerror(errno));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *decode_paths(PyObject *self, PyObject *args)
{
    const char *old_path;
    const char *patch_path;
    const char *new_path;
    FILE *oldf = NULL;
    FILE *patchf = NULL;
    FILE *newf = NULL;
    struct ddelta_header header;
    int err = 0;

    (void)self;
    if (!PyArg_ParseTuple(args, "sss", &old_path, &patch_path, &new_path)) {
        return NULL;
    }

    oldf = fopen(old_path, "rb");
    patchf = fopen(patch_path, "rb");
    newf = fopen(new_path, "wb");
    if (oldf == NULL || patchf == NULL || newf == NULL) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto out;
    }

    err = ddelta_header_read(&header, patchf);
    if (err < 0) {
        PyErr_Format(PyExc_RuntimeError, "ddelta header read failed (%d)", -err);
        goto out;
    }

    Py_BEGIN_ALLOW_THREADS
    err = ddelta_apply(&header, patchf, oldf, newf);
    Py_END_ALLOW_THREADS

    if (err < 0) {
        PyErr_Format(PyExc_RuntimeError, "ddelta decode failed (%d)", -err);
        goto out;
    }

    if (oldf) {
        fclose(oldf);
        oldf = NULL;
    }
    if (patchf) {
        fclose(patchf);
        patchf = NULL;
    }
    if (newf) {
        fclose(newf);
        newf = NULL;
    }
    Py_RETURN_NONE;

out:
    if (oldf) {
        fclose(oldf);
    }
    if (patchf) {
        fclose(patchf);
    }
    if (newf) {
        fclose(newf);
    }
    return NULL;
}

static PyMethodDef methods[] = {
    {
        "encode_paths", encode_paths, METH_VARARGS,
        "encode_paths(old_path, new_path, patch_path) -> None"
    },
    {
        "decode_paths", decode_paths, METH_VARARGS,
        "decode_paths(old_path, patch_path, new_path) -> None"
    },
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_core",
    "Uncompressed ddelta encode and decode.",
    -1,
    methods
};

PyMODINIT_FUNC PyInit__core(void)
{
    return PyModule_Create(&moduledef);
}
