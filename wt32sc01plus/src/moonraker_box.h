#pragma once
#include <ArduinoJson.h>
#include <functional>
#include "moonraker_client.h"

// The reading of a QIDI box from Moonraker's answers, without the network: the
// answers come from a [MoonrakerFetch], so the same code runs on the printer
// (moonrakerReadBox in moonraker_client.cpp) and in the host tests
// (test/test_moonraker).

// Queries the printer objects [objects] (the part after "?" of
// /printer/objects/query, e.g. "multi_color_controller&rfid_bridge") and parses
// the answer through [filter] into [doc]; on failure [error] says why.
using MoonrakerFetch = std::function<bool(const String &objects, JsonDocument &filter,
                                          JsonDocument &doc, String &error)>;

QidiBoxResult moonrakerReadBoxWith(const MoonrakerFetch &fetch, int box);
