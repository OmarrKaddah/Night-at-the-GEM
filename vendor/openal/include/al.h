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

#ifndef AL_AL_H
#define AL_AL_H

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef AL_API
 #if defined(AL_LIBTYPE_STATIC)
  #define AL_API
 #elif defined(_WIN32)
  #define AL_API __declspec(dllimport)
 #else
  #define AL_API extern
 #endif
#endif

#if defined(_WIN32)
 #define AL_APIENTRY __cdecl
#else
 #define AL_APIENTRY
#endif

/* Type definitions */
typedef char ALboolean;
typedef char ALchar;
typedef signed char ALbyte;
typedef unsigned char ALubyte;
typedef short ALshort;
typedef unsigned short ALushort;
typedef int ALint;
typedef unsigned int ALuint;
typedef int ALsizei;
typedef int ALenum;
typedef float ALfloat;
typedef double ALdouble;
typedef void ALvoid;

/* Enumerant values */
#define AL_NONE                                   0
#define AL_FALSE                                  0
#define AL_TRUE                                   1

/* Source properties */
#define AL_SOURCE_RELATIVE                        0x202
#define AL_CONE_INNER_ANGLE                       0x1001
#define AL_CONE_OUTER_ANGLE                       0x1002
#define AL_PITCH                                  0x1003
#define AL_POSITION                               0x1004
#define AL_DIRECTION                              0x1005
#define AL_VELOCITY                               0x1006
#define AL_LOOPING                                0x1007
#define AL_BUFFER                                 0x1009
#define AL_GAIN                                   0x100A
#define AL_MIN_GAIN                               0x100D
#define AL_MAX_GAIN                               0x100E
#define AL_ORIENTATION                            0x100F
#define AL_SOURCE_STATE                           0x1010
#define AL_INITIAL                                0x1011
#define AL_PLAYING                                0x1012
#define AL_PAUSED                                 0x1013
#define AL_STOPPED                                0x1014
#define AL_BUFFERS_QUEUED                         0x1015
#define AL_BUFFERS_PROCESSED                      0x1016
#define AL_REFERENCE_DISTANCE                     0x1020
#define AL_ROLLOFF_FACTOR                         0x1021
#define AL_CONE_OUTER_GAIN                        0x1022
#define AL_MAX_DISTANCE                           0x1023
#define AL_SEC_OFFSET                             0x1024
#define AL_SAMPLE_OFFSET                          0x1025
#define AL_BYTE_OFFSET                            0x1026
#define AL_SOURCE_TYPE                            0x1027
#define AL_STATIC                                 0x1028
#define AL_STREAMING                              0x1029
#define AL_UNDETERMINED                           0x1030

/* Listener properties */
#define AL_METERS_PER_UNIT                        0x20004

/* Source EFX properties */
#define AL_DIRECT_FILTER                          0x20005
#define AL_AUXILIARY_SEND_FILTER                  0x20006
#define AL_AIR_ABSORPTION_FACTOR                  0x20007
#define AL_ROOM_ROLLOFF_FACTOR                    0x20008
#define AL_CONE_OUTER_GAINHF                      0x20009
#define AL_DIRECT_FILTER_GAINHF_AUTO              0x2000A
#define AL_AUXILIARY_SEND_FILTER_GAIN_AUTO        0x2000B
#define AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO      0x2000C

/* Buffer formats */
#define AL_FORMAT_MONO8                           0x1100
#define AL_FORMAT_MONO16                          0x1101
#define AL_FORMAT_STEREO8                         0x1102
#define AL_FORMAT_STEREO16                        0x1103

/* Buffer properties */
#define AL_FREQUENCY                              0x2001
#define AL_BITS                                   0x2002
#define AL_CHANNELS                               0x2003
#define AL_SIZE                                   0x2004

/* Buffer state */
#define AL_UNUSED                                 0x2010
#define AL_PENDING                                0x2011
#define AL_PROCESSED                              0x2012

/* Errors */
#define AL_NO_ERROR                               0
#define AL_INVALID_NAME                           0xA001
#define AL_INVALID_ENUM                           0xA002
#define AL_INVALID_VALUE                          0xA003
#define AL_INVALID_OPERATION                      0xA004
#define AL_OUT_OF_MEMORY                          0xA005

/* Context strings */
#define AL_VENDOR                                 0xB001
#define AL_VERSION                                0xB002
#define AL_RENDERER                               0xB003
#define AL_EXTENSIONS                             0xB004

/* Global tweakage */
#define AL_DOPPLER_FACTOR                         0xC000
#define AL_DOPPLER_VELOCITY                       0xC001
#define AL_SPEED_OF_SOUND                         0xC003
#define AL_DISTANCE_MODEL                         0xD000
#define AL_INVERSE_DISTANCE                       0xD001
#define AL_INVERSE_DISTANCE_CLAMPED               0xD002
#define AL_LINEAR_DISTANCE                        0xD003
#define AL_LINEAR_DISTANCE_CLAMPED                0xD004
#define AL_EXPONENT_DISTANCE                      0xD005
#define AL_EXPONENT_DISTANCE_CLAMPED              0xD006

/* Max sources */
#define AL_MAX_SOURCES                            0x20003

/* Renderer State management */
AL_API void AL_APIENTRY alEnable(ALenum capability);
AL_API void AL_APIENTRY alDisable(ALenum capability);
AL_API ALboolean AL_APIENTRY alIsEnabled(ALenum capability);

/* State retrieval */
AL_API const ALchar* AL_APIENTRY alGetString(ALenum param);
AL_API void AL_APIENTRY alGetBooleanv(ALenum param, ALboolean *values);
AL_API void AL_APIENTRY alGetIntegerv(ALenum param, ALint *values);
AL_API void AL_APIENTRY alGetFloatv(ALenum param, ALfloat *values);
AL_API void AL_APIENTRY alGetDoublev(ALenum param, ALdouble *values);
AL_API ALboolean AL_APIENTRY alGetBoolean(ALenum param);
AL_API ALint AL_APIENTRY alGetInteger(ALenum param);
AL_API ALfloat AL_APIENTRY alGetFloat(ALenum param);
AL_API ALdouble AL_APIENTRY alGetDouble(ALenum param);

/* Error retrieval */
AL_API ALenum AL_APIENTRY alGetError(void);

/* Extension support */
AL_API ALboolean AL_APIENTRY alIsExtensionPresent(const ALchar *extname);
AL_API void* AL_APIENTRY alGetProcAddress(const ALchar *fname);
AL_API ALenum AL_APIENTRY alGetEnumValue(const ALchar *ename);

/* Listener functions */
AL_API void AL_APIENTRY alListenerf(ALenum param, ALfloat value);
AL_API void AL_APIENTRY alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3);
AL_API void AL_APIENTRY alListenerfv(ALenum param, const ALfloat *values);
AL_API void AL_APIENTRY alListeneri(ALenum param, ALint value);
AL_API void AL_APIENTRY alListener3i(ALenum param, ALint value1, ALint value2, ALint value3);
AL_API void AL_APIENTRY alListeneriv(ALenum param, const ALint *values);
AL_API void AL_APIENTRY alGetListenerf(ALenum param, ALfloat *value);
AL_API void AL_APIENTRY alGetListener3f(ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3);
AL_API void AL_APIENTRY alGetListenerfv(ALenum param, ALfloat *values);
AL_API void AL_APIENTRY alGetListeneri(ALenum param, ALint *value);
AL_API void AL_APIENTRY alGetListener3i(ALenum param, ALint *value1, ALint *value2, ALint *value3);
AL_API void AL_APIENTRY alGetListeneriv(ALenum param, ALint *values);

/* Source functions */
AL_API void AL_APIENTRY alGenSources(ALsizei n, ALuint *sources);
AL_API void AL_APIENTRY alDeleteSources(ALsizei n, const ALuint *sources);
AL_API ALboolean AL_APIENTRY alIsSource(ALuint source);
AL_API void AL_APIENTRY alSourcef(ALuint source, ALenum param, ALfloat value);
AL_API void AL_APIENTRY alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3);
AL_API void AL_APIENTRY alSourcefv(ALuint source, ALenum param, const ALfloat *values);
AL_API void AL_APIENTRY alSourcei(ALuint source, ALenum param, ALint value);
AL_API void AL_APIENTRY alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3);
AL_API void AL_APIENTRY alSourceiv(ALuint source, ALenum param, const ALint *values);
AL_API void AL_APIENTRY alGetSourcef(ALuint source, ALenum param, ALfloat *value);
AL_API void AL_APIENTRY alGetSource3f(ALuint source, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3);
AL_API void AL_APIENTRY alGetSourcefv(ALuint source, ALenum param, ALfloat *values);
AL_API void AL_APIENTRY alGetSourcei(ALuint source, ALenum param, ALint *value);
AL_API void AL_APIENTRY alGetSource3i(ALuint source, ALenum param, ALint *value1, ALint *value2, ALint *value3);
AL_API void AL_APIENTRY alGetSourceiv(ALuint source, ALenum param, ALint *values);
AL_API void AL_APIENTRY alSourcePlayv(ALsizei n, const ALuint *sources);
AL_API void AL_APIENTRY alSourceStopv(ALsizei n, const ALuint *sources);
AL_API void AL_APIENTRY alSourceRewindv(ALsizei n, const ALuint *sources);
AL_API void AL_APIENTRY alSourcePausev(ALsizei n, const ALuint *sources);
AL_API void AL_APIENTRY alSourcePlay(ALuint source);
AL_API void AL_APIENTRY alSourceStop(ALuint source);
AL_API void AL_APIENTRY alSourceRewind(ALuint source);
AL_API void AL_APIENTRY alSourcePause(ALuint source);
AL_API void AL_APIENTRY alSourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers);
AL_API void AL_APIENTRY alSourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers);

/* Buffer functions */
AL_API void AL_APIENTRY alGenBuffers(ALsizei n, ALuint *buffers);
AL_API void AL_APIENTRY alDeleteBuffers(ALsizei n, const ALuint *buffers);
AL_API ALboolean AL_APIENTRY alIsBuffer(ALuint buffer);
AL_API void AL_APIENTRY alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei freq);
AL_API void AL_APIENTRY alBufferf(ALuint buffer, ALenum param, ALfloat value);
AL_API void AL_APIENTRY alBuffer3f(ALuint buffer, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3);
AL_API void AL_APIENTRY alBufferfv(ALuint buffer, ALenum param, const ALfloat *values);
AL_API void AL_APIENTRY alBufferi(ALuint buffer, ALenum param, ALint value);
AL_API void AL_APIENTRY alBuffer3i(ALuint buffer, ALenum param, ALint value1, ALint value2, ALint value3);
AL_API void AL_APIENTRY alBufferiv(ALuint buffer, ALenum param, const ALint *values);
AL_API void AL_APIENTRY alGetBufferf(ALuint buffer, ALenum param, ALfloat *value);
AL_API void AL_APIENTRY alGetBuffer3f(ALuint buffer, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3);
AL_API void AL_APIENTRY alGetBufferfv(ALuint buffer, ALenum param, ALfloat *values);
AL_API void AL_APIENTRY alGetBufferi(ALuint buffer, ALenum param, ALint *value);
AL_API void AL_APIENTRY alGetBuffer3i(ALuint buffer, ALenum param, ALint *value1, ALint *value2, ALint *value3);
AL_API void AL_APIENTRY alGetBufferiv(ALuint buffer, ALenum param, ALint *values);

/* Distance models */
AL_API void AL_APIENTRY alDistanceModel(ALenum distanceModel);
AL_API void AL_APIENTRY alDopplerFactor(ALfloat value);
AL_API void AL_APIENTRY alDopplerVelocity(ALfloat value);
AL_API void AL_APIENTRY alSpeedOfSound(ALfloat value);

#if defined(__cplusplus)
}
#endif

#endif /* AL_AL_H */