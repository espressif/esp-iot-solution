/*************************************

Copyright (c) 2015-2050, JD Smart All rights reserved. 

*************************************/
#ifndef _JOYLINK_SMNT_ADP_H_
#define _JOYLINK_SMNT_ADP_H_
/*
  ≈‰≤„Œƒº˛
*/
typedef  unsigned char        uint8;
typedef  char                  int8;
typedef  unsigned short      uint16;
#define MAC_ADDR_LEN            6
#define SUBTYPE_PROBE_REQ       4


// 2-byte Frame control field
typedef    struct    GNU_PACKED {
    uint16        Ver:2;                // Protocol version
    uint16        Type:2;                // MSDU type
    uint16        SubType:4;            // MSDU subtype
    uint16        ToDs:1;                // To DS indication
    uint16        FrDs:1;                // From DS indication
    uint16        MoreFrag:1;            // More fragment bit
    uint16        Retry:1;            // Retry status bit
    uint16        PwrMgmt:1;            // Power management bit
    uint16        MoreData:1;            // More data bit
    uint16        Wep:1;                // Wep data
    uint16        Order:1;            // Strict order expected
} FRAME_CONTROL;

typedef    struct    _HEADER_802_11    {
    FRAME_CONTROL   FC;
    uint16          Duration;
    uint8           Addr1[MAC_ADDR_LEN];
    uint8           Addr2[MAC_ADDR_LEN];
    uint8            Addr3[MAC_ADDR_LEN];
    uint16            Frag:4;
    uint16            Sequence:12;
    uint8            Octet[0];
} *PHEADER_802_11;

#ifdef PLATFORM_ESP32
typedef PHEADER_802_11 PHEADER_802_11_SMNT ;
#endif
#endif
