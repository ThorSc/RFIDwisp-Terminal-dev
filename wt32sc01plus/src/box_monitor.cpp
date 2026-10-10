#include "box_monitor.h"
#include "moonraker_client.h"
#include "spoolman_client.h"
#include <WiFi.h>
#include <map>
#include <vector>

struct Request {
  uint32_t generation = 0;
  String printerAddress;
  int box = 1;
  String spoolmanAddress;
};

static SemaphoreHandle_t lock;
static Request pending;
static bool hasPending = false;
static bool running = false;
static BoxResult finished;
static bool hasFinished = false;
static uint32_t latestGeneration = 0;

static void resolve(const Request &request, BoxResult &out) {
  QidiBoxResult read = moonrakerReadBox(request.printerAddress, request.box);
  out.ok = read.ok;
  out.error = read.error;
  out.unsupported = read.unsupported;
  out.boxCount = read.boxCount;
  if (!read.ok || read.slots.size() != 4) return;

  std::map<int, String> vendors;
  std::map<int, int> weights;
  if (request.spoolmanAddress.length() > 0) {
    // A Spoolman that cannot be reached only leaves the vendor and weight
    // columns empty.
    std::vector<SpoolmanVendor> vendorList;
    if (spoolmanGetVendors(request.spoolmanAddress, vendorList).ok) {
      for (const SpoolmanVendor &vendor : vendorList) vendors[vendor.id] = vendor.name;
    }
    spoolmanGetWeights(request.spoolmanAddress, weights);
  }

  for (size_t i = 0; i < 4; i++) {
    const QidiSlot &slot = read.slots[i];
    BoxSlotView &view = out.slots[i];
    view.loaded = slot.loaded;
    view.active = slot.active;
    view.material = slot.material;
    view.qidiVendor = slot.vendor.equalsIgnoreCase("generic") ? String("Gen.") : slot.vendor;
    view.colorHex = slot.colorHex;
    view.spoolNumber = slot.spoolNumber;
    if (slot.internalVendorId > 0 && vendors.count(slot.internalVendorId)) {
      view.spoolmanVendor = vendors[slot.internalVendorId];
    }
    if (slot.spoolNumber > 0 && weights.count(slot.spoolNumber)) {
      view.weightGrams = weights[slot.spoolNumber];
    }
  }
}

static void workerTask(void *) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(100));

    Request request;
    bool got = false;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (hasPending) {
      request = pending;
      hasPending = false;
      running = true;
      got = true;
    }
    xSemaphoreGive(lock);
    if (!got) continue;

    BoxResult result;
    result.generation = request.generation;
    result.box = request.box;
    if (WiFi.status() != WL_CONNECTED) {
      result.error = "Not connected to Wi-Fi.";
    } else {
      resolve(request, result);
    }

    xSemaphoreTake(lock, portMAX_DELAY);
    finished = result;
    hasFinished = true;
    running = false;
    xSemaphoreGive(lock);
  }
}

void boxMonitorBegin() {
  lock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(workerTask, "boxmon", 12288, nullptr, 1, nullptr, 0);
}

uint32_t boxMonitorRequest(const String &printerAddress, int box, const String &spoolmanAddress) {
  xSemaphoreTake(lock, portMAX_DELAY);
  latestGeneration++;
  pending.generation = latestGeneration;
  pending.printerAddress = printerAddress;
  pending.box = box;
  pending.spoolmanAddress = spoolmanAddress;
  hasPending = true;
  uint32_t generation = latestGeneration;
  xSemaphoreGive(lock);
  return generation;
}

uint32_t boxMonitorLatestGeneration() { return latestGeneration; }

bool boxMonitorTakeResult(BoxResult &out) {
  bool got = false;
  xSemaphoreTake(lock, portMAX_DELAY);
  if (hasFinished) {
    out = finished;
    hasFinished = false;
    got = true;
  }
  xSemaphoreGive(lock);
  return got;
}

bool boxMonitorBusy() {
  xSemaphoreTake(lock, portMAX_DELAY);
  bool busy = hasPending || running;
  xSemaphoreGive(lock);
  return busy;
}
