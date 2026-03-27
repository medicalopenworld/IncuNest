
#ifndef _WIFI_OTA_H_
#define _WIFI_OTA_H_

// WiFi connectivity removed. All functions return disabled state.
// Server connectivity is handled exclusively via GPRS.

inline bool WIFIIsConnected()        { return false; }
inline bool WIFIIsConnectedToServer(){ return false; }
inline bool WIFICheckNewEvent()      { return false; }
inline void wifiInit()               {}
inline void wifiDisable()            {}

#endif // _WIFI_OTA_H_
