/* Bluetooth Core Types Header file */
#ifndef __BTR_CORE_TYPES_H__
#define __BTR_CORE_TYPES_H__

typedef enum _enBTRCoreRet {
    enBTRCoreFailure, 
    enBTRCoreInitFailure, 
    enBTRCoreNotInitialized, 
    enBTRCoreInvalidAdapter, 
    enBTRCorePairingFailed,
    enBTRCoreDiscoveryFailure, 
    enBTRCoreInvalidArg, 
    enBTRCoreSuccess
} enBTRCoreRet;


typedef unsigned long long int tBTRCoreDevId;

#endif // __BTR_CORE_TYPES_H__
