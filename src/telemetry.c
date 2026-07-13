#include "telemetry.h"

void telemetry_record_message(struct telemetry_state *st, size_t bytes) {
    (void)bytes;                 /* TODO: parse/validate payload here */
    if (st) st->messages_total++;
}
