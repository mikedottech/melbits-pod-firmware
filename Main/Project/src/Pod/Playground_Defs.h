// SPDX-FileCopyrightText: 2017-2020 Melbot Studios, S.L.
// SPDX-License-Identifier: LicenseRef-Melbot-Portfolio
//
// Original author: Miguel Angel Exposito (2017-2020)
//
// This source code is the property of Melbot Studios, S.L. and is published
// publicly for portfolio and educational review purposes with the express
// written authorization of Melbot Studios, S.L.
//
// All rights remain with Melbot Studios, S.L. Redistribution, modification,
// commercial use, or derivative works are not permitted without prior
// written consent of the copyright holder. See LICENSE for full terms.

#ifndef _POD_PLAYGROUND_DEFS_H_
#define _POD_PLAYGROUND_DEFS_H_

#include "common.h"
#include "mbt_config.h"

// CHANNELS ----------------------------------------------------------------------------------
enum
{
    POD_PG_CHANNEL_MAIN = 0,
    POD_PG_CHANNEL_FILE,
    POD_PG_CHANNEL_STREAM,
    POD_PG_CHANNEL_EVENT,
    POD_PG_CHANNEL_MAX
};

// REQUEST CODES -----------------------------------------------------------------------------
enum
{
    POD_PG_OPCODE_INVALID = 0,

// FILE XFER ----------------------------------------
    POD_PG_OPCODE_STOREARCHIVE,
    POD_PG_OPCODE_ERASEPAGES,
    POD_PG_OPCODE_ABORTXFER,
    POD_PG_OPCODE_PUTFILE,          // RAM files only
    POD_PG_OPCODE_DELFILE,          // Flash files only
    POD_PG_OPCODE_GETFILE,          // All files
    POD_PG_OPCODE_GETFILEINFO,      // All files

// UI -----------------------------------------------
    POD_PG_OPCODE_PLAYCLIP,
    POD_PG_OPCODE_ABORTCLIP,

// ACTIVITY -----------------------------------------
    POD_PG_OPCODE_START_ACTIVITY,
    POD_PG_OPCODE_STOP_ACTIVITY,
    POD_PG_OPCODE_GET_ACTIVITY_STATE,

// STREAM -------------------------------------------
    POD_PG_OPCODE_SETSTREAMITEMSMASK,

// EVENTS -------------------------------------------
    POD_PG_OPCODE_SETEVENTITEMSMASK,

// VIBRATION ----------------------------------------
    POD_PG_OPCODE_VIBRATION,

// BOX ----------------------------------------------
    POD_PG_OPCODE_ENTERBOXMODE,

// DFU ----------------------------------------------
    POD_PG_OPCODE_ENTERDFUMODE,

// TEST ---------------------------------------------
    POD_PG_OPCODE_TESTCMD,

// NONCE --------------------------------------------
    POD_PG_OPCODE_NONCEGET,

// CHALLENGE ----------------------------------------
    POD_PG_CHALLENGERSP,

// PIN ----------------------------------------------
    POD_PG_OPCODE_PINSET = 200

// --------------------------------------------------
    //POD_PG_OPCODE_REPLY = 254
};

// RESPONSE CODES ----------------------------------------------------------------------------
enum
{
    POD_PG_RSCODE_INVALID = 0,
    POD_PG_RSCODE_OK,
    POD_PG_RSCODE_INVALID_STATE,
    POD_PG_RSCODE_EXTENDED,
    POD_PG_RSCODE_INVALID_PARAM,
    POD_PG_RSCODE_RESTRICTED,
    POD_PG_RSCODE_PENDING,
    POD_PG_RSCODE_NOTIMPLEMENTED
};

// EVENT CODES -------------------------------------------------------------------------------
enum
{
    POD_PG_EVCODE_INVALID = 0,
    POD_PG_EVCODE_FILESRV_EXTENDED,
    POD_PG_EVCODE_FILESRV_FLOW_CTRL,       // Storage OK. Please send more data
    POD_PG_EVCODE_FILESRV_OP_COMPLETE,     // File operatin finished. No more data allowed on file channel    
    POD_PG_EVCODE_EVTSRV,
    POD_PG_EVCODE_HELLO, // Handshake packet
    POD_PG_EVCODE_PINACCEPTED               // User has confirmed the PIN
};

typedef uint64_t Pod_PGPIN_t;

// REQUEST PAYLOADS -------------------------------------------------------------------------
typedef struct
{
    uint16_t    offset;
    uint16_t    len;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqStoreArcPayload_t;

typedef struct
{
    uint16_t    id;
    uint16_t    len;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqPutFilePayload_t;

typedef struct
{
    uint16_t    id;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqGetFileInfoPayload_t;

typedef struct
{
    uint16_t    id;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqGetFilePayload_t;

typedef struct
{
    uint16_t    id;
    uint8_t     extra;
    uint16_t    prio;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqPlayClipPayload_t;

typedef struct
{
    uint8_t     level;
    uint16_t    time_ms;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqVibrationPayload_t;

typedef struct
{
    uint8_t     id;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqStartActivityPayload_t;

typedef struct
{
    uint8_t     mask;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqSetStreamItemsMaskPayload_t;

typedef struct
{
    uint16_t    mask;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqSetEventItemsMaskPayload_t;

typedef struct
{
    uint8_t    cmd;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqTestCommandPayload_t;

#define POD_PG_PINSER_FLAGS_CLEARPIN (1 << 0)
#define POD_PG_PINSER_FLAGS_MLINK    (1 << 1)

typedef struct
{
    Pod_PGPIN_t    pin;
    uint8_t        flags;
} MBT_CFG_PLATFORM_PACKED Pod_PGReqPinSetCommandPayload_t;

typedef struct
{
} MBT_CFG_PLATFORM_PACKED Pod_PGReqNonceGetCommandPayload_t;

// RESPONSE PAYLOADS -------------------------------------------------------------------------
typedef struct
{
    uint16_t    size;
} MBT_CFG_PLATFORM_PACKED Pod_PGRspFileInfoPayload_t;

typedef struct
{
    uint16_t    code;
} MBT_CFG_PLATFORM_PACKED Pod_PGRspExtendedCodePayload_t;

typedef struct
{
    uint8_t     id;
    uint16_t    state;
} MBT_CFG_PLATFORM_PACKED Pod_PGRspActivityStatePayload_t;

typedef struct
{
    uint8_t     nonce[12];
} MBT_CFG_PLATFORM_PACKED Pod_PGRspNonceGetPayload_t;

// EVENT PAYLOADS ----------------------------------------------------------------------------
typedef struct
{
    uint32_t    masked;
    uint32_t    activity;
} MBT_CFG_PLATFORM_PACKED Pod_PGEvtEventSvrPayload_t;

typedef struct
{
    uint16_t    code;
} MBT_CFG_PLATFORM_PACKED Pod_PGEvtExtendedCodePayload_t;


// STREAM PAYLOADSS --------------------------------------------------------------------------
typedef struct
{
    uint8_t     strmSources;
    HAL_analogSample_t     firstSample;
} MBT_CFG_PLATFORM_PACKED Pod_PGStrmPayloadPayload_t;

// COMMON HEADER -----------------------------------------------------------------------------
typedef struct
{   // 0xab
    uint8_t     chan : 4;   // Channel ID (b)
    uint8_t     seq  : 4;   // Sequence number (a)
    uint8_t     code;       // Operation code for requests, event code for events, result code for replies
} MBT_CFG_PLATFORM_PACKED Pod_PGCommonHeader_t;

typedef struct
{
    Pod_PGCommonHeader_t header;

    union
    {
        union
        {
            Pod_PGReqStoreArcPayload_t              storeArchive;
            Pod_PGReqGetFileInfoPayload_t           getFileInfo;
            Pod_PGReqGetFilePayload_t               getFile;
            Pod_PGReqPutFilePayload_t               putfile;
            Pod_PGReqPlayClipPayload_t              playClip;
            Pod_PGReqVibrationPayload_t             vibration;
            Pod_PGReqStartActivityPayload_t         startActivity;
            Pod_PGReqSetStreamItemsMaskPayload_t    setStreamItemsMask;
            Pod_PGReqSetEventItemsMaskPayload_t     setEventItemsMask;
            Pod_PGReqTestCommandPayload_t           testCommand;
            Pod_PGReqPinSetCommandPayload_t         pinSet;
        } req;

        union
        {
            Pod_PGRspFileInfoPayload_t              fileInfo;
            Pod_PGRspExtendedCodePayload_t          extendedCode;
            Pod_PGRspActivityStatePayload_t         activityState;
            Pod_PGRspNonceGetPayload_t              encryptionNonce;
        } rsp;

        union
        {
            Pod_PGEvtEventSvrPayload_t              events;
            Pod_PGEvtExtendedCodePayload_t          extended;
        } evt;

        union
        {
            Pod_PGStrmPayloadPayload_t              stream;
        } strm;
    } payload;
} MBT_CFG_PLATFORM_PACKED Pod_PGPDU_t;

#endif
