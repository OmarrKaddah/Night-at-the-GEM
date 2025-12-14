/**
 * OpenAL cross platform audio library
 * Copyright (C) 2008 by authors.
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 */

#ifndef AL_ALC_H
#define AL_ALC_H

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef ALC_API
 #if defined(AL_LIBTYPE_STATIC)
  #define ALC_API
 #elif defined(_WIN32)
  #define ALC_API __declspec(dllimport)
 #else
  #define ALC_API extern
 #endif
#endif

#if defined(_WIN32)
 #define ALC_APIENTRY __cdecl
#else
 #define ALC_APIENTRY
#endif

/* Type definitions */
typedef char ALCboolean;
typedef char ALCchar;
typedef signed char ALCbyte;
typedef unsigned char ALCubyte;
typedef short ALCshort;
typedef unsigned short ALCushort;
typedef int ALCint;
typedef unsigned int ALCuint;
typedef int ALCsizei;
typedef int ALCenum;
typedef float ALCfloat;
typedef double ALCdouble;
typedef void ALCvoid;

/* Device type */
typedef struct ALCdevice ALCdevice;
/* Context type */
typedef struct ALCcontext ALCcontext;

/* Boolean values */
#define ALC_FALSE                                 0
#define ALC_TRUE                                  1

/* Context attribute list terminators */
#define ALC_INVALID                               0

/* Context attributes */
#define ALC_FREQUENCY                             0x1007
#define ALC_REFRESH                               0x1008
#define ALC_SYNC                                  0x1009
#define ALC_MONO_SOURCES                          0x1010
#define ALC_STEREO_SOURCES                        0x1011

/* Errors */
#define ALC_NO_ERROR                              0
#define ALC_INVALID_DEVICE                        0xA001
#define ALC_INVALID_CONTEXT                       0xA002
#define ALC_INVALID_ENUM                          0xA003
#define ALC_INVALID_VALUE                         0xA004
#define ALC_OUT_OF_MEMORY                         0xA005

/* Device specifiers */
#define ALC_DEFAULT_DEVICE_SPECIFIER              0x1004
#define ALC_DEVICE_SPECIFIER                      0x1005
#define ALC_EXTENSIONS                            0x1006
#define ALC_ALL_DEVICES_SPECIFIER                 0x1013
#define ALC_DEFAULT_ALL_DEVICES_SPECIFIER         0x1012

/* Capture extension */
#define ALC_CAPTURE_DEVICE_SPECIFIER              0x310
#define ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER      0x311
#define ALC_CAPTURE_SAMPLES                       0x312

/* Major and minor version numbers */
#define ALC_MAJOR_VERSION                         0x1000
#define ALC_MINOR_VERSION                         0x1001
#define ALC_ATTRIBUTES_SIZE                       0x1002
#define ALC_ALL_ATTRIBUTES                        0x1003

/* Context management */
ALC_API ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint *attrlist);
ALC_API ALCboolean  ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context);
ALC_API void        ALC_APIENTRY alcProcessContext(ALCcontext *context);
ALC_API void        ALC_APIENTRY alcSuspendContext(ALCcontext *context);
ALC_API void        ALC_APIENTRY alcDestroyContext(ALCcontext *context);
ALC_API ALCcontext* ALC_APIENTRY alcGetCurrentContext(void);
ALC_API ALCdevice*  ALC_APIENTRY alcGetContextsDevice(ALCcontext *context);

/* Device management */
ALC_API ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *devicename);
ALC_API ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device);

/* Error support */
ALC_API ALCenum ALC_APIENTRY alcGetError(ALCdevice *device);

/* Extension support */
ALC_API ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname);
ALC_API void*      ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcname);
ALC_API ALCenum    ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumname);

/* Query functions */
ALC_API const ALCchar* ALC_APIENTRY alcGetString(ALCdevice *device, ALCenum param);
ALC_API void           ALC_APIENTRY alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values);

/* Capture functions */
ALC_API ALCdevice* ALC_APIENTRY alcCaptureOpenDevice(const ALCchar *devicename, ALCuint frequency, ALCenum format, ALCsizei buffersize);
ALC_API ALCboolean ALC_APIENTRY alcCaptureCloseDevice(ALCdevice *device);
ALC_API void       ALC_APIENTRY alcCaptureStart(ALCdevice *device);
ALC_API void       ALC_APIENTRY alcCaptureStop(ALCdevice *device);
ALC_API void       ALC_APIENTRY alcCaptureSamples(ALCdevice *device, ALCvoid *buffer, ALCsizei samples);

#if defined(__cplusplus)
}
#endif

#endif /* AL_ALC_H */