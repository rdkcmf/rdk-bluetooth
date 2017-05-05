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

#if 1
#define BTRCORELOG_ERROR(format...)       RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.BTRCORE", format)
#define BTRCORELOG_WARN(format...)        RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.BTRCORE", format)
#define BTRCORELOG_INFO(format...)        RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.BTRCORE", format)
#define BTRCORELOG_DEBUG(format...)       RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.BTRCORE", format)
#define BTRCORELOG_TRACE(format...)       RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.BTRCORE", format)
#else
#define BTRCORELOG_ERROR(format...)       fprintf (stderr, format)
#define BTRCORELOG_WARN(format...)        fprintf (stderr, format)
#define BTRCORELOG_INFO(format...)        fprintf (stderr, format)
#define BTRCORELOG_DEBUG(format...)       fprintf (stderr, format)
#define BTRCORELOG_TRACE(format...)       fprintf (stderr, format)
#endif

#endif /* __BTRCORE_PRIV_H__ */
