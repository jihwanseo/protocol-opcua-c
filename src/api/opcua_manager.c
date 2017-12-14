#include "opcua_manager.h"
#include "edge_opcua_server.h"
#include "edge_opcua_client.h"

#include <stdio.h>
#include <stdlib.h>

static ReceivedMessageCallback* receivedMsgCb;
static StatusCallback* statusCb;
static DiscoveryCallback *discoveryCb;

static bool b_serverInitialized = false;
static bool b_clientInitialized = false;

static void registerRecvCallback(ReceivedMessageCallback* callback) {
  receivedMsgCb = callback;
}

static void registerStatusCallback(StatusCallback* callback) {
  statusCb = callback;
}

static void registerDiscoveryCallback(DiscoveryCallback* callback) {
  discoveryCb = callback;
}

void registerCallbacks(EdgeConfigure *config) {
  registerRecvCallback(config->recvCallback);
  registerStatusCallback(config->statusCallback);
  registerDiscoveryCallback(config->discoveryCallback);
}


EdgeResult* createNamespace(char* name, char* rootNodeId,
                                                                   char* rootBrowseName, char* rootDisplayName) {
  createNamespaceInServer(name, rootNodeId, rootBrowseName, rootDisplayName);
  EdgeResult* result = (EdgeResult*) malloc(sizeof(EdgeResult));
  result->code = STATUS_OK;
  return result;
}

EdgeResult* createNode(char* namespaceUri, EdgeNodeItem* item) {
  // add Nodes in server
  EdgeResult* result = addNodesInServer(item);
  return result;
}

EdgeResult* addReference(EdgeReference *reference) {
  EdgeResult* result = addReferenceInServer(reference);
  return result;
}

EdgeResult* readNode(EdgeMessage *msg) {
  EdgeResult* result = readNodesFromServer(msg);
  return result;
}

EdgeResult* writeNode(EdgeMessage *msg) {
  EdgeResult* result = writeNodesInServer(msg);
  return result;
}

EdgeResult* browseNode(EdgeMessage *msg) {
  EdgeResult* result = browseNodesInServer(msg);
  return result;
}

void createServer(EdgeEndPointInfo *epInfo) {
  printf ("\n[Received command] :: Server start \n");
  if (b_serverInitialized) {
    printf( "Server already initialised");
    return ;
  }
  EdgeResult* result = start_server(epInfo);
  if (result == NULL)
    return ;
  if (result->code == STATUS_OK) {
    b_serverInitialized = true;
  }
  free (result); result = NULL;
}

void closeServer(EdgeEndPointInfo *epInfo) {
  if (b_serverInitialized) {
    stop_server(epInfo);
    b_serverInitialized = false;
  }
}

void getEndpointInfo(EdgeEndPointInfo* epInfo) {
  printf("\n[Received command] :: Get endpoint info for [%s] \n\n", epInfo->endpointUri);
  getClientEndpoints(epInfo->endpointUri);
}

void connectClient(EdgeEndPointInfo *epInfo) {
  printf ("\n[Received command] :: Client connect \n");
  if (b_clientInitialized) {
    printf( "Client already initialised");
    return ;
  }
  bool result = connect_client(epInfo->endpointUri);
  if (!result)
    return ;
}

void disconnectClient(EdgeEndPointInfo *epInfo) {
  printf("\n[Received command] :: Client disconnect \n");
  disconnect_client(epInfo);
  b_clientInitialized = false;
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
//  printf("============= onSendMessage============");
//  if (msg->command == CMD_START_SERVER) {
//    printf ("\n[Received command] :: Server start \n");
//    if (b_serverInitialized) {
//      printf( "Server already initialised");
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
//    printf ("\n[Received command] :: Client connect \n");
//    if (b_clientInitialized) {
//      printf( "Client already initialised");
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
//    printf("\n[Received command] :: Client disconnect \n");
//    disconnect_client();
//    b_clientInitialized = false;
//  }
//}

void onResponseMessage(EdgeMessage *msg) {
  if (NULL == receivedMsgCb) {
    printf("receiver callback not registered\n\n");
    return ;
  }
  if (receivedMsgCb && msg->type == GENERAL_RESPONSE) {
    receivedMsgCb->resp_msg_cb(msg);
  }
  if (receivedMsgCb && msg->type == BROWSE_RESPONSE) {
    receivedMsgCb->browse_msg_cb(msg);
  }

}

void onDiscoveryCallback(EdgeDevice *device) {
  if (NULL == discoveryCb) {
    // discovery callback not registered by application.
    return ;
  }
  discoveryCb->endpoint_found_cb(device);
}

void onStatusCallback(EdgeEndPointInfo* epInfo, EdgeStatusCode status) {
  if (NULL == statusCb) {
    // status callback not registered by application.
    return ;
  }
  if (STATUS_SERVER_STARTED == status || STATUS_CLIENT_STARTED == status) {
    statusCb->start_cb(epInfo, status);
  } else if (STATUS_STOP_SERVER == status || STATUS_STOP_CLIENT== status) {
    statusCb->stop_cb(epInfo, status);
  } else if (STATUS_CONNECTED == status || STATUS_DISCONNECTED == status) {
    statusCb->network_cb(epInfo, status);
  }
}
