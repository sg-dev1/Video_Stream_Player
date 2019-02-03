# Debugging

## Memory allocation
sudo /opt/vc/bin/vcdbg reloc

## Logging

### View log messages from videocore

- see "Raspberry Pi GPU Audio Video Programming" book p. 261

sudo /opt/vc/bin/vcgencmd cache_flush
sudo /opt/vc/bin/vcgencmd set_logging level=0x40
sudo /opt/vc/bin/vcdbg log msg

-  In code

´´´
#include <interface/mmal/mmal_logging.h>
#include <interface/vcos/vcos_logging.h>
[...]
vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE);
´´´

- vcdbg commands

$ sudo /opt/vc/bin/vcdbg help log
Usage: /opt/vc/bin/vcdbg log command [args ...]

Where command is one of the following:
  ex                   - Prints out the exception log
  assert               - Prints out the assertion log
  info                 - Prints information from the logging headers
  msg                  - Prints out the message log
  level  CAT n|e|w|i|t - Sets the VCOS logging level for the specified category (ELF symbol)
  list                 - List the VCOS logging levels

### MMAL related logging

e.g. sudo /opt/vc/bin/mmal_vc_diag

$ /opt/vc/bin/mmal_vc_diag  commands
help                 give this help
version              report VC MMAL server version number
stats                report VC MMAL statistics
reset                reset VC MMAL statistics
commands             list available commands
create               create a component
eventlog             display event log
components           [update] list components
mmal-stats           list mmal core stats
ic-stats             [reset] list imageconv stats
compact              trigger memory compaction
autosusp             test out auto-suspend/resume
camerainfo           get camera info
host_log             dumps the MMAL VC log
host_log_write       appends a message to the MMAL VC log of host messages
