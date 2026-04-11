/*
 * EGL platform_base extension shim for libhybris
 *
 * The hybris EGL implementation does not advertise EGL_EXT_platform_base
 * in its client extension string, even though the underlying functions
 * (eglGetPlatformDisplayEXT, etc.) are present. This causes libepoxy
 * (used by Flutter and GDK) to refuse to resolve eglGetPlatformDisplayEXT,
 * breaking apps like FluffyChat.
 *
 * This LD_PRELOADed shim intercepts dlsym to wrap eglQueryString, injecting
 * EGL_EXT_platform_base into the client extension string.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EGL_EXTENSIONS 0x3055

typedef void* EGLDisplay;
typedef int EGLint;
typedef const char* (*eglQueryString_fn)(EGLDisplay, EGLint);

static eglQueryString_fn real_eglQueryString = NULL;
static char *patched_extensions = NULL;

static const char *wrapped_eglQueryString(EGLDisplay dpy, EGLint name) {
    if (!real_eglQueryString) return NULL;

    const char *result = real_eglQueryString(dpy, name);

    /* Intercept client extensions query (EGL_NO_DISPLAY + EGL_EXTENSIONS) */
    if (dpy == NULL && name == EGL_EXTENSIONS && result != NULL) {
        if (!strstr(result, "EGL_EXT_platform_base")) {
            if (!patched_extensions) {
                size_t len = strlen(result) + strlen(" EGL_EXT_platform_base") + 1;
                patched_extensions = malloc(len);
                if (patched_extensions)
                    snprintf(patched_extensions, len, "%s EGL_EXT_platform_base", result);
            }
            if (patched_extensions) return patched_extensions;
        }
    }
    return result;
}

void *dlsym(void *handle, const char *symbol) {
    static void* (*real_dlsym)(void*, const char*) = NULL;
    if (!real_dlsym) {
        real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.17");
        if (!real_dlsym) real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.34");
        if (!real_dlsym) real_dlsym = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.2.5");
    }

    void *result = real_dlsym(handle, symbol);

    if (result && symbol && strcmp(symbol, "eglQueryString") == 0) {
        real_eglQueryString = (eglQueryString_fn)result;
        return (void*)wrapped_eglQueryString;
    }

    return result;
}
