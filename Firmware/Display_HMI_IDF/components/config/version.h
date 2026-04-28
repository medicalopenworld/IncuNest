/**
 * @file version.h
 * @brief Single authoritative source of the firmware version string.
 *
 * @details ONLY this file defines the version. Never write version strings
 *          elsewhere. Screens, boot messages, and OTA all read from here.
 *
 * Normativa aplicable:
 * - IEC 62304 §8.1.3 — Software configuration item identification
 *
 * @author IncuNest Team
 * @date   2026-04-28
 */
#pragma once

#define FW_VERSION_MAJOR  0
#define FW_VERSION_MINOR  1
#define FW_VERSION_PATCH  0
#define FW_VERSION_STR    "0.1.0"

/** Full build identifier including date — used in screen_info and boot log */
#define FW_BUILD_ID       "IncuNest-HMI v" FW_VERSION_STR " (IDF)"
