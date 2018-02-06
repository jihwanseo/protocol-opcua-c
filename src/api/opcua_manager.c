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

#include "opcua_manager.h"
#include "edge_opcua_server.h"
#include "edge_opcua_client.h"
#include "edge_logger.h"
#include "edge_utils.h"

#include <stdio.h>
#include <stdlib.h>

#define TAG "opcua_manager"

static ReceivedMessageCallback *receivedMsgCb;
static StatusCallback *statusCb;
static DiscoveryCallback *discoveryCb;

static bool b_serverInitialized = false;

void showNodeList(void)
{
    printNodeListInServer();
}

static void registerRecvCallback(ReceivedMessageCallback *callback)
{
    receivedMsgCb = callback;
}

static void registerStatusCallback(StatusCallback *callback)
{
    statusCb = callback;
}

static void registerDiscoveryCallback(DiscoveryCallback *callback)
{
    discoveryCb = callback;
}

void configure(EdgeConfigure *config)
{
    registerRecvCallback(config->recvCallback);
    registerStatusCallback(config->statusCallback);
    registerDiscoveryCallback(config->discoveryCallback);

    setSupportedApplicationTypes(config->supportedApplicationTypes);
}

EdgeResult createNamespace(char *name, char *rootNodeId, char *rootBrowseName,
        char *rootDisplayName)
{
    createNamespaceInServer(name, rootNodeId, rootBrowseName, rootDisplayName);
    EdgeResult result;
    result.code = STATUS_OK;
    return result;
}

EdgeResult createNode(char *namespaceUri, EdgeNodeItem *item)
{
    // add Nodes in server
    EdgeResult result = addNodesInServer(namespaceUri, item);
    return result;
}

EdgeResult modifyVariableNode(char *namespaceUri, char *nodeUri, EdgeVersatility *value)
{
    // modify variable nodes
    EdgeResult result = modifyNodeInServer(namespaceUri, nodeUri, value);
    return result;
}

EdgeResult addReference(EdgeReference *reference)
{
    EdgeResult result = addReferenceInServer(reference);
    return result;
}

EdgeResult createMethodNode(char *namespaceUri, EdgeNodeItem *item, EdgeMethod *method)
{
    EdgeResult result = addMethodNodeInServer(namespaceUri, item, method);
    return result;
}

EdgeResult readNode(EdgeMessage *msg)
{
    EdgeResult result = readNodesFromServer(msg);
    return result;
}

EdgeResult writeNode(EdgeMessage *msg)
{
    EdgeResult result = writeNodesInServer(msg);
    return result;
}

EdgeResult browseNode(EdgeMessage *msg)
{
    EdgeResult result = browseNodesInServer(msg);
    return result;
}

EdgeResult browseViews(EdgeMessage *msg)
{
    EdgeResult result = browseViewsInServer(msg);
    return result;
}

EdgeResult browseNext(EdgeMessage *msg)
{
    EdgeResult result = browseNextInServer(msg);
    return result;
}

EdgeResult callMethod(EdgeMessage *msg)
{
    EdgeResult result = callMethodInServer(msg);
    return result;
}

EdgeResult handleSubscription(EdgeMessage *msg)
{
    EdgeResult result = executeSubscriptionInServer(msg);
    return result;
}

void createServer(EdgeEndPointInfo *epInfo)
{
    EDGE_LOG(TAG, "[Received command] :: Server start.");
    if (b_serverInitialized)
    {
        EDGE_LOG(TAG, "Server already initialized.");
        return;
    }

    EdgeResult result = start_server(epInfo);
    if (result.code == STATUS_OK)
    {
        b_serverInitialized = true;
    }
}

void closeServer(EdgeEndPointInfo *epInfo)
{
    if (b_serverInitialized)
    {
        stop_server(epInfo);
        b_serverInitialized = false;
    }
}

EdgeResult getEndpointInfo(EdgeEndPointInfo *epInfo)
{
    EDGE_LOG_V(TAG, "[Received command] :: Get endpoint info for [%s].\n", epInfo->endpointUri);
    return getClientEndpoints(epInfo->endpointUri);
}

EdgeResult findServers(const char *endpointUri, size_t serverUrisSize, unsigned char **serverUris,
        size_t localeIdsSize, unsigned char **localeIds, size_t *registeredServersSize,
        EdgeApplicationConfig **registeredServers)
{
    return findServersInternal(endpointUri, serverUrisSize, serverUris, localeIdsSize, localeIds,
            registeredServersSize, registeredServers);
}

void connectClient(EdgeEndPointInfo *epInfo)
{
    EDGE_LOG(TAG, "[Received command] :: Client connect.");
    bool result = connect_client(epInfo->endpointUri);
    if (!result)
        return;
}

void disconnectClient(EdgeEndPointInfo *epInfo)
{
    EDGE_LOG(TAG, "[Received command] :: Client disconnect.");
    disconnect_client(epInfo);
}

EdgeNodeItem* createVariableNodeItem(char* name, EdgeNodeIdentifier type, void* data,
        EdgeIdentifier nodeType)
{
    return createVariableNodeItemImpl(name, type, data, nodeType);
}

EdgeNodeItem* createNodeItem(char* name, EdgeIdentifier nodeType, EdgeNodeId *sourceNodeId)
{
    return createNodeItemImpl(name, nodeType, sourceNodeId);
}

EdgeResult deleteNodeItem(EdgeNodeItem* item)
{
    return deleteNodeItemImpl(item);
}

void destroyEdgeResult(EdgeResult *res)
{
    freeEdgeResult(res);
}

void destroyEdgeEndpointConfig(EdgeEndpointConfig *epConfig)
{
    freeEdgeEndpointConfig(epConfig);
}

void destroyEdgeApplicationConfigMembers(EdgeApplicationConfig *config)
{
    freeEdgeApplicationConfigMembers(config);
}

void destroyEdgeVersatility(EdgeVersatility *versatileValue)
{
    freeEdgeVersatility(versatileValue);
}

void destroyEdgeNodeId(EdgeNodeId *nodeId)
{
    freeEdgeNodeId(nodeId);
}

void destroyEdgeArgument(EdgeArgument *arg)
{
    freeEdgeArgument(arg);
}

void destroyEdgeMethodRequestParams(EdgeMethodRequestParams *reqParams)
{
    freeEdgeMethodRequestParams(reqParams);
}

void destroyEdgeNodeInfo(EdgeNodeInfo *nodeInfo)
{
    freeEdgeNodeInfo(nodeInfo);
}

void destroyEdgeContinuationPoint(EdgeContinuationPoint *cp)
{
    freeEdgeContinuationPoint(cp);
}

void destroyEdgeContinuationPointList(EdgeContinuationPointList *cpList)
{
    freeEdgeContinuationPointList(cpList);
}

void destroyEdgeEndpointInfo(EdgeEndPointInfo *endpointInfo)
{
    freeEdgeEndpointInfo(endpointInfo);
}

void destroyEdgeRequest(EdgeRequest *req)
{
    freeEdgeRequest(req);
}

void destroyEdgeResponse(EdgeResponse *resp)
{
    freeEdgeResponse(resp);
}

void destroyEdgeMessage(EdgeMessage *msg)
{
    freeEdgeMessage(msg);
}

char *copyString(const char *str)
{
    return cloneString(str);
}

//EdgeResult* send(EdgeMessage* msg) {
//  if (msg == NULL)
//    return NULL;
//  //bool ret = add_to_sendQ(msg);

//  EdgeResult* result = (EdgeResult*) malloc(sizeof(EdgeResult));
//  result->code = STATUS_OK;
////  result->code = (ret  ? STATUS_OK : STATUS_ENQUEUE_ERROR);
//  return result;
//}

//void onSendMessage(EdgeMessage* msg) {
//  EDGE_LOG(TAG, "============= onSendMessage============");
//  if (msg->command == CMD_START_SERVER) {
//    EDGE_LOG(TAG, "\n[Received command] :: Server start \n");
//    if (b_serverInitialized) {
//      EDGE_LOG(TAG, "Server already initialised");
//      return ;
//    }
//    EdgeResult* result = start_server(msg->endpointInfo);
//    if (result == NULL)
//      return ;
//    if (result->code == STATUS_OK) {
//      b_serverInitialized = true;
//    }
//    free (result); result = NULL;
//  } else if (msg->command == CMD_START_CLIENT) {
//    EDGE_LOG(TAG, "\n[Received command] :: Client connect \n");
//    if (b_clientInitialized) {
//      EDGE_LOG(TAG, "Client already initialised");
//      return ;
//    }
//    bool result = connect_client(msg->endpointInfo->endpointUri);
//    if (!result)
//      return ;
//   b_clientInitialized = true;
//  } else if (msg->command == CMD_STOP_SERVER) {
//    stop_server(msg->endpointInfo);
//    b_serverInitialized = false;
//  } else if (msg->command == CMD_STOP_CLIENT) {
//    EDGE_LOG(TAG, "\n[Received command] :: Client disconnect \n");
//    disconnect_client();
//    b_clientInitialized = false;
//  }
//}

void onResponseMessage(EdgeMessage *msg)
{
    if (NULL == receivedMsgCb || NULL == msg)
    {
        EDGE_LOG(TAG, "parameter is invalid.");
        return;
    }

    switch (msg->type)
    {
        case GENERAL_RESPONSE:
            receivedMsgCb->resp_msg_cb(msg);
            break;
        case BROWSE_RESPONSE:
            receivedMsgCb->browse_msg_cb(msg);
            break;
        case REPORT:
            receivedMsgCb->monitored_msg_cb(msg);
            break;
        case ERROR:
            receivedMsgCb->error_msg_cb(msg);
            break;
        default:
            break;
    }
}

void onDiscoveryCallback(EdgeDevice *device)
{
    if (NULL == discoveryCb)
    {
        // discovery callback not registered by application.
        return;
    }
    discoveryCb->endpoint_found_cb(device);
}

void onStatusCallback(EdgeEndPointInfo *epInfo, EdgeStatusCode status)
{
    if (NULL == statusCb)
    {
        // status callback not registered by application.
        return;
    }
    if (STATUS_SERVER_STARTED == status || STATUS_CLIENT_STARTED == status)
    {
        statusCb->start_cb(epInfo, status);
    }
    else if (STATUS_STOP_SERVER == status || STATUS_STOP_CLIENT == status)
    {
        statusCb->stop_cb(epInfo, status);
    }
    else if (STATUS_CONNECTED == status || STATUS_DISCONNECTED == status)
    {
        statusCb->network_cb(epInfo, status);
    }
}
