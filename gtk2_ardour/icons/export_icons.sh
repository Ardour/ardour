#! /bin/bash

# Export one PNG for every icon-size in app-icon_tango.svg. The "targets" layer in the SVG should be hidden.

for s in \
  "ardour-app-icon_tango_256px" \
  "ardour-app-icon_tango_048px" \
  "ardour-app-icon_tango_032px" \
  "ardour-app-icon_tango_022px" \
  "ardour-app-icon_tango_016px" \
  "ardour-app-icon_osx" \
  "ardour-app-icon_osx_mask";
  do inkscape --export-id "$s" --export-png "$s.png" app-icon_tango.svg;
done
