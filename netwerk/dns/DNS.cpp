/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DNS.h"

#include "mozilla/Assertions.h"
#include "mozilla/mozalloc.h"
#include <string.h>

#ifdef XP_WIN
#include "ws2tcpip.h"
#endif

namespace mozilla {
namespace net {

#ifdef XP_WIN
int WINAPI
WspiapiLegacyGetNameInfo (const struct sockaddr *ptSocketAddress,
			 socklen_t tSocketLength,
			 char *pszNodeName, size_t tNodeLength,
			 char *pszServiceName, size_t tServiceLength,
			 int iFlags)
{
  struct servent *svc;
  WORD port;
  char str[] = "65535";
  char *pstr = str;
  struct hostent *phost;
  struct in_addr l_inaddr;
  char *pnode = NULL, *pc = NULL;

  if (!ptSocketAddress || tSocketLength < (int) sizeof (struct sockaddr))
    return EAI_FAIL;
  if (ptSocketAddress->sa_family != AF_INET)
    return EAI_FAMILY;
  if (tSocketLength < (int) sizeof (struct sockaddr_in))
    return EAI_FAIL;
  if (!(pszNodeName && tNodeLength) && !(pszServiceName && tServiceLength))
    return EAI_NONAME;
  if ((iFlags & NI_NUMERICHOST) != 0 && (iFlags & NI_NAMEREQD) != 0)
    return EAI_BADFLAGS;
  if (pszServiceName && tServiceLength)
    {
	port = ((struct sockaddr_in *) ptSocketAddress)->sin_port;
	if (iFlags & NI_NUMERICSERV)
	  sprintf (str, "%u", ntohs (port));
      else
	  {
	    svc = getservbyport(port, (iFlags & NI_DGRAM) ? "udp" : NULL);
	    if (svc && svc->s_name)
	      pstr = svc->s_name;
	    else
	      sprintf (str, "%u", ntohs (port));
	  }
	if (tServiceLength > strlen (pstr))
	  strcpy (pszServiceName, pstr);
	else
	  return EAI_FAIL;
    }
  if (pszNodeName && tNodeLength)
    {
	l_inaddr = ((struct sockaddr_in *) ptSocketAddress)->sin_addr;
	if (iFlags & NI_NUMERICHOST)
	  pnode = inet_ntoa (l_inaddr);
	else
	  {
	    phost = gethostbyaddr ((char *) &l_inaddr, sizeof (struct in_addr), AF_INET);
	    if (phost && phost->h_name)
	      {
		pnode = phost->h_name;
		if ((iFlags & NI_NOFQDN) != 0 && ((pc = strchr (pnode,'.')) != NULL))
		  *pc = 0;
	      }
	    else
	      {
		if ((iFlags & NI_NAMEREQD) != 0)
		  {
		    switch(WSAGetLastError())
		      {
		      case WSAHOST_NOT_FOUND: return EAI_NONAME;
		      case WSATRY_AGAIN: return EAI_AGAIN;
		      case WSANO_RECOVERY: return EAI_FAIL;
		      default: return EAI_NONAME;
		      }
		  }
		else
		  pnode = inet_ntoa (l_inaddr);
	      }
	  }
	if (tNodeLength > strlen (pnode))
	  strcpy (pszNodeName, pnode);
	else
	  return EAI_FAIL;
    }
  return 0;
}
#endif

const char *inet_ntop_internal(int af, const void *src, char *dst, socklen_t size)
{
#ifdef XP_WIN
  if (af == AF_INET) {
    struct sockaddr_in s;
    memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET;
    memcpy(&s.sin_addr, src, sizeof(struct in_addr));
    int result = WspiapiLegacyGetNameInfo((struct sockaddr *)&s, sizeof(struct sockaddr_in),
                             dst, size, nullptr, 0, NI_NUMERICHOST);
    if (result == 0) {
      return dst;
    }
  }
  else if (af == AF_INET6) {
    struct sockaddr_in6 s;
    memset(&s, 0, sizeof(s));
    s.sin6_family = AF_INET6;
    memcpy(&s.sin6_addr, src, sizeof(struct in_addr6));
    int result = WspiapiLegacyGetNameInfo((struct sockaddr *)&s, sizeof(struct sockaddr_in6),
                             dst, size, nullptr, 0, NI_NUMERICHOST);
    if (result == 0) {
      return dst;
    }
  }
  return nullptr;
#else
  return inet_ntop(af, src, dst, size);
#endif
}

// Copies the contents of a PRNetAddr to a NetAddr.
// Does not do a ptr safety check!
void PRNetAddrToNetAddr(const PRNetAddr *prAddr, NetAddr *addr)
{
  if (prAddr->raw.family == PR_AF_INET) {
    addr->inet.family = AF_INET;
    addr->inet.port = prAddr->inet.port;
    addr->inet.ip = prAddr->inet.ip;
  }
  else if (prAddr->raw.family == PR_AF_INET6) {
    addr->inet6.family = AF_INET6;
    addr->inet6.port = prAddr->ipv6.port;
    addr->inet6.flowinfo = prAddr->ipv6.flowinfo;
    memcpy(&addr->inet6.ip, &prAddr->ipv6.ip, sizeof(addr->inet6.ip.u8));
    addr->inet6.scope_id = prAddr->ipv6.scope_id;
  }
#if defined(XP_UNIX)
  else if (prAddr->raw.family == PR_AF_LOCAL) {
    addr->local.family = AF_LOCAL;
    memcpy(addr->local.path, prAddr->local.path, sizeof(addr->local.path));
  }
#endif
}

// Copies the contents of a NetAddr to a PRNetAddr.
// Does not do a ptr safety check!
void NetAddrToPRNetAddr(const NetAddr *addr, PRNetAddr *prAddr)
{
  if (addr->raw.family == AF_INET) {
    prAddr->inet.family = PR_AF_INET;
    prAddr->inet.port = addr->inet.port;
    prAddr->inet.ip = addr->inet.ip;
  }
  else if (addr->raw.family == AF_INET6) {
    prAddr->ipv6.family = PR_AF_INET6;
    prAddr->ipv6.port = addr->inet6.port;
    prAddr->ipv6.flowinfo = addr->inet6.flowinfo;
    memcpy(&prAddr->ipv6.ip, &addr->inet6.ip, sizeof(addr->inet6.ip.u8));
    prAddr->ipv6.scope_id = addr->inet6.scope_id;
  }
#if defined(XP_UNIX)
  else if (addr->raw.family == AF_LOCAL) {
    prAddr->local.family = PR_AF_LOCAL;
    memcpy(prAddr->local.path, addr->local.path, sizeof(addr->local.path));
  }
#endif
}

bool NetAddrToString(const NetAddr *addr, char *buf, uint32_t bufSize)
{
  if (addr->raw.family == AF_INET) {
    if (bufSize < INET_ADDRSTRLEN) {
      return false;
    }
    struct in_addr nativeAddr = {};
    nativeAddr.s_addr = addr->inet.ip;
    return !!inet_ntop_internal(AF_INET, &nativeAddr, buf, bufSize);
  }
  else if (addr->raw.family == AF_INET6) {
    if (bufSize < INET6_ADDRSTRLEN) {
      return false;
    }
    struct in6_addr nativeAddr = {};
    memcpy(&nativeAddr.s6_addr, &addr->inet6.ip, sizeof(addr->inet6.ip.u8));
    return !!inet_ntop_internal(AF_INET6, &nativeAddr, buf, bufSize);
  }
#if defined(XP_UNIX)
  else if (addr->raw.family == AF_LOCAL) {
    if (bufSize < sizeof(addr->local.path)) {
      return false;
    }
    memcpy(buf, addr->local.path, bufSize);
    return true;
  }
#endif
  return false;
}

bool IsLoopBackAddress(const NetAddr *addr)
{
  if (addr->raw.family == AF_INET) {
    return (addr->inet.ip == htonl(INADDR_LOOPBACK));
  }
  else if (addr->raw.family == AF_INET6) {
    if (IPv6ADDR_IS_LOOPBACK(&addr->inet6.ip)) {
      return true;
    } else if (IPv6ADDR_IS_V4MAPPED(&addr->inet6.ip) &&
               IPv6ADDR_V4MAPPED_TO_IPADDR(&addr->inet6.ip) == htonl(INADDR_LOOPBACK)) {
      return true;
    }
  }
  return false;
}

bool IsIPAddrAny(const NetAddr *addr)
{
  if (addr->raw.family == AF_INET) {
    if (addr->inet.ip == htonl(INADDR_ANY)) {
      return true;
    }
  }
  else if (addr->raw.family == AF_INET6) {
    if (IPv6ADDR_IS_UNSPECIFIED(&addr->inet6.ip)) {
      return true;
    } else if (IPv6ADDR_IS_V4MAPPED(&addr->inet6.ip) &&
               IPv6ADDR_V4MAPPED_TO_IPADDR(&addr->inet6.ip) == htonl(INADDR_ANY)) {
      return true;
    }
  }
  return false;
}

bool IsIPAddrV4Mapped(const NetAddr *addr)
{
  if (addr->raw.family == AF_INET6) {
    return IPv6ADDR_IS_V4MAPPED(&addr->inet6.ip);
  }
  return false;
}

NetAddrElement::NetAddrElement(const PRNetAddr *prNetAddr)
{
  PRNetAddrToNetAddr(prNetAddr, &mAddress);
}

NetAddrElement::~NetAddrElement()
{
}

AddrInfo::AddrInfo(const char *host, const PRAddrInfo *prAddrInfo,
                   const char *cname)
{
  size_t hostlen = strlen(host);
  mHostName = static_cast<char*>(moz_xmalloc(hostlen + 1));
  memcpy(mHostName, host, hostlen + 1);
  if (cname) {
      size_t cnameLen = strlen(cname);
      mCanonicalName = static_cast<char*>(moz_xmalloc(cnameLen + 1));
      memcpy(mCanonicalName, cname, cnameLen + 1);
  }
  else {
      mCanonicalName = nullptr;
  }

  PRNetAddr tmpAddr;
  void *iter = nullptr;
  do {
    iter = PR_EnumerateAddrInfo(iter, prAddrInfo, 0, &tmpAddr);
    if (iter) {
      NetAddrElement *addrElement = new NetAddrElement(&tmpAddr);
      mAddresses.insertBack(addrElement);
    }
  } while (iter);
}

AddrInfo::~AddrInfo()
{
  NetAddrElement *addrElement;
  while ((addrElement = mAddresses.popLast())) {
    delete addrElement;
  }
  moz_free(mHostName);
  moz_free(mCanonicalName);
}

} // namespace dns
} // namespace mozilla
