/***************************************************************************
 *
 * Copyright (c) 2014 Baidu.com, Inc. All Rights Reserved
 *
 **************************************************************************/
#ifndef OP_OPED_NOAH_TOOLS_BBTS_GROUP_CONF_RANGE_H_
#define OP_OPED_NOAH_TOOLS_BBTS_GROUP_CONF_RANGE_H_

// define conf range constant value

namespace bbts {
namespace group {

static const int kMinUploadLimit = 0;
static const int kMaxUploadLimit = 2000;

static const int kMinDownloadLimit = 0;
static const int kMaxDownloadLimit = 2000;

static const int kMinConnectionLimit = 1;
static const int kMaxConnectionLimit = 32768;

static const int kMinPeersNumWant = 10;
static const int kMaxPeersNumWant = 100;

static const int kMinListenPort = 10000;
static const int kMaxListenPort = 65534;

static const int kMinActiveSeeds = 1;
static const int kMaxActiveSeeds = 32768;

static const int kMinActiveDownloads = 1;
static const int kMaxActiveDownloads = 32768;

static const int kMinActiveLimit = 1;
static const int kMaxActiveLimit = 32768;

static const int kMinReconnectTime = 0;
static const int kMaxReconnectTime = 100;

static const int kMinCacheSize = 1;
static const int kMaxCacheSize = 1024;

static const int kMinCacheExpiry = 1;
static const int kMaxCacheExpiry = 32768;

static const int kMinMetadataSize = 1;
static const int kMaxMetadataSize = 50;

static const int kMinSeedAnnounceInterval = 10;
static const int kMaxSeedAnnounceInterval = 32768;

static const int kMinPeerConnectionTimeout = 1;
static const int kMaxPeerConnectionTimeout = 128;

static const int kMinReadCacheLineSize = 0;
static const int kMaxReadCacheLineSize = 1024;

static const int kMinWriteCacheLineSize = 0;
static const int kMaxWriteCacheLineSize = 1024;

static const int kMinQueuedDiskBytes = 0;
static const int kMaxQueuedDiskBytes = 1024;

static const int kMinFilePoolSize = 1;
static const int kMaxFilePoolSize = 1024;

static const int kMinOutRequestQueue = 1;
static const int kMaxOutRequestQueue = 32768;

static const int kMinAllowedInRequestQueue = 1;
static const int kMaxAllowedInRequestQueue = 32768;

static const int kMinWholePiecesThreshold = 1;
static const int kMaxWholePiecesThreshold = 128;

static const int kMinRequestQueueTime = 1;
static const int kMaxRequestQueueTime = 128;

static const int kMinSendBufferLowWatermark = 0;
static const int kMaxSendBufferLowWatermark = 10240;

static const int kMinSendBufferWatermark = 0;
static const int kMaxSendBufferWatermark = 1024;

static const int kMinSendSocketBufferSize = 0;
static const int kMaxSendSocketBufferSize = 32768;

static const int kMinRecvSocketBufferSize = 0;
static const int kMaxRecvSocketBufferSize = 32768;


}  // namespace group;
}  // namespace bbts;

#endif  // OP_OPED_NOAH_TOOLS_BBTS_GROUP_CONF_RANGE_H_
