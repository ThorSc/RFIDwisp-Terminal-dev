// Host tests of the box reading (src/moonraker_box.cpp) against canned
// Moonraker answers, in the old format (`hardware` + `slots`) and in the one of
// newer firmware (Max4 01.01.07.01, Plus4 2.1.1: flat `box`, `box_stepper`
// runout sensors, `save_variables`). The new-format answers are trimmed from a
// Plus4 read live; the old format is built the way the app's unit tests do.
//
// Run with `pio test -e native` from wt32sc01plus/. Prints "<n> checks, <m>
// failed" and one FAIL line per failed check.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdio.h>
#include <map>
#include <string>
#include "moonraker_box.h"

static int checks = 0;
static int failed = 0;

static void check(bool ok, const char *name) {
  checks++;
  if (!ok) {
    failed++;
    printf("FAIL %s\n", name);
  }
}

// The answers of a "printer": object list (as asked) -> JSON text. A request
// that has no entry fails like a network error.
using Answers = std::map<std::string, std::string>;

static MoonrakerFetch fetcher(const Answers &answers) {
  return [&answers](const String &objects, JsonDocument &filter, JsonDocument &doc, String &error) {
    auto found = answers.find(objects.c_str());
    if (found == answers.end()) {
      error = "no answer";
      return false;
    }
    DeserializationError e =
        deserializeJson(doc, found->second.c_str(), DeserializationOption::Filter(filter));
    if (e) {
      error = "malformed";
      return false;
    }
    return true;
  };
}

static const char *kMain = "multi_color_controller&rfid_bridge";

static std::string status(const std::string &inner) {
  return "{\"result\":{\"status\":{" + inner + "}}}";
}

// A tag block as rfid_bridge reports it: vendor id in byte 13, spool number in
// bytes 14-15 (big-endian).
static std::string tagBlock(int vendorId, int spool) {
  char text[40];
  snprintf(text, sizeof text, "00000000000000000000000000%02X%04X", vendorId, spool);
  return text;
}

// ---- old format --------------------------------------------------------

static std::string slotMaterial(const char *filament, const char *vendor, const char *color) {
  return std::string("{\"filament\":{\"filament\":\"") + filament + "\",\"id\":3},\"vendor\":\"" + vendor +
         "\",\"color\":\"" + color + "\"}";
}

// `states`: state of slot0..15 as a JSON object body; `materials`: body.
static std::string oldAnswer(int boxCount, bool connected, const std::string &states,
                             const std::string &materials, const std::string &lastRaw = "") {
  return status("\"multi_color_controller\":{\"hardware\":{\"box_count\":" + std::to_string(boxCount) +
                ",\"connected\":" + (connected ? "true" : "false") + ",\"x\":1},\"slots\":{\"states\":{" +
                states + "},\"materials\":{" + materials + "}}}" +
                (lastRaw.empty() ? "" : ",\"rfid_bridge\":{\"last_raw\":{" + lastRaw + "}}"));
}

static std::string states(const std::map<int, int> &set) {
  std::string out;
  for (int n = 0; n < 16; n++) {
    auto s = set.find(n);
    out += std::string(n ? "," : "") + "\"slot" + std::to_string(n) + "\":" + std::to_string(s == set.end() ? 0 : s->second);
  }
  return out;
}

static void testOldFormat() {
  std::string materials = "\"slot0\":" + slotMaterial("PLA", "Generic", "#FFC000") + ",\"slot1\":" +
                          slotMaterial("PETG", "QIDI", "#1E88E5") + ",\"slot5\":" +
                          slotMaterial("ASA", "Generic", "#202020");
  std::string lastRaw = "\"slot0\":\"" + tagBlock(5, 12) + "\"";
  Answers answers{{kMain, oldAnswer(2, true, states({{0, 1}, {1, 2}, {5, 1}}), materials, lastRaw)}};

  QidiBoxResult box1 = moonrakerReadBoxWith(fetcher(answers), 1);
  check(box1.ok && !box1.unsupported, "old: box 1 ok");
  check(box1.boxCount == 2, "old: box count");
  check(box1.slots.size() == 4, "old: four slots");
  if (box1.slots.size() == 4) {
    check(box1.slots[0].loaded && !box1.slots[0].active, "old: slot 0 loaded, not active");
    check(box1.slots[0].material == "PLA" && box1.slots[0].vendor == "Generic" &&
              box1.slots[0].colorHex == "#FFC000",
          "old: slot 0 material, vendor, colour");
    check(box1.slots[0].spoolNumber == 12 && box1.slots[0].internalVendorId == 5, "old: slot 0 tag data");
    check(box1.slots[1].loaded && box1.slots[1].active, "old: state 2 is the active slot");
    check(box1.slots[1].spoolNumber == -1, "old: no tag data without last_raw");
    check(!box1.slots[2].loaded && box1.slots[2].material.isEmpty(), "old: empty slot");
  }

  QidiBoxResult box2 = moonrakerReadBoxWith(fetcher(answers), 2);
  check(box2.ok && box2.slots.size() == 4 && box2.slots[0].slot == 4, "old: box 2 starts at slot 4");
  check(box2.slots.size() == 4 && box2.slots[1].loaded && box2.slots[1].material == "ASA",
        "old: box 2 slot 5");

  QidiBoxResult box3 = moonrakerReadBoxWith(fetcher(answers), 3);
  check(box3.ok && box3.boxCount == 2 && box3.slots.empty(), "old: a box beyond the count is empty, no error");

  Answers off{{kMain, oldAnswer(2, false, states({}), "")}};
  QidiBoxResult disconnected = moonrakerReadBoxWith(fetcher(off), 1);
  check(disconnected.ok && disconnected.boxCount == 0 && disconnected.slots.empty(),
        "old: box hardware not connected gives count 0");

  Answers badCount{{kMain, oldAnswer(7, true, states({}), "")}};
  QidiBoxResult invalid = moonrakerReadBoxWith(fetcher(badCount), 1);
  check(!invalid.ok && !invalid.unsupported, "old: invalid box count is an error, not 'unsupported'");

  Answers missingState{{kMain, oldAnswer(1, true, "\"slot0\":1", materials)}};
  QidiBoxResult broken = moonrakerReadBoxWith(fetcher(missingState), 1);
  check(!broken.ok, "old: a missing slot state is an error");

  Answers noData{{kMain, oldAnswer(1, true, states({{0, 1}}), "")}};
  QidiBoxResult noMaterial = moonrakerReadBoxWith(fetcher(noData), 1);
  check(!noMaterial.ok, "old: a loaded slot without material data is an error");

  Answers hardwareOnly{{kMain, status("\"multi_color_controller\":{\"hardware\":{}}")}};
  QidiBoxResult invalidHardware = moonrakerReadBoxWith(fetcher(hardwareOnly), 1);
  check(!invalidHardware.ok && invalidHardware.unsupported, "old: empty hardware status is unsupported");
}

// ---- new format --------------------------------------------------------

struct FlatSlot {
  const char *material;
  const char *vendor;
  const char *color;
};

// multi_color_controller of newer firmware with the slots of box `index`.
static std::string newController(int index, const FlatSlot slots[4], const std::string &top) {
  std::string box = "\"box_count\":1,\"identity\":\"box_v1\"";
  for (int m = 0; m < 4; m++) {
    std::string p = "\"box" + std::to_string(index) + "_filament_info_" + std::to_string(m) + "_";
    box += "," + p + "filament_id\":\"QFG00\"," + p + "filament_str\":\"" + slots[m].material + "\"," + p +
           "vendor_str\":\"" + slots[m].vendor + "\"," + p + "color_rgb_str\":[\"" + slots[m].color + "\"]," + p +
           "min_temp\":240";
  }
  return "\"multi_color_controller\":{\"task\":{},\"box\":{" + box + "}" + top + "}";
}

static const char *kStateQuery0 =
    "save_variables&box_stepper%20slot0&box_stepper%20slot1&box_stepper%20slot2&box_stepper%20slot3";
static const char *kStateQuery1 =
    "save_variables&box_stepper%20slot4&box_stepper%20slot5&box_stepper%20slot6&box_stepper%20slot7";

// box_stepper runout sensors (0 = filament) of four slots from `first`, plus
// save_variables.
static std::string stateAnswer(int first, const int runout[4], const std::string &variables) {
  std::string out;
  for (int m = 0; m < 4; m++) {
    if (runout[m] < 0) continue; // sensor not exposed
    out += "\"box_stepper slot" + std::to_string(first + m) + "\":{\"runout_button\":" +
           std::to_string(runout[m]) + ",\"rfid_state\":0},";
  }
  return status(out + "\"save_variables\":{\"variables\":{" + variables + ",\"filament_slot0\":7}}");
}

static void testNewFormat() {
  const FlatSlot slots[4] = {{"PLA Basic", "QIDI", "#228332"},
                             {"PETG Basic", "QIDI", "#FAFAFA"},
                             {"PETG Basic", "Generic", "#060606"},
                             {"ABS", "QIDI", "#FF0000"}};
  std::string top = ",\"box_count\":1,\"enable_box\":1,\"operation\":{\"active_slot\":\"slot16\"}";
  std::string lastRaw = ",\"rfid_bridge\":{\"last_raw\":{\"slot0\":\"" + tagBlock(5, 258) + "\"}}";
  std::string mainAnswer = status(newController(0, slots, top).substr(0) + lastRaw);

  // Plus4 as read live: slot 0 loaded, the others empty, nothing feeding.
  {
    const int runout[4] = {0, 1, 1, 1};
    Answers answers{{kMain, mainAnswer},
                    {kStateQuery0, stateAnswer(0, runout, "\"slot_sync\":\"slot-1\",\"last_load_slot\":\"slot-1\",\"slot0\":1")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && !r.unsupported, "new: box 1 ok");
    check(r.boxCount == 1, "new: box count");
    check(r.slots.size() == 4, "new: four slots");
    if (r.slots.size() == 4) {
      check(r.slots[0].loaded && !r.slots[0].active, "new: slot 0 loaded, not active");
      check(r.slots[0].material == "PLA Basic" && r.slots[0].vendor == "QIDI" && r.slots[0].colorHex == "#228332",
            "new: slot 0 material, vendor, colour (colour from the list)");
      check(r.slots[0].spoolNumber == 258 && r.slots[0].internalVendorId == 5, "new: slot 0 tag data");
      check(!r.slots[1].loaded && r.slots[1].material.isEmpty() && r.slots[1].colorHex.isEmpty(),
            "new: slot 1 empty (runout sensor 1)");
    }
  }

  // The slot that feeds the extruder: save_variables' slot_sync wins.
  {
    const int runout[4] = {0, 0, 1, 1};
    Answers answers{{kMain, mainAnswer}, {kStateQuery0, stateAnswer(0, runout, "\"slot_sync\":\"slot1\",\"last_load_slot\":\"slot0\"")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.slots.size() == 4 && r.slots[1].active && !r.slots[0].active, "new: active slot from slot_sync");
  }
  // "slot-1" in slot_sync means none; last_load_slot is the next source.
  {
    const int runout[4] = {0, 0, 1, 1};
    Answers answers{{kMain, mainAnswer}, {kStateQuery0, stateAnswer(0, runout, "\"slot_sync\":\"slot-1\",\"last_load_slot\":\"slot0\"")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.slots.size() == 4 && r.slots[0].active, "new: active slot falls back to last_load_slot");
  }
  // Without slot_sync in save_variables the operation's active slot counts.
  {
    std::string topSlot1 = ",\"box_count\":1,\"enable_box\":1,\"operation\":{\"active_slot\":\"slot2\"}";
    const int runout[4] = {0, 0, 0, 1};
    Answers answers{{kMain, status(newController(0, slots, topSlot1))},
                    {kStateQuery0, stateAnswer(0, runout, "\"x\":1")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.slots.size() == 4 && r.slots[2].active, "new: active slot from operation.active_slot");
  }

  // No runout sensor exposed: the slotN flag of save_variables decides.
  {
    const int runout[4] = {-1, -1, -1, -1};
    Answers answers{{kMain, mainAnswer}, {kStateQuery0, stateAnswer(0, runout, "\"slot0\":0,\"slot1\":1,\"slot2\":0,\"slot3\":0")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.slots.size() == 4 && !r.slots[0].loaded && r.slots[1].loaded && r.slots[1].material == "PETG Basic",
          "new: loaded state from save_variables without runout sensors");
  }

  // The state request fails: the box still shows, every slot as empty.
  {
    Answers answers{{kMain, mainAnswer}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.slots.size() == 4 && !r.slots[0].loaded && !r.slots[1].loaded,
          "new: failed state request leaves the slots empty, no error");
  }

  // Box 2: its own keys (box1_...) and slots 4-7.
  {
    std::string twoBoxes = status(newController(1, slots, ",\"box_count\":2,\"enable_box\":1"));
    const int runout[4] = {1, 0, 1, 1};
    Answers answers{{kMain, twoBoxes}, {kStateQuery1, stateAnswer(4, runout, "\"slot_sync\":\"slot-1\"")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 2);
    check(r.ok && r.boxCount == 2 && r.slots.size() == 4 && r.slots[0].slot == 4, "new: box 2 starts at slot 4");
    check(r.slots.size() == 4 && r.slots[1].loaded && r.slots[1].material == "PETG Basic", "new: box 2 slot 5");
  }

  // Box beyond the count: no error, nothing to read.
  {
    Answers answers{{kMain, mainAnswer}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 2);
    check(r.ok && r.boxCount == 1 && r.slots.empty(), "new: a box beyond the count is empty, no error");
  }

  // enable_box 0 (box switched off): count 0.
  {
    std::string off = status(newController(0, slots, ",\"box_count\":1,\"enable_box\":0"));
    Answers answers{{kMain, off}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.boxCount == 0 && r.slots.empty(), "new: enable_box 0 gives count 0");
  }

  // The count only inside `box`.
  {
    std::string inner = status(newController(0, slots, ""));
    const int runout[4] = {0, 1, 1, 1};
    Answers answers{{kMain, inner}, {kStateQuery0, stateAnswer(0, runout, "\"slot_sync\":\"slot-1\"")}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(r.ok && r.boxCount == 1, "new: box count from box.box_count");
  }

  // No count at all, or no data of the requested box: unsupported.
  {
    std::string noCount = status("\"multi_color_controller\":{\"box\":{\"identity\":\"box_v1\"}}");
    Answers answers{{kMain, noCount}};
    QidiBoxResult r = moonrakerReadBoxWith(fetcher(answers), 1);
    check(!r.ok && r.unsupported, "new: no box count is unsupported");

    std::string noInfo = status("\"multi_color_controller\":{\"box_count\":1,\"enable_box\":1,\"box\":{\"box_count\":1}}");
    Answers answers2{{kMain, noInfo}};
    QidiBoxResult r2 = moonrakerReadBoxWith(fetcher(answers2), 1);
    check(!r2.ok && r2.unsupported, "new: no filament info of the box is unsupported");
  }
}

// ---- other printers and failures ---------------------------------------

static void testOthers() {
  // A printer without multi_color_controller (e.g. a Q2).
  Answers q2{{kMain, status("\"rfid_bridge\":{\"last_raw\":{}}")}};
  QidiBoxResult r = moonrakerReadBoxWith(fetcher(q2), 1);
  check(!r.ok && r.unsupported, "no multi_color_controller is unsupported");

  Answers none;
  QidiBoxResult failedRequest = moonrakerReadBoxWith(fetcher(none), 1);
  check(!failedRequest.ok && !failedRequest.unsupported && failedRequest.error == "no answer",
        "a failed request passes its error on");

  QidiBoxResult outOfRange = moonrakerReadBoxWith(fetcher(none), 5);
  check(!outOfRange.ok && !outOfRange.error.isEmpty(), "box number out of range");
}

int main() {
  testOldFormat();
  testNewFormat();
  testOthers();
  printf("%d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
