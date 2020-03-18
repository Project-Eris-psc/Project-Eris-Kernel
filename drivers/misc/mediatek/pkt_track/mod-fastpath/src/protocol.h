#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#include <linux/in.h>

/* assume CPU is little endian */
enum {
	ETYPE_VLAN = htons(0x8100),
	ETYPE_IPv4 = htons(0x0800),
	ETYPE_IPv6 = htons(0x86dd),
	ETYPE_PPPoE = htons(0x8864),
	PPP_IPv4 = htons(0x0021),
	PPP_IPv6 = htons(0x0057),
};

struct ip4header {
	unsigned char ip_hl:4, ip_v:4;	/* this means that each member is 4 bits */
	unsigned char ip_tos;
	unsigned short ip_len;
	unsigned short ip_id;
	unsigned short ip_off;
	unsigned char ip_ttl;
	unsigned char ip_p;
	unsigned short ip_sum;
	unsigned int ip_src;
	unsigned int ip_dst;
} __attribute__ ((__packed__));	/* total ip header length: 20 bytes (=160 bits) */

struct ip6header {
	unsigned char priority:4, version:4;
	unsigned char flow_lbl[3];
	unsigned short payload_len;
	unsigned char nexthdr;
	unsigned char hop_limit;
	struct in6_addr saddr;
	struct in6_addr daddr;
} __attribute__ ((__packed__));

#define TH_FIN	0x01
#define TH_SYN	0x02
#define TH_RST	0x04
#define TH_PUSH	0x08
#define TH_ACK	0x10
#define TH_URG	0x20

struct tcpheader {
	unsigned short th_sport;
	unsigned short th_dport;
	unsigned int th_seq;
	unsigned int th_ack;
	unsigned char th_x2:4, th_off:4;
	unsigned char th_flags;
	unsigned short th_win;
	unsigned short th_sum;
	unsigned short th_urp;
} __attribute__ ((__packed__));	/* total tcp header length: 20 bytes (=160 bits) */

struct udpheader {
	unsigned short uh_sport;
	unsigned short uh_dport;
	unsigned short uh_len;
	unsigned short uh_check;
} __attribute__ ((__packed__));	/* total udp header length: 8 bytes (=64 bits) */

struct icmpheader {
	unsigned char icmp_type;
	unsigned char icmp_code;
	unsigned short icmp_cksum;
	/* The following data structures are ICMP type specific */
	unsigned short icmp_id;
	unsigned short icmp_seq;
} __attribute__ ((__packed__));	/* total icmp header length: 8 bytes (=64 bits) */

struct greheader {
	unsigned char gre_s;
	unsigned char gre_f:5, gre_v:3;
	unsigned short gre_p;
	unsigned short gre_len;
	unsigned short gre_callid;
	unsigned long gre_seq;
	unsigned long gre_ack;
} __attribute__ ((__packed__));

#endif
