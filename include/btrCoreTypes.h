/* Bluetooth Core Types Header file */
#ifndef __BTR_CORE_TYPES_H__
#define __BTR_CORE_TYPES_H__


#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

typedef enum _BOOLEAN {
    FALSE,
    TRUE
} BOOLEAN;


typedef void* tBTRCoreHandle;

typedef unsigned long long int tBTRCoreDevId;


typedef enum _enBTRCoreRet {
    enBTRCoreFailure, 
    enBTRCoreInitFailure, 
    enBTRCoreNotInitialized, 
    enBTRCoreInvalidAdapter, 
    enBTRCorePairingFailed,
    enBTRCoreDiscoveryFailure, 
    enBTRCoreDeviceNotFound, 
    enBTRCoreInvalidArg, 
    enBTRCoreSuccess
} enBTRCoreRet;


#endif // __BTR_CORE_TYPES_H__
