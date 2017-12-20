#ifndef METHOD_H
#define METHOD_H

#include "opcua_common.h"
#include "open62541.h"

#ifdef __cplusplus
extern "C" {
#endif

EdgeResult *executeMethod(UA_Client *client, EdgeMessage *msg);

#ifdef __cplusplus
}
#endif

#endif  // METHOD_H
