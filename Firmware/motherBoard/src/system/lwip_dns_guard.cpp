// Every DNS lookup of the firmware runs inside lwIP's own thread.
//
// lwIP's "raw" API (dns_*, udp_*, tcp_*) is not thread-safe: it may only be
// called from the TCP/IP thread ("tiT") or with the core lock held, and this
// SDK is built without the core lock (CONFIG_LWIP_TCPIP_CORE_LOCKING is not
// set). arduino-esp32 2.0.14 breaks that rule in one place:
// WiFiGenericClass::hostByName() calls dns_gethostbyname() straight from the
// task that wants the address. Here that is the OTA/WiFi task on core 1, while
// tiT runs on core 0 and walks the same DNS table once a second (dns_tmr).
//
// What it cost (known_issues.md #27): dns_enqueue() marks a table entry as new
// before it gives it a UDP pcb. If the DNS timer runs in between, it sends the
// entry with the pcb index left by the previous lookup, which is the "no pcb"
// mark (one past the end of dns_pcbs). The pointer read from there is not a
// pcb and udp_sendto() faults: PANIC in tiT, LoadProhibited, and the board
// restarts. Seen on the bench every hour or so with a ThingsBoard host that
// does not resolve.
//
// The fix does not touch the core. platformio.ini links with
// -Wl,--wrap=dns_gethostbyname, so every reference to dns_gethostbyname in
// the program (ours, the core's, the precompiled libraries') lands in
// __wrap_dns_gethostbyname below, and __real_dns_gethostbyname is lwIP's
// function. The wrapper hands the call to tiT with tcpip_api_call() and waits
// for it: same arguments, same return value, but now serialized with the
// timer and with the responses.
//
// Without the linker flag this file does nothing (nobody calls the wrapper),
// so the flag and the file go together: see [mb_hw18plus] in platformio.ini.
//
// arduino-esp32 3.x resolves through lwip_getaddrinfo(), which already does
// this by itself; when the core is updated this file and the flag can go.

#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/sys.h"

extern "C" {

err_t __real_dns_gethostbyname(const char *hostname, ip_addr_t *addr,
                               dns_found_callback found, void *callback_arg);

err_t __wrap_dns_gethostbyname(const char *hostname, ip_addr_t *addr,
                               dns_found_callback found, void *callback_arg);

} // extern "C"

namespace {

struct DnsCall {
  struct tcpip_api_call_data call; // first: tcpip_api_call() hands this back
  const char *hostname;
  ip_addr_t *addr;
  dns_found_callback found;
  void *callback_arg;
};

// Runs in tiT.
err_t dnsCallInTcpip(struct tcpip_api_call_data *call) {
  DnsCall *c = reinterpret_cast<DnsCall *>(call);
  return __real_dns_gethostbyname(c->hostname, c->addr, c->found,
                                  c->callback_arg);
}

} // namespace

err_t __wrap_dns_gethostbyname(const char *hostname, ip_addr_t *addr,
                               dns_found_callback found, void *callback_arg) {
  // Already in tiT (lwIP's own callers: netdb through api_msg, SNTP), or the
  // TCP/IP thread does not exist yet: call it directly. Posting to our own
  // mailbox and waiting for the answer would never return. The query is true
  // in both cases in a build without the core lock.
  if (sys_thread_tcpip(LWIP_CORE_LOCK_QUERY_HOLDER)) {
    return __real_dns_gethostbyname(hostname, addr, found, callback_arg);
  }
  // Blocks until tiT has run the call (the per-thread semaphore of lwIP);
  // `c` lives on this stack until then. The lookup itself stays asynchronous:
  // ERR_INPROGRESS comes back at once and `found` is called later, from tiT,
  // exactly as before.
  DnsCall c = {};
  c.hostname = hostname;
  c.addr = addr;
  c.found = found;
  c.callback_arg = callback_arg;
  return tcpip_api_call(dnsCallInTcpip, &c.call);
}
