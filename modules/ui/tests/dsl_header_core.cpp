#include <ui/DSLCore.h>

// This translation unit deliberately includes only the stable core header. Its purpose is to catch
// missing includes and accidental dependencies on the DSL umbrella or experimental forwarders.
bool dsl_core_header_is_self_contained() { return coral::ui::dsl::kApiVersion >= 1; }
