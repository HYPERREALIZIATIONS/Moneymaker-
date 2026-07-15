#include "backend.h"

// HashrateTracker is fully inline in backend.h; this TU exists so the core
// library has a translation unit and a stable place to add future shared
// backend helpers (e.g. nonce patching, difficulty conversion).
namespace mm::core {
} // namespace mm::core
