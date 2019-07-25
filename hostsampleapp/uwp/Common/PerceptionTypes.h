//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include <guiddef.h>

DEFINE_GUID(SPATIALPROP_QRTrackerObjectId, 0xf2e86326, 0xfbd8, 0x4088, 0xb5, 0xfb, 0xac, 0x61, 0xc, 0x3, 0x99, 0x7f);
DEFINE_GUID(SPATIALPROP_QRTracker_TrackingStatus, 0x270f00dc, 0xc0d9, 0x442c, 0x87, 0x25, 0x27, 0xb6, 0xbb, 0xd9, 0x43, 0xd);
DEFINE_GUID(SPATIALPROP_QRTracker_QRCodesList, 0x338b32f8, 0x1ce0, 0x4e75, 0x8d, 0xf4, 0x56, 0xd4, 0x2f, 0x73, 0x96, 0x74);
DEFINE_GUID(SPATIALPROP_QRCode_PhysicalSize, 0xcfb07ae5, 0x456a, 0x4aaf, 0x9d, 0x6a, 0xa2, 0x9d, 0x3, 0x9b, 0xc0, 0x56);
DEFINE_GUID(SPATIALPROP_QRCode_LastSeenTime, 0xb2b08c2d, 0xb531, 0x4f18, 0x87, 0x84, 0x44, 0x84, 0x46, 0x3d, 0x88, 0x53);
DEFINE_GUID(SPATIALPROP_QRCode_StreamInfo, 0x609143ea, 0x4ec5, 0x4b0e, 0xba, 0xef, 0x52, 0xa5, 0x5c, 0xfc, 0x23, 0x58);


#pragma pack(push, 1)
typedef struct SPATIAL_GRAPH_QR_CODE_STREAM_INFO
{
    UINT32 Version;
    ULONG StreamSize;

    _Field_size_(StreamSize) BYTE StreamData[ANYSIZE_ARRAY];

} SPATIAL_GRAPH_QR_CODE_STREAM_INFO, *PSPATIAL_GRAPH_QR_CODE_STREAM_INFO;
#pragma pack(pop)
