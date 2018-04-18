/******************************************************************
 *
 * Copyright 2017 Samsung Electronics All Rights Reserved.
 *
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include "browse_common.h"
#include "edge_node_type.h"
#include "edge_logger.h"
#include "edge_malloc.h"
#include "edge_open62541.h"
#include "message_dispatcher.h"
#include "command_adapter.h"

#include <inttypes.h>
#include <string.h>

#define TAG "browse_common"

#define GUID_LENGTH (36)

static const int BROWSE_NODECLASS_MASK = UA_NODECLASS_OBJECT | UA_NODECLASS_VARIABLE
    | UA_NODECLASS_VIEW | UA_NODECLASS_METHOD;
static const int VIEW_NODECLASS_MASK = UA_NODECLASS_OBJECT | UA_NODECLASS_VIEW;
static const int SHOW_SPECIFIC_NODECLASS_MASK = UA_NODECLASS_VARIABLE | UA_NODECLASS_VIEW | UA_NODECLASS_METHOD;
static const int SHOW_SPECIFIC_NODECLASS = false;

static const char WELL_KNOWN_LOCALHOST_URI_VALUE[] = "opc.tcp://localhost";

static response_cb_t g_responseCallback = NULL;

browsePathNode *browsePathNodeListHead = NULL, *browsePathNodeListTail = NULL;

void setErrorResponseCallback(response_cb_t callback) {
    g_responseCallback = callback;
}

NodesToBrowse_t *initNodesToBrowse(size_t size)
{
    NodesToBrowse_t *browseNodesInfo = (NodesToBrowse_t *) EdgeCalloc(1, sizeof(NodesToBrowse_t));
    VERIFY_NON_NULL_MSG(browseNodesInfo, "Failed EdgeCalloc for browseNodesInfo\n", NULL);
    browseNodesInfo->size = size;

    if(size > 0)
    {
        browseNodesInfo->nodeId = (UA_NodeId *) EdgeCalloc(size, sizeof(UA_NodeId));
        if (IS_NULL(browseNodesInfo->nodeId))
        {
            EDGE_LOG(TAG, "Memory allocation failed for browseNodesInfo->nodeId.");
            EdgeFree(browseNodesInfo);
            return NULL;
        }

        browseNodesInfo->browseName = (unsigned char **) calloc(size, sizeof(unsigned char *));
        if (IS_NULL(browseNodesInfo->browseName))
        {
            EDGE_LOG(TAG, "Memory allocation failed for browseNodesInfo->browseName.");
            EdgeFree(browseNodesInfo->nodeId);
            EdgeFree(browseNodesInfo);
            return NULL;
        }
    }
    return browseNodesInfo;
}

void destroyNodesToBrowse(NodesToBrowse_t *ptr, bool deleteNodeId)
{
    VERIFY_NON_NULL_NR_MSG(ptr, "Null Pointer received\n");

    for(int i = 0; i < ptr->size; ++i)
    {
        if(deleteNodeId)
            UA_NodeId_deleteMembers(ptr->nodeId + i);
        EdgeFree(ptr->browseName[i]);
    }
    EdgeFree(ptr->nodeId);
    EdgeFree(ptr->browseName);
    EdgeFree(ptr);
}

static unsigned char *convertUAStringToUnsignedChar(UA_String *uaStr)
{
    if (!uaStr || uaStr->length <= 0)
    {
        return NULL;
    }

    unsigned char *str = (unsigned char *) EdgeCalloc(uaStr->length+1, sizeof(unsigned char));
    VERIFY_NON_NULL_MSG(str, "EdgeCalloc FAILED for  unsigned char String\n", NULL);

    for (int i = 0; i < uaStr->length; ++i)
    {
        str[i] = uaStr->data[i];
    }
    return str;
}

static EdgeContinuationPointList *getContinuationPointList(UA_String *uaStr,
        unsigned char *browsePrefix)
{
    VERIFY_NON_NULL_MSG(uaStr, "UA_String received is NULL\n", NULL);

    EdgeContinuationPointList *cpList = (EdgeContinuationPointList *) EdgeCalloc(1,
            sizeof(EdgeContinuationPoint));
    VERIFY_NON_NULL_MSG(cpList, "EdgeCalloc FAILED for EdgeContinuationPointList\n", NULL);

    cpList->count = 1;
    cpList->cp = (EdgeContinuationPoint **) EdgeCalloc(cpList->count, sizeof(EdgeContinuationPoint *));
    if (!cpList->cp)
    {
        freeEdgeContinuationPointList(cpList);
        return NULL;
    }

    cpList->cp[0] = (EdgeContinuationPoint *) EdgeCalloc(1, sizeof(EdgeContinuationPoint));
    if (!cpList->cp[0])
    {
        freeEdgeContinuationPointList(cpList);
        return NULL;
    }

    cpList->cp[0]->continuationPoint = convertUAStringToUnsignedChar(uaStr);
    if (!cpList->cp[0]->continuationPoint)
    {
        freeEdgeContinuationPointList(cpList);
        return NULL;
    }

    if(IS_NOT_NULL(browsePrefix))
        cpList->cp[0]->browsePrefix = cloneData(browsePrefix, strlen((char *)browsePrefix)+1);

    cpList->cp[0]->length = uaStr->length;
    return cpList;
}

static UA_ByteString *getUAStringFromEdgeContinuationPoint(EdgeContinuationPoint *cp)
{
    if (IS_NULL(cp) || cp->length < 1)
    {
        return NULL;
    }

    UA_ByteString *byteStr = (UA_ByteString *) EdgeMalloc(sizeof(UA_ByteString));
    VERIFY_NON_NULL_MSG(byteStr, "EdgeMalloc FAILED for UA Bytestring\n", NULL);

    byteStr->length = cp->length;
    byteStr->data = (UA_Byte *) EdgeMalloc(byteStr->length * sizeof(UA_Byte));
    if (IS_NULL(byteStr->data))
    {
        EdgeFree(byteStr);
        return NULL;
    }

    for (int i = 0; i < byteStr->length; ++i)
    {
        byteStr->data[i] = cp->continuationPoint[i];
    }

    return byteStr;
}

UA_NodeId *getNodeId(EdgeRequest *req)
{
    VERIFY_NON_NULL_MSG(req, "EdgeRequest is NULL\n", NULL);
    VERIFY_NON_NULL_MSG(req->nodeInfo, "EdgeRequest NodeInfo is NULL\n", NULL);
    VERIFY_NON_NULL_MSG(req->nodeInfo->nodeId, "EdgeRequest NodeId is NULL\n", NULL);

    UA_NodeId *node = (UA_NodeId *) EdgeCalloc(1, sizeof(UA_NodeId));
    VERIFY_NON_NULL_MSG(node, "EdgeCalloc FAILED for UA Node Id\n", NULL);
    if (req->nodeInfo->nodeId->type == INTEGER)
    {
        *node = UA_NODEID_NUMERIC(req->nodeInfo->nodeId->nameSpace,
                req->nodeInfo->nodeId->integerNodeId);
    }
    else if (req->nodeInfo->nodeId->type == STRING)
    {
        *node = UA_NODEID_STRING_ALLOC(req->nodeInfo->nodeId->nameSpace,
                req->nodeInfo->nodeId->nodeId);
    }
    else
    {
        *node = UA_NODEID_NUMERIC(req->nodeInfo->nodeId->nameSpace, UA_NS0ID_ROOTFOLDER);
    }
    return node;
}

UA_NodeId *getNodeIdMultiReq(EdgeMessage *msg, int reqId)
{
    VERIFY_NON_NULL_MSG(msg, "EdgeMessage Parameter is NULL\n", NULL);
    return getNodeId(msg->requests[reqId]);
}

static EdgeNodeInfo *getEndpoint(EdgeMessage *msg, int msgId)
{
    if (msg->type == SEND_REQUEST)
    {
        return msg->request->nodeInfo;
    }
    return msg->requests[msgId]->nodeInfo;
}

static bool checkStatusGood(UA_StatusCode status)
{
    return (UA_STATUSCODE_GOOD == status) ? true : false;
}

static UA_BrowseDescription *getBrowseDescriptions(NodesToBrowse_t *browseNodesInfo,
        EdgeMessage *msg, UA_UInt32 nodeClassMask)
{
    VERIFY_NON_NULL_MSG(msg, "EdgeMessage parameter is NULL\n", NULL);
    VERIFY_NON_NULL_MSG(msg->browseParam, "EdgeMessage BrowseParam is NULL\n", NULL);
    VERIFY_NON_NULL_MSG(browseNodesInfo, "BrowseNodeInfo Parameter is NULL\n", NULL);

    int direct = msg->browseParam->direction;
    UA_BrowseDirection directionParam = UA_BROWSEDIRECTION_FORWARD;
    if (DIRECTION_INVERSE == direct)
    {
        directionParam = UA_BROWSEDIRECTION_INVERSE;
    }
    else if (DIRECTION_BOTH == direct)
    {
        directionParam = UA_BROWSEDIRECTION_BOTH;
    }

    UA_BrowseDescription *browseDesc = (UA_BrowseDescription *) UA_calloc(browseNodesInfo->size,
            sizeof(UA_BrowseDescription));
    VERIFY_NON_NULL_MSG(browseDesc, "UA CALLOC FAILED for UA Browse Desc\n", NULL);

    for (size_t idx = 0; idx < browseNodesInfo->size; ++idx)
    {
        browseDesc[idx].nodeId = browseNodesInfo->nodeId[idx];
        browseDesc[idx].browseDirection = directionParam;
        browseDesc[idx].referenceTypeId = UA_NODEID_NUMERIC(SYSTEM_NAMESPACE_INDEX,
        UA_NS0ID_REFERENCES);
        browseDesc[idx].includeSubtypes = true;
        browseDesc[idx].nodeClassMask = nodeClassMask;
        browseDesc[idx].resultMask = UA_BROWSERESULTMASK_ALL;
    }
    return browseDesc;
}

void invokeErrorCb(uint32_t srcMsgId, EdgeNodeId *srcNodeId,
        EdgeStatusCode edgeResult, const char *versatileValue)
{
    EdgeMessage *resultMsg = (EdgeMessage *) EdgeCalloc(1, sizeof(EdgeMessage));
    VERIFY_NON_NULL_NR_MSG(resultMsg, "EdgeCalloc FAILED for EdgeMessage in invokeErrorCb\n");

    resultMsg->message_id = srcMsgId; // Error message corresponds to the request message with the given message id.
    resultMsg->endpointInfo = (EdgeEndPointInfo *) EdgeCalloc(1, sizeof(EdgeEndPointInfo));
    if (IS_NULL(resultMsg->endpointInfo))
    {
        goto EXIT;
    }

    resultMsg->endpointInfo->endpointUri = cloneString(WELL_KNOWN_LOCALHOST_URI_VALUE);
    if(IS_NULL(resultMsg->endpointInfo->endpointUri))
    {
        goto EXIT;
    }

    resultMsg->type = ERROR;
    resultMsg->result = createEdgeResult(edgeResult);
    if (IS_NULL(resultMsg->result))
    {
        goto EXIT;
    }

    resultMsg->responses = (EdgeResponse **) EdgeCalloc(1, sizeof(EdgeResponse *));
    if (IS_NULL(resultMsg->responses))
    {
        goto EXIT;
    }

    resultMsg->responses[0] = (EdgeResponse *) EdgeCalloc(1, sizeof(EdgeResponse));
    if (IS_NULL(resultMsg->responses[0]))
    {
        goto EXIT;
    }

    resultMsg->responses[0]->message = (EdgeVersatility *) EdgeCalloc(1, sizeof(EdgeVersatility));
    if (IS_NULL(resultMsg->responses[0]->message))
    {
        goto EXIT;
    }
    resultMsg->responses[0]->message->isArray = false;
    resultMsg->responses[0]->message->value = cloneString(versatileValue);

    if (srcNodeId)
    {
        resultMsg->responses[0]->nodeInfo = (EdgeNodeInfo *) EdgeCalloc(1, sizeof(EdgeNodeInfo));
        if (IS_NULL(resultMsg->responses[0]->nodeInfo))
        {
            goto EXIT;
        }
        resultMsg->responses[0]->nodeInfo->nodeId = srcNodeId;
    }

    resultMsg->responseLength = 1;

    if (IS_NOT_NULL(g_responseCallback))
    {
        g_responseCallback(resultMsg);
    }

    if (resultMsg->responses[0]->nodeInfo != NULL)
    {
        resultMsg->responses[0]->nodeInfo->nodeId = NULL;
    }

EXIT:
    // Deallocate memory.
    freeEdgeMessage(resultMsg);
}

static bool checkContinuationPoint(uint32_t msgId, UA_BrowseResult browseResult,
        EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    /*if(browseResult.continuationPoint.length <= 0)
     {
     EDGE_LOG(TAG, "Error: " CONTINUATIONPOINT_EMPTY);
     invokeErrorCb(srcNodeId, STATUS_ERROR, CONTINUATIONPOINT_EMPTY);
     retVal = false;
     }
     else*/if (browseResult.continuationPoint.length >= 1000)
    {
        EDGE_LOG(TAG, "Error: " CONTINUATIONPOINT_LONG);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, CONTINUATIONPOINT_LONG);
        retVal = false;
    }
    else if (browseResult.continuationPoint.length > 0
            && (browseResult.referencesSize <= 0 || !browseResult.references))
    {
        EDGE_LOG(TAG, "Error: " STATUS_VIEW_REFERENCE_DATA_INVALID_VALUE);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, STATUS_VIEW_REFERENCE_DATA_INVALID_VALUE);
        retVal = false;
    }
    return retVal;
}

static bool checkBrowseName(uint32_t msgId, UA_String browseName, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if (browseName.length <= 0 || NULL == browseName.data)
    {
        EDGE_LOG(TAG, "Error: " BROWSENAME_EMPTY);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, BROWSENAME_EMPTY);
        retVal = false;
    }
    else if (browseName.length >= 1000)
    {
        EDGE_LOG(TAG, "Error: " BROWSENAME_LONG);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, BROWSENAME_LONG);
        retVal = false;
    }

    return retVal;
}

static bool checkNodeClass(uint32_t msgId, UA_NodeClass nodeClass, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if (false == isNodeClassValid(nodeClass))
    {
        EDGE_LOG(TAG, "Error: " NODECLASS_INVALID);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, NODECLASS_INVALID);
        retVal = false;
    }
    else if (UA_NODECLASS_UNSPECIFIED != BROWSE_NODECLASS_MASK &&
        (nodeClass & BROWSE_NODECLASS_MASK) == 0)
    {
        EDGE_LOG(TAG, "Error: " STATUS_VIEW_NOTINCLUDE_NODECLASS_VALUE);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, STATUS_VIEW_NOTINCLUDE_NODECLASS_VALUE);
        retVal = false;
    }
    return retVal;
}

static bool checkDisplayName(uint32_t msgId, UA_String displayName, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if (displayName.length <= 0 || NULL == displayName.data)
    {
        EDGE_LOG(TAG, "Error: " DISPLAYNAME_EMPTY);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, DISPLAYNAME_EMPTY);
        retVal = false;
    }
    else if (displayName.length >= 1000)
    {
        EDGE_LOG(TAG, "Error: " DISPLAYNAME_LONG);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, DISPLAYNAME_LONG);
        retVal = false;
    }

    return retVal;
}

static bool checkNodeId(uint32_t msgId, UA_ExpandedNodeId nodeId, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if (UA_NodeId_isNull(&nodeId.nodeId))
    {
        EDGE_LOG(TAG, "Error: " NODEID_NULL);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, NODEID_NULL);
        retVal = false;
    }
    else if (nodeId.serverIndex != 0)
    {
        EDGE_LOG(TAG, "Error: " NODEID_SERVERINDEX);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, NODEID_SERVERINDEX);
        retVal = false;
    }
    return retVal;
}

static bool checkReferenceTypeId(uint32_t msgId, UA_NodeId nodeId, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if (UA_NodeId_isNull(&nodeId))
    {
        EDGE_LOG(TAG, "Error: " REFERENCETYPEID_NULL);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, REFERENCETYPEID_NULL);
        retVal = false;
    }
    return retVal;
}

static bool checkTypeDefinition(uint32_t msgId, UA_ReferenceDescription *ref, EdgeNodeId *srcNodeId)
{
    bool retVal = true;
    if ((ref->nodeClass == UA_NODECLASS_OBJECT || ref->nodeClass == UA_NODECLASS_VARIABLE)
            && UA_NodeId_isNull(&ref->typeDefinition.nodeId))
    {
        EDGE_LOG(TAG, "Error: " TYPEDEFINITIONNODEID_NULL);
        invokeErrorCb(msgId, srcNodeId, STATUS_ERROR, TYPEDEFINITIONNODEID_NULL);
        retVal = false;
    }
    return retVal;
}

static void invokeResponseCb(EdgeMessage *msg, int msgId, EdgeNodeId *srcNodeId,
        EdgeBrowseResult *browseResult, size_t size, const unsigned char *browsePath, char *valueAlias)
{
    VERIFY_NON_NULL_NR_MSG(browseResult, "EdgeBrowseResult Param is NULL\n");
    VERIFY_NON_NULL_NR_MSG(browseResult->browseName, "EdgeBrowseResult.BrowseName is NULL\n");

    EdgeMessage *resultMsg = (EdgeMessage *) EdgeCalloc(1, sizeof(EdgeMessage));
    VERIFY_NON_NULL_NR_MSG(resultMsg, "EdgeCalloc Failed for EdgeMessage in invokeResponseCb\n");

    resultMsg->type = BROWSE_RESPONSE;
    resultMsg->message_id = msg->message_id;
    resultMsg->endpointInfo = cloneEdgeEndpointInfo(msg->endpointInfo);
    if (IS_NULL(resultMsg->endpointInfo))
    {
        EDGE_LOG(TAG, "Failed to clone the EdgeEndpointInfo.");
        goto ERROR;
    }

    resultMsg->responses = (EdgeResponse **) EdgeCalloc (1, sizeof(EdgeResponse *));
    if (IS_NULL(resultMsg->responses))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        goto ERROR;
    }
    resultMsg->responseLength = 1;
    resultMsg->responses[0] = (EdgeResponse *) EdgeCalloc(1, sizeof(EdgeResponse));
    if (IS_NULL(resultMsg->responses[0]))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        goto ERROR;
    }

    if(IS_NOT_NULL(browsePath))
    {
        resultMsg->responses[0]->message = (EdgeVersatility *) EdgeCalloc(1, sizeof(EdgeVersatility));
        if (IS_NULL(resultMsg->responses[0]->message))
        {
            EDGE_LOG(TAG, "Memory allocation failed.");
            goto ERROR;
        }
        resultMsg->responses[0]->message->isArray = false;
        resultMsg->responses[0]->message->value = (unsigned char *)cloneData(browsePath, strlen((char *)browsePath)+1);
        if(IS_NULL(resultMsg->responses[0]->message->value))
        {
            EDGE_LOG(TAG, "Memory allocation failed.");
            goto ERROR;
        }
    }

    resultMsg->responses[0]->nodeInfo = (EdgeNodeInfo *) EdgeCalloc(1, sizeof(EdgeNodeInfo));
    if (IS_NULL(resultMsg->responses[0]->nodeInfo))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        goto ERROR;
    }
    resultMsg->responses[0]->nodeInfo->nodeId = cloneEdgeNodeId(srcNodeId);               //srcNodeId;
    resultMsg->responses[0]->nodeInfo->valueAlias = (char *)cloneData(valueAlias, strlen((char *)valueAlias)+1); //valueAlias;
    resultMsg->responses[0]->requestId = msgId; // Response for msgId'th request.
    resultMsg->browseResult = (EdgeBrowseResult *) EdgeCalloc(1, sizeof(EdgeBrowseResult));               //browseResult;
    if (IS_NULL(resultMsg->browseResult))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        goto ERROR;
    }
    resultMsg->browseResult->browseName = cloneString(browseResult->browseName);
    if(IS_NULL(resultMsg->browseResult->browseName))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        goto ERROR;
    }

    resultMsg->browseResultLength = size;

    add_to_recvQ(resultMsg);
    return;

ERROR:
    // Deallocate memory.
    freeEdgeMessage(resultMsg);
}

static void invokeResponseCbForContinuationPoint(EdgeMessage *msg, int msgId,
        EdgeNodeId *srcNodeId, UA_ByteString *continuationPoint, unsigned char *browsePrefix)
{
    if (!continuationPoint || continuationPoint->length < 1)
    {
        return;
    }

    if(browsePrefix)
    {
        ++browsePrefix; // To skip the leading '/'
    }

    EdgeMessage *resultMsg = (EdgeMessage *) EdgeCalloc(1, sizeof(EdgeMessage));
    VERIFY_NON_NULL_NR_MSG(resultMsg, "EdgeCalloc failed for EdgeMessage in invokeResponseCbForCP\n");

    resultMsg->type = BROWSE_RESPONSE;

    resultMsg->cpList = getContinuationPointList(continuationPoint, browsePrefix);
    if (!resultMsg->cpList)
    {
        EDGE_LOG(TAG, "Failed to form the continuation point.");
        EdgeFree(resultMsg);
        return;
    }

    resultMsg->endpointInfo = cloneEdgeEndpointInfo(msg->endpointInfo);
    if (!resultMsg->endpointInfo)
    {
        EDGE_LOG(TAG, "Failed to clone the EdgeEndpointInfo.");
        freeEdgeMessage(resultMsg);
        return;
    }

    EdgeResponse *response = (EdgeResponse *) EdgeCalloc(1, sizeof(EdgeResponse));
    if (IS_NULL(response))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        freeEdgeMessage(resultMsg);
        return;
    }
    response->nodeInfo = (EdgeNodeInfo *) EdgeCalloc(1, sizeof(EdgeNodeInfo));
    if (IS_NULL(response->nodeInfo))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        freeEdgeResponse(response);
        freeEdgeMessage(resultMsg);
        return;
    }
    response->nodeInfo->nodeId = cloneEdgeNodeId(srcNodeId);
    response->requestId = msgId; // Response for msgId'th request.
    EdgeResponse **responses = (EdgeResponse **) calloc(1, sizeof(EdgeResponse *));
    if (IS_NULL(responses))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        response->nodeInfo->nodeId = NULL;
        freeEdgeResponse(response);
        freeEdgeMessage(resultMsg);
        return;
    }
    responses[0] = response;
    resultMsg->responses = responses;
    resultMsg->responseLength = 1;
    resultMsg->message_id = msg->message_id;

    add_to_recvQ(resultMsg);
}

EdgeNodeId *getEdgeNodeId(UA_NodeId *node)
{
    VERIFY_NON_NULL_MSG(node, "UA NODE Parameter is NULL\n", NULL);

    EdgeNodeId *edgeNodeId = (EdgeNodeId *) EdgeCalloc(1, sizeof(EdgeNodeId));
    VERIFY_NON_NULL_MSG(edgeNodeId, "EdgeCalloc FAILED for edge node ID in getEdgeNodeId\n", NULL);

    edgeNodeId->nameSpace = node->namespaceIndex;
    switch (node->identifierType)
    {
        case UA_NODEIDTYPE_NUMERIC:
            edgeNodeId->type = INTEGER;
            edgeNodeId->integerNodeId = node->identifier.numeric;
            break;
        case UA_NODEIDTYPE_STRING:
            edgeNodeId->type = STRING;
            edgeNodeId->nodeId = convertUAStringToString(&node->identifier.string);
            break;
        case UA_NODEIDTYPE_BYTESTRING:
            edgeNodeId->type = BYTESTRING;
            edgeNodeId->nodeId = convertUAStringToString(&node->identifier.string);
            break;
        case UA_NODEIDTYPE_GUID:
            edgeNodeId->type = UUID;
            UA_Guid guid = node->identifier.guid;
            char *value = (char *) EdgeMalloc(GUID_LENGTH + 1);
            if (IS_NULL(value))
            {
                EDGE_LOG(TAG, "Memory allocation failed.");
                EdgeFree(edgeNodeId);
                edgeNodeId = NULL;
                break;
            }

            snprintf(value, GUID_LENGTH + 1, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    guid.data1, guid.data2, guid.data3, guid.data4[0], guid.data4[1], guid.data4[2],
                    guid.data4[3], guid.data4[4], guid.data4[5], guid.data4[6], guid.data4[7]);
            edgeNodeId->nodeId = value;
            break;
        default:
            // All valid cases are handled above.
            break;
    }

    return edgeNodeId;
}

void destroyBrowsePathNodeList(browsePathNode **browsePathListHead,
        browsePathNode **browsePathListTail)
{
    browsePathNode *ptr = *browsePathListHead;
    while(ptr != NULL)
    {
        browsePathNode *nextNode = ptr->next;
        EdgeFree(ptr);
        ptr = nextNode;
    }
    *browsePathListHead = *browsePathListTail = NULL;
}

static browsePathNode* pushBrowsePathNode(browsePathNode **browsePathListHead,
        browsePathNode **browsePathListTail, EdgeNodeId *edgeNodeId, unsigned char *browseName)
{
    browsePathNode *newNode = (browsePathNode*)EdgeCalloc(1, sizeof(browsePathNode));
    VERIFY_NON_NULL_MSG(newNode, "EdgeMalloc FAILED for browsePathNode\n", NULL);
    newNode->browseName = browseName;
    newNode->edgeNodeId = edgeNodeId;
    if(IS_NULL(*browsePathListTail))
    {
        *browsePathListHead = *browsePathListTail = newNode;
    }
    else
    {
        (*browsePathListTail)->next = newNode;
        newNode->pre = *browsePathListTail;
        *browsePathListTail = newNode;
    }
    return newNode;
}

static void popBrowsePathNode(browsePathNode **browsePathListHead, browsePathNode **browsePathListTail)
{
    if(*browsePathListHead == NULL || *browsePathListTail == NULL)
    {
        EDGE_LOG(TAG, "Browse Path Node Pop Error. List head/tail pointer is NULL.");
        return;
    }
    browsePathNode *deleteNode = *browsePathListTail;
    *browsePathListTail = deleteNode->pre;
    if(*browsePathListTail)
    {
        (*browsePathListTail)->next = NULL;
    }
    else
    {
        *browsePathListHead = NULL;
    }
    EdgeFree(deleteNode);
}

static unsigned char *getCurrentBrowsePath(browsePathNode *browsePathListHead)
{
    VERIFY_NON_NULL_MSG(browsePathListHead, "browsePathListHead is NULL\n", NULL);

    const size_t blockSize = 100;
    size_t curSize = blockSize;
    int lastUsed = -1;
    unsigned char *browsePath = (unsigned char *)EdgeMalloc(curSize * sizeof(unsigned char));
    VERIFY_NON_NULL_MSG(browsePath, "EdgeMalloc failed for browsePath in current\n", NULL);

    for(browsePathNode *ptr = browsePathListHead; ptr != NULL ; ptr = ptr->next)
    {
        /*EdgeNodeTypeCommon type = ptr->edgeNodeId->type;
        if(type == INTEGER){
        printf("/%d",ptr->edgeNodeId->integerNodeId);
        }else if( type == STRING){
        printf("/%s",ptr->edgeNodeId->nodeId);
        }*/
        if(IS_NULL(ptr->browseName))
        {
            continue;
        }
        size_t strLen = strlen((char *)ptr->browseName);
        if(lastUsed+strLen+2 >= curSize)
        {
            curSize += blockSize;
            unsigned char *newLoc = (unsigned char *)EdgeRealloc(browsePath, curSize);
            if(IS_NULL(newLoc))
            {
                EDGE_LOG(TAG, "EdgeRealloc Memory allocation failed.");
                EdgeFree(browsePath);
                return NULL;
            }
            browsePath = newLoc;
        }
        browsePath[++lastUsed] = '/';
        memcpy(browsePath+(++lastUsed), ptr->browseName, strLen);
        lastUsed += strLen-1;
    }
    if(lastUsed < 0)
    {
        EdgeFree(browsePath);
        return NULL;
    }
    browsePath[lastUsed+1] = '\0';
    return browsePath;
}

static bool hasNode(UA_String *browseName, browsePathNode *browsePathListHead)
{
    unsigned char *browseNameCharStr = convertUAStringToUnsignedChar(browseName);
    if(IS_NULL(browseNameCharStr))
    {
        return false;
    }

    bool found = false;
    for(browsePathNode *ptr = browsePathListHead; ptr != NULL ; ptr = ptr->next)
    {
        if(IS_NOT_NULL(ptr->browseName) &&
            !memcmp(ptr->browseName, browseNameCharStr, strlen((char *)ptr->browseName)+1))
        {
            found = true;
            break;
        }
    }

    FREE(browseNameCharStr);
    return found;
}

static char *getValueAlias(char *browseName, UA_NodeId* nodeId, UA_LocalizedText description)
{
    char *nodeInfo = NULL;
    const int bufferSize = 20;
    int browseNameLen = 0;
    nodeInfo = (char *)EdgeCalloc(bufferSize, sizeof(char));
    VERIFY_NON_NULL_MSG(nodeInfo, "EdgeCalloc FAILED for Node info\n", NULL);

    if(IS_NOT_NULL(browseName))
    {
        browseNameLen = strlen(browseName);
    }

    char curType = getCharacterNodeIdType(nodeId->identifierType);
    if (UA_NODEIDTYPE_STRING == nodeId->identifierType)
    {
        unsigned char *valueType = convertUAStringToUnsignedChar(&description.text);
        if(IS_NOT_NULL(valueType))
        {
            if (0 == strncmp((const char*)valueType, "v=", 2))
            {
                snprintf(nodeInfo, bufferSize*sizeof(char), "{%d;%c;%s}", nodeId->namespaceIndex, curType, valueType);
            }
            else
            {
                snprintf(nodeInfo, bufferSize*sizeof(char), "{%d;%c;v=0}", nodeId->namespaceIndex, curType);
            }
            EdgeFree(valueType);
        }
    }
    else
    {
        snprintf(nodeInfo, bufferSize*sizeof(char), "{%d;%c}", nodeId->namespaceIndex, curType);
    }

    int nodeInfoLen = strlen(nodeInfo);
    char *valueAlias = (char *)EdgeCalloc(nodeInfoLen+browseNameLen + 1, sizeof(char));
    if(IS_NULL(valueAlias))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        EdgeFree(nodeInfo);
        return NULL;
    }
    if(nodeInfoLen > 0)
    {
        memcpy(valueAlias, nodeInfo, nodeInfoLen);
        if(browseNameLen > 0)
        {
            memcpy(valueAlias + nodeInfoLen, browseName, browseNameLen);
        }
    }
    valueAlias[nodeInfoLen + browseNameLen] = '\0';

    EdgeFree(nodeInfo);
    return valueAlias;
}

static unsigned char *getCompleteBrowsePath(char *valueAlias,
        browsePathNode *browsePathListHead)
{
    int valueAliasLen = 0;
    if(IS_NOT_NULL(valueAlias))
    {
        valueAliasLen = strlen(valueAlias);
    }

    unsigned char *browsePath = getCurrentBrowsePath(browsePathListHead);
    int pathLen = IS_NOT_NULL(browsePath) ? strlen((char *)browsePath) : 0;
    unsigned char *completePath = (unsigned char *)EdgeCalloc(pathLen+valueAliasLen+2, sizeof(unsigned char));
    if(IS_NULL(completePath))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        EdgeFree(browsePath);
        return NULL;
    }

    if(pathLen > 0)
    {
        memcpy(completePath, browsePath, pathLen);
    }

    completePath[pathLen++] = '/';
    if(valueAliasLen > 0)
    {
        memcpy(completePath + pathLen, valueAlias, valueAliasLen);
        pathLen += valueAliasLen;
    }

    completePath[pathLen] = '\0';
    EdgeFree(browsePath);
    return completePath;
}

unsigned char *convertNodeIdToString(UA_NodeId *nodeId)
{
    VERIFY_NON_NULL_MSG(nodeId, "UA NODE IF parameter is NULL\n", NULL);

    unsigned char *browseName = NULL;
    if(UA_NODEIDTYPE_STRING == nodeId->identifierType)
    {
        browseName = convertUAStringToUnsignedChar(&nodeId->identifier.string);
    }
    /*else if(UA_NODEIDTYPE_NUMERIC == nodeId->identifierType)
    {
        int maxDigits = 10;
        browseName = (unsigned char *)EdgeCalloc(maxDigits+1, sizeof(unsigned char));
         VERIFY_NON_NULL_MSG(browseName, NULL);
        snprintf((char *)browseName, maxDigits, "%" PRIu32, nodeId->identifier.numeric);
    } */
    // TODO: Handle GUID and ByteString
    return browseName;
}

static ViewNodeInfo_t *getNodeInfo(UA_NodeId *nodeId, UA_String *browseName)
{
    VERIFY_NON_NULL_MSG(nodeId, "NodeID parameter is NULL\n", NULL);
    VERIFY_NON_NULL_MSG(browseName, "BrowseName  parameter is NULL\n", NULL);

    ViewNodeInfo_t *nodeInfo = (ViewNodeInfo_t *)EdgeCalloc(1, sizeof(ViewNodeInfo_t));
    VERIFY_NON_NULL_MSG(nodeInfo, "EdgeCalloc FAILED for ViewNodeInfo_t\n", NULL);

    if(browseName->length > 0)
    {
        nodeInfo->browseName = convertUAStringToUnsignedChar(browseName);
        if(IS_NULL(nodeInfo->browseName))
        {
            EDGE_LOG(TAG, "Failed to convert UA_String to unsigned char string.");
            EdgeFree(nodeInfo);
            return NULL;
        }
    }

    nodeInfo->nodeId = (UA_NodeId *)EdgeCalloc(1, sizeof(UA_NodeId));
    if(IS_NULL(nodeInfo->nodeId))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        EdgeFree(nodeInfo->browseName);
        EdgeFree(nodeInfo);
        return NULL;
    }

    if(UA_STATUSCODE_GOOD != UA_NodeId_copy(nodeId, nodeInfo->nodeId))
    {
        EDGE_LOG(TAG, "Failed to copy the node id.");
        EdgeFree(nodeInfo->nodeId);
        EdgeFree(nodeInfo->browseName);
        EdgeFree(nodeInfo);
        return NULL;
    }

    return nodeInfo;
}

void destroyViewListMembers(List *ptr)
{
    VERIFY_NON_NULL_NR_MSG(ptr, "NULL list parameter\n");

    while(ptr)
    {
        ViewNodeInfo_t *nodeInfo = ptr->data;
        EdgeFree(nodeInfo->browseName);
        UA_NodeId_delete(nodeInfo->nodeId);
        EdgeFree(nodeInfo);
        ptr=ptr->link;
    }
}

EdgeStatusCode browse(UA_Client *client, EdgeMessage *msg, bool browseNext,
    NodesToBrowse_t *browseNodesInfo, int *reqIdList, List **viewList,
    browsePathNode **browsePathListHead, browsePathNode **browsePathListTail)
{
    UA_BrowseResponse *resp = NULL;
    UA_BrowseResponse browseResp =
    {
    { 0 } };
    UA_BrowseNextResponse browseNextResp =
    {
    { 0 } };
    UA_BrowseDescription *nodesToBrowse = NULL;
    if (browseNext)
    {
        UA_BrowseNextRequest bReq;
        UA_BrowseNextRequest_init(&bReq);
        bReq.releaseContinuationPoints = false;
        bReq.continuationPointsSize = msg->cpList->count;
        bReq.continuationPoints = (UA_ByteString *) EdgeMalloc(
                bReq.continuationPointsSize * sizeof(UA_ByteString));
        VERIFY_NON_NULL_MSG(bReq.continuationPoints, "EdgeMalloc FAILED for bReq\n", STATUS_INTERNAL_ERROR);

        for (int i = 0; i < bReq.continuationPointsSize; ++i)
        {
            UA_ByteString *byteStr;
            bReq.continuationPoints[i] =
                    (byteStr = getUAStringFromEdgeContinuationPoint(msg->cpList->cp[i])) ?
                            *byteStr : UA_BYTESTRING_NULL;
            EdgeFree(byteStr);
        }
        browseNextResp = UA_Client_Service_browseNext(client, bReq);
        resp = (UA_BrowseResponse *) &browseNextResp;
        UA_BrowseNextRequest_deleteMembers(&bReq);
        EdgeFree(bReq.continuationPoints);
    }
    else
    {
        nodesToBrowse = getBrowseDescriptions(browseNodesInfo, msg,
            IS_NULL(viewList) ? BROWSE_NODECLASS_MASK : VIEW_NODECLASS_MASK);
        VERIFY_NON_NULL_MSG(nodesToBrowse, "NULL nodes to browse returned\n", STATUS_ERROR);

        UA_BrowseRequest bReq;
        UA_BrowseRequest_init(&bReq);
        bReq.requestedMaxReferencesPerNode = msg->browseParam->maxReferencesPerNode;
        bReq.nodesToBrowse = nodesToBrowse;
        bReq.nodesToBrowseSize = browseNodesInfo->size;
        browseResp = UA_Client_Service_browse(client, bReq);
        resp = &browseResp;
    }

    if (resp->responseHeader.serviceResult != UA_STATUSCODE_GOOD || resp->resultsSize <= 0)
    {
        char *versatileVal;
        EdgeStatusCode statusCode;
        // Invoke error callback
        if (resp->resultsSize <= 0)
        {
            statusCode = STATUS_VIEW_BROWSERESULT_EMPTY;
            versatileVal = STATUS_VIEW_BROWSERESULT_EMPTY_VALUE;
            EDGE_LOG(TAG, "Error: Empty browse response!!!");
        }
        else
        {
            statusCode = STATUS_SERVICE_RESULT_BAD;
            versatileVal = STATUS_SERVICE_RESULT_BAD_VALUE;
            UA_StatusCode serviceResult = resp->responseHeader.serviceResult;
            (void) serviceResult;
            EDGE_LOG_V(TAG, "Error in browse :: 0x%08x(%s)\n", serviceResult, UA_StatusCode_name(serviceResult));
        }

        EdgeNodeInfo *nodeInfo = getEndpoint(msg, 0);
        invokeErrorCb(msg->message_id, (nodeInfo ? nodeInfo->nodeId : NULL), statusCode, versatileVal);
        EdgeFree(nodesToBrowse);
        UA_BrowseResponse_deleteMembers(resp);
        return statusCode;
    }

    int *nextReqIdList = NULL;
    NodesToBrowse_t *nextBrowseNodesInfo = NULL;
    EdgeStatusCode statusCode;
    int nodeIdUnknownCount = 0;
    EdgeNodeId *srcNodeId = NULL;
    unsigned char *srcBrowseName = NULL;
    for (size_t i = 0; i < resp->resultsSize; ++i)
    {
        freeEdgeNodeId(srcNodeId);
        srcNodeId = getEdgeNodeId(&browseNodesInfo->nodeId[i]);
        srcBrowseName = browseNodesInfo->browseName[i];
        if(IS_NULL(pushBrowsePathNode(browsePathListHead, browsePathListTail, srcNodeId, srcBrowseName)))
        {
            EDGE_LOG(TAG, "Push Node of Browse Path Error.");
            statusCode = STATUS_INTERNAL_ERROR;
            goto EXIT;
        }

        UA_StatusCode status = resp->results[i].statusCode;
        int reqId = reqIdList[i];
        EdgeBrowseDirection direction = msg->browseParam->direction;
        if (checkStatusGood(status) == false)
        {
            if (UA_STATUSCODE_BADNODEIDUNKNOWN == status)
                nodeIdUnknownCount++;

            if (nodeIdUnknownCount == resp->resultsSize)
            {
                EDGE_LOG(TAG, "Error: " STATUS_VIEW_NODEID_UNKNOWN_ALL_RESULTS_VALUE);
                invokeErrorCb(msg->message_id, srcNodeId, STATUS_VIEW_NODEID_UNKNOWN_ALL_RESULTS,
                STATUS_VIEW_NODEID_UNKNOWN_ALL_RESULTS_VALUE);
            }
            else
            {
                const char *statusStr = UA_StatusCode_name(status);
                invokeErrorCb(msg->message_id, srcNodeId, STATUS_VIEW_RESULT_STATUS_CODE_BAD, statusStr);
            }
            continue;
        }

        if (!checkContinuationPoint(msg->message_id, resp->results[i], srcNodeId))
        {
            continue;
        }

        // If it is a browseNext call,
        // then references should not be empty if statuscode is good.
        if (browseNext && !resp->results[i].referencesSize)
        {
            EDGE_LOG(TAG, "Error: " STATUS_VIEW_REFERENCE_DATA_INVALID_VALUE);
            invokeErrorCb(msg->message_id, srcNodeId, STATUS_ERROR, STATUS_VIEW_REFERENCE_DATA_INVALID_VALUE);
            continue;
        }

        nextReqIdList = (int *) calloc(resp->results[i].referencesSize, sizeof(int));
        if (IS_NULL(nextReqIdList))
        {
            EDGE_LOG(TAG, "Memory allocation failed.");
            statusCode = STATUS_INTERNAL_ERROR;
            goto EXIT;
        }

        nextBrowseNodesInfo = initNodesToBrowse(resp->results[i].referencesSize);
        if (IS_NULL(nextBrowseNodesInfo))
        {
            EDGE_LOG(TAG, "Memory allocation failed.");
            statusCode = STATUS_INTERNAL_ERROR;
            goto EXIT;
        }

        int nextReqListCount = 0;
        size_t nextNodeListCount = 0;
        for (size_t j = 0; j < resp->results[i].referencesSize; ++j)
        {
            bool isError = false;
            UA_ReferenceDescription *ref = &(resp->results[i].references[j]);
            if ((direction == DIRECTION_FORWARD && ref->isForward == false)
                    || (direction == DIRECTION_INVERSE && ref->isForward == true))
            {
                EDGE_LOG(TAG, "Error: " STATUS_VIEW_DIRECTION_NOT_MATCH_VALUE);
                invokeErrorCb(msg->message_id, srcNodeId, STATUS_VIEW_DIRECTION_NOT_MATCH,
                STATUS_VIEW_DIRECTION_NOT_MATCH_VALUE);
                isError = true;
            }

            if (!checkBrowseName(msg->message_id, ref->browseName.name, srcNodeId))
                isError = true;
            if (!checkNodeClass(msg->message_id, ref->nodeClass, srcNodeId))
                isError = true;
            if (!checkDisplayName(msg->message_id, ref->displayName.text, srcNodeId))
                isError = true;
            if (!checkNodeId(msg->message_id, ref->nodeId, srcNodeId))
                isError = true;
            if (!checkReferenceTypeId(msg->message_id, ref->referenceTypeId, srcNodeId))
                isError = true;
            if (!checkTypeDefinition(msg->message_id, ref, srcNodeId))
                isError = true;

            if (!isError)
            {
#if DEBUG
                logNodeId(ref->nodeId.nodeId);
#endif
                if (!hasNode(&ref->browseName.name, *browsePathListHead))
                {
                    if(IS_NULL(viewList))
                    {
                        size_t size = 1;
                        EdgeBrowseResult *browseResult = (EdgeBrowseResult *) EdgeCalloc(size,
                                sizeof(EdgeBrowseResult));
                        if (IS_NULL(browseResult))
                        {
                            EDGE_LOG(TAG, "Memory allocation failed.");
                            statusCode = STATUS_INTERNAL_ERROR;
                            goto EXIT;
                        }

                        if (UA_NODEIDTYPE_STRING == ref->nodeId.nodeId.identifierType)
                        {
                            browseResult->browseName = convertUAStringToString(&ref->nodeId.nodeId.identifier.string);
                        }
                        else
                        {
                            browseResult->browseName = convertUAStringToString(&ref->browseName.name);
                        }

                        if (!browseResult->browseName)
                        {
                            EDGE_LOG(TAG, "Memory allocation failed.");
                            statusCode = STATUS_INTERNAL_ERROR;
                            EdgeFree(browseResult);
                            goto EXIT;
                        }

                        // EdgeVersatility in EdgeResponse will have the complete path to browse name (Including the browse name).
                        unsigned char *completePath = NULL;
                        char *valueAlias = NULL;
                        if((!SHOW_SPECIFIC_NODECLASS) || (ref->nodeClass & SHOW_SPECIFIC_NODECLASS_MASK)){
                            valueAlias = getValueAlias(browseResult->browseName, &(ref->nodeId.nodeId), ref->displayName);
                            completePath = getCompleteBrowsePath(valueAlias, *browsePathListHead);
                        }

                        invokeResponseCb(msg, reqId, srcNodeId, browseResult, size, completePath, valueAlias);
                        EdgeFree(completePath);
                        EdgeFree(valueAlias);
                        EdgeFree(browseResult->browseName);
                        EdgeFree(browseResult);
                    }
                    else if(UA_NODECLASS_VIEW == ref->nodeClass)
                    {
                        // This browse() is for views. If the current reference is a view node, then it will be added in the viewList.
                        // Application callback will not be invoked.
                        ViewNodeInfo_t *info = getNodeInfo(&ref->nodeId.nodeId, &ref->browseName.name);
                        if(IS_NULL(info))
                        {
                            EDGE_LOG(TAG, "Failed to copy node info from ReferenceDescription.");
                            statusCode = STATUS_INTERNAL_ERROR;
                            goto EXIT;
                        }
                        if(!addListNode(viewList, info))
                        {
                            EDGE_LOG(TAG, "Adding view node to list failed.");
                            statusCode = STATUS_INTERNAL_ERROR;
                            goto EXIT;
                        }
                    }

                    if(UA_NODECLASS_VARIABLE != ref->nodeClass)
                    {
                        nextBrowseNodesInfo->nodeId[nextNodeListCount] = ref->nodeId.nodeId;
                        nextBrowseNodesInfo->browseName[nextNodeListCount] = convertUAStringToUnsignedChar(&ref->browseName.name);
                        nextReqIdList[nextReqListCount] = reqId;
                        nextNodeListCount++;
                        nextReqListCount++;
                    }
                }
                else
                {
                    EDGE_LOG(TAG, "Already visited this node in the current browse path.");
                }
            }
        }

        nextBrowseNodesInfo->size = nextNodeListCount;

        // Pass the continuation point for this result to the application.
        if (resp->results[i].continuationPoint.length > 0)
        {
            EDGE_LOG(TAG, "Passing continuation point to application.");
            unsigned char *browsePrefix = getCurrentBrowsePath(*browsePathListHead);
            invokeResponseCbForContinuationPoint(msg, reqId, srcNodeId,
                    &resp->results[i].continuationPoint, browsePrefix);
            EdgeFree(browsePrefix);
        }

        if (nextNodeListCount > 0)
        {
            browse(client, msg, false, nextBrowseNodesInfo, nextReqIdList, viewList,
                browsePathListHead, browsePathListTail);
        }
        popBrowsePathNode(browsePathListHead, browsePathListTail);
        freeEdgeNodeId(srcNodeId);
        srcNodeId = NULL;
        FREE(nextReqIdList);
        destroyNodesToBrowse(nextBrowseNodesInfo, false);
        nextBrowseNodesInfo = NULL;
    }

    statusCode = STATUS_OK;

    EXIT:
    // Deallocate memory.
    EdgeFree(nextReqIdList);
    destroyNodesToBrowse(nextBrowseNodesInfo, false);
    EdgeFree(nodesToBrowse);
    freeEdgeNodeId(srcNodeId);
    UA_BrowseResponse_deleteMembers(resp);
    return statusCode;
}

void browseNodes(UA_Client *client, EdgeMessage *msg)
{
    size_t nodesToBrowseSize = (msg->requestLength) ? msg->requestLength : 1;
    int *reqIdList = (int *) EdgeCalloc(nodesToBrowseSize, sizeof(int));
    if (IS_NULL(reqIdList))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        invokeErrorCb(msg->message_id, NULL, STATUS_INTERNAL_ERROR, "Memory allocation failed.");
        return;
    }

    NodesToBrowse_t *browseNodesInfo = initNodesToBrowse(nodesToBrowseSize);
    if (IS_NULL(browseNodesInfo))
    {
        EDGE_LOG(TAG, "Memory allocation failed.");
        invokeErrorCb(msg->message_id, NULL, STATUS_INTERNAL_ERROR, "Memory allocation failed.");
        return;
    }

    if (msg->type == SEND_REQUEST)
    {
        EDGE_LOG(TAG, "Message Type: " SEND_REQUEST_DESC);
        UA_NodeId *nodeId;
        browseNodesInfo->nodeId[0] = (nodeId = getNodeId(msg->request)) ? *nodeId : UA_NODEID_NULL;
        browseNodesInfo->browseName[0] = convertNodeIdToString(nodeId);
        reqIdList[0] = 0;
        EdgeFree(nodeId);
    }
    else
    {
        EDGE_LOG(TAG, "Message Type: " SEND_REQUESTS_DESC);
        if (nodesToBrowseSize > MAX_BROWSEREQUEST_SIZE)
        {
            EdgeNodeInfo *nodeInfo = getEndpoint(msg, 0);
            EDGE_LOG(TAG, "Error: " STATUS_VIEW_BROWSEREQUEST_SIZEOVER_VALUE);
            invokeErrorCb(msg->message_id, (nodeInfo ? nodeInfo->nodeId : NULL), STATUS_ERROR,
                    STATUS_VIEW_BROWSEREQUEST_SIZEOVER_VALUE);
            destroyNodesToBrowse(browseNodesInfo, true);
            EdgeFree(reqIdList);
            return;
        }

        for (size_t i = 0; i < nodesToBrowseSize; ++i)
        {
            UA_NodeId *nodeId;
            browseNodesInfo->nodeId[i] = (nodeId = getNodeIdMultiReq(msg, i)) ? *nodeId : UA_NODEID_NULL;
            browseNodesInfo->browseName[i] = convertNodeIdToString(nodeId);
            EdgeFree(nodeId);
            reqIdList[i] = i;
        }
    }

    browsePathNode *browsePathListHead = NULL, *browsePathListTail = NULL;
    EdgeStatusCode statusCode = browse(client, msg, false, browseNodesInfo, reqIdList, NULL,
        &browsePathListHead, &browsePathListTail);
    if (statusCode != STATUS_OK)
    {
        EDGE_LOG(TAG, "Browse failed.");
        invokeErrorCb(msg->message_id, NULL, STATUS_ERROR, "Browse failed.");
    }

    destroyNodesToBrowse(browseNodesInfo, true);
    destroyBrowsePathNodeList(&browsePathListHead, &browsePathListTail);
    EdgeFree(reqIdList);
}
