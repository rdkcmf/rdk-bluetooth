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
/*btrCore_service.h
includes information for query of available services
*/

#ifndef __BTR_CORE_SERVICE_H__
#define __BTR_CORE_SERVICE_H__

/* Below are few of the shortened UUID per
 * https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
 */

#define BTR_CORE_SP_TEXT        "Serial Port"
#define BTR_CORE_SP             "0x1101"

#define BTR_CORE_HEADSET_TEXT   "Headset"
#define BTR_CORE_HEADSET        "0x1108"

#define BTR_CORE_A2SRC_TEXT     "Audio Source"
#define BTR_CORE_A2SRC          "0x110a"

#define BTR_CORE_A2SNK_TEXT     "Audio Sink"
#define BTR_CORE_A2SNK          "0x110b"

#define BTR_CORE_AVRTG_TEXT     "AV Remote Target"
#define BTR_CORE_AVRTG          "0x110c"

#define BTR_CORE_AAD_TEXT       "Advanced Audio Distribution"
#define BTR_CORE_AAD            "0x110d"

#define BTR_CORE_AVRCT_TEXT     "AV Remote"
#define BTR_CORE_AVRCT          "0x110e"

#define BTR_CORE_AVREMOTE_TEXT  "A/V Remote Control Controller"
#define BTR_CORE_AVREMOTE       "0x110F"

#define BTR_CORE_HS_AG_TEXT     "Headset - Audio Gateway (AG)"
#define BTR_CORE_HS_AG          "0x1112"

#define BTR_CORE_HANDSFREE_TEXT "Handsfree"
#define BTR_CORE_HANDSFREE      "0x111e"

#define BTR_CORE_HAG_TEXT       "Handsfree - Audio Gateway"
#define BTR_CORE_HAG            "0x111f"

#define BTR_CORE_HEADSET2_TEXT  "Headset - HS"
#define BTR_CORE_HEADSET2       "0x1131"

#define BTR_CORE_GEN_AUDIO_TEXT "GenericAudio"
#define BTR_CORE_GEN_AUDIO      "0x1203"

#define BTR_CORE_PNP_TEXT       "PnP Information"
#define BTR_CORE_PNP            "0x1200"

#define BTR_CORE_GEN_ATRIB_TEXT "Generic Attribute"
#define BTR_CORE_GEN_ATRIB      "0x1801"

#endif // __BTR_CORE_SERVICE_H__
