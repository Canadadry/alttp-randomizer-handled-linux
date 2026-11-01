#include "rando.h"
#include "rando_config.h"
#include "rando_filler.h"
#include "rando_items.h"

/* measure.sh finds each size by symbol name (nm -S): a renamed or added probe needs the same change
 * to its probes list. */
unsigned char rando_probe_RandoGen[sizeof(RandoGen)];
unsigned char rando_probe_RandoOptions[sizeof(RandoOptions)];
unsigned char rando_probe_RandoPrng[sizeof(RandoPrng)];
unsigned char rando_probe_RandoPools[sizeof(RandoPools)];
unsigned char rando_probe_RandoWorld[sizeof(RandoWorld)];
unsigned char rando_probe_RandoItems[sizeof(RandoItems)];
unsigned char rando_probe_RandoConfig[sizeof(RandoConfig)];
unsigned char rando_probe_RandoFillState[sizeof(RandoFillState)];
