/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef __BTRCORE_PRIV_H__
#define __BTRCORE_PRIV_H__

#include "rdk_debug.h"

#define PREFIX(format)  "%d\t: %s - " format

#ifdef RDK_LOGGER_ENABLED

extern int b_rdk_logger_enabled;

#define LOG_ERROR(format, ...)   if(b_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.BTRCORE", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}   
#define LOG_WARN(format, ...)    if(b_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.BTRCORE", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_INFO(format, ...)    if(b_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.BTRCORE", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_DEBUG(format, ...)   if(b_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.BTRCORE", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#define LOG_TRACE(format, ...)   if(b_rdk_logger_enabled) {\
    RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.BTRCORE", format, __VA_ARGS__);\
    } else {\
    fprintf (stderr, format, __VA_ARGS__);\
}
#else

#define LOG_ERROR(format, ...)              fprintf(stderr, format, __VA_ARGS__)
#define LOG_WARN(format,  ...)              fprintf(stderr, format, __VA_ARGS__)
#define LOG_INFO(format,  ...)              fprintf(stderr, format, __VA_ARGS__)
#define LOG_DEBUG(format, ...)              fprintf(stderr, format, __VA_ARGS__)
#define LOG_TRACE(format, ...)              fprintf(stderr, format, __VA_ARGS__)

#endif


#define BTRCORELOG_ERROR(format, ...)       LOG_ERROR(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRCORELOG_WARN(format,  ...)       LOG_WARN(PREFIX(format),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRCORELOG_INFO(format,  ...)       LOG_INFO(PREFIX(format),  __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRCORELOG_DEBUG(format, ...)       LOG_DEBUG(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define BTRCORELOG_TRACE(format, ...)       LOG_TRACE(PREFIX(format), __LINE__, __FUNCTION__, ##__VA_ARGS__)



#endif /* __BTRCORE_PRIV_H__ */
