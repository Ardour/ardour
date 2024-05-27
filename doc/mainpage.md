# Overview and Source Tree Layout

Welcome to the in-code documentation for Ardour.

The main components of Ardour are as follows:

- A GTK2 front-end, in `gtk2_ardour`.
- libardour, the audio-processing back-end, in `libs/ardour`.



The source tree is laid out as follows:

## Front Ends

* `gtk2_ardour/`</dt>

  The main Ardour GUI -- This is where most of the complexity is.
  It is cleanly separated from the backend and processing engine.

* `headless/`

  hardour -- headless Ardour, mostly demo-code how to use Ardour without a GUI.

* `session_utils/`

  command-line tools using libardour (e.g. export)

* `luasession/`

  arlua -- fully fledged commandline interface to libardour

## Libraries

A collection of libraries and utility functions. Most are shared
libraries, and almost all are exclusive to Ardour.
A few specific libraries are compiled statically (e.g. fluidsynth for use in plugins).

### Ardour specific libs

* `libs/pbd/`

   Generic utility classes. This is used the basis for all Ardour specific libraries.
   It provides basic concepts and OS abstractions.

   *The name comes from "Paul Barton-Davis", Paul's full name at the time he started working on working on audio software.*

* `libs/evoral/`

   Evoral is Ardour's event Library, used for control events, control lists, automation evaluation,
   parameter interpolation, parameter descriptions, incl. MIDI event abstraction.

   - `libs/evoral/libsmf/` (contains several non-upstreamed fixes)

     Handling Standard %MIDI File (Evoral::SMF) format. Abstracted to C++ in SMF.{cc,h}

* libs/backends/

   Interaction with Operating System's Audio/MIDI API:
   ALSA, CoreAudio, JACK, PortAudio/ASIO, PulseAudio

   see ARDOUR::AudioBackend

* `libs/surfaces/`

   Control Surfaces, dynamically loaded by libardour on runtime,
   to remote-control ardour (midi bindings, network etc).

   see ARDOUR::ControlProtocol

* `libs/midi++2/`

   %MIDI parsing, MIDNAM handling, Port abstraction for I/O

* `libs/temporal`

   Various utility code for dealing with different kinds of time,
   including Timecode, and musical time conversions (Temporal::Beats, Temporal::BBT_Time).

   This library also provides the fundamental time types, and the *TempoMap*.
   See <https://ardour.org/representing-time.html> for more information.

* `libs/panners/`

   Pan plugins (stereo-balance, VBAP, etc) are dynamically loaded at runtime.

   see ARDOUR::Panner

* `libs/audiographer/`

   Mini Ardour inside Ardour to export audio-files from sessions.

   It is a combination of AudioGrapher::Source and AudioGrapher::Sink classes that are chained together by ARDOUR::ExportGraphBuilder as shown in the ASCII art
   [Export Graph](https://git.ardour.org/ardour/ardour/src/commit/0df0e14e2309a00d433827fa34b87638b87f4fff/libs/ardour/export_graph_builder.cc#L73-L154).

* `libs/ardour/`

   This is it. libardour runs Ardour sessions.

   All realtime processing happens here, plugins are managed etc.

   Some starting points are ARDOUR::Route ARDOUR::Session ARDOUR::Processor

### UI related libs

* libs/gtkmm2ext/

   Utility Library to extend GDK, GTK, and basic abstraction for UIs
   and event-loops. This library is not limited to the GUI, but also
   used for other graphical interfaces (e.g. Push2, NI Maschine control
   surfaces).

* `libs/canvas/`

  Cairo Canvas, provides a slate for scalable drawing and basic layout/packing
  This is used by Ardour's main editor.

  See ArdourCanvas::GtkCanvas and ArdourCanvas::Item

* `libs/widgets/`

   Ardour GUI widgets (buttons, fader, knobs, etc). They are basically all CairoWidget%s

* `libs/waveview/`

   Threaded waveform rendering and waveform image cache.

* libs/tk/

  A localized version of GTK+2, renamed as YTK. See Gtk namespace for relevant documentation.

  - `libs/tk/ydk` `libs/tk/ytk`

    gdk, gtk based on upstream gtk+ 2.24.23

  - `libs/tk/ydk-pixbuf`

    stripped down version of gdk-pixbuf 2.31.1

  - `libs/tk/ydkmm` , `libs/tk/ytkmm`

    gdkmm, gtkmm based on upstream gtkmm 2.45.3

  - `libs/tk/ztk`

    atk 2.14.0

  - `libs/tk/ztkmm`

    atkmm 2.22.7

  - `libs/tk/suil`

     A local copy of <https://github.com/lv2/suil/> (based on 0.10.8).
     Since GTK2 is in our source tree, we also need to provide various plugin UI wrappers
     (`x11_in_gtk2`, `win_in_gtk2`, `suil_cocoa_in_gtk2`).

### Plugin Scan Tools

By default plugins are scanned by a dedicated external process. If that crashes the
main application is not affected, and the plugin that causes the scanner to crash can be
blacklisted.

For practical and historical reasons the actual scanner code lives inside libardour.

* `libs/auscan/`

   Apple Audio Unit Plugin Scan commandline tool.

* `libs/fst/`

   VST2/3 plugin scan commandline tools.

* `libs/vfork/`

   A exec-wrapper which redirects file-descriptors to be used with vfork(2).
   It is used to launch external applications, without impacting real-time
   constraints of the calling process.


### Ardour Community Effect (ACE) Plugins

* `libs/plugins/`

   A *bread and butter* set of LV2 Plugins included with Ardour.

   Many of them are custom version of existing plugins (zamaudio, x42),
   that have been customized to be bundled with Ardour on all platforms
   that ardour runs on.


### Independent, standalone libs

These are 3rd party libs that have been copied into Ardour's source-tree.

* `libs/aaf/`

   Unmodified <https://github.com/agfline/LibAAF> for importing AAF sessions.

   Use `tools/update_libaaf.sh` to update from upstream.

* `libs/appleutility/`

   Utility classes, abstraction for CoreAudio and AudioUnits (OSX, macOS)

* `libs/ardouralsautil/`

   Utility class for device-listing (used by the JACK and ALSA backends).
   Device-reservation commandline tool (linked against libdbus), which is
   also available from <https://github.com/x42/alsa_request_device>

* `libs/clearlooks-newer/`

   GTK theme engine (used by `gtk2_ardour`)

* `libs/fluidsynth/`

   Stripped down (library only) and slightly customized version of fluidsynth.

   Use `tools/update_fluidsynth.sh` to update from upstream.

* `libs/hidapi/`

   Unmodified <https://github.com/signal11/hidapi> for interaction with some
   control surfaces (Push2, NI Maschine)

* `libs/libltc/`

   Unmodified <https://github.com/x42/libltc/> for Linear Timecode en/decoding. see LTCFrame.

* `libs/lua/`

   Lua Script interpreter and C++ class abstraction
   - libs/lua/lua-5.3.5 is unmodified upstream Lua-5.3.5
   - libs/lua/LuaBridge is a highly customized version of
     <https://github.com/vinniefalco/LuaBridge> (C++ bindings)

* `libs/ptformat/`

   Unmodified <https://github.com/zamaudio/ptformat> for loading ProTools sessions.

* `libs/qm-dsp/`

   Stripped down version of <https://github.com/c4dm/qm-dsp>
   The Queen Mary DSP library is used by VAMP Plugins.

* `vamp-plugins`

   VAMP plugins for audio analysis and offline processing (uses qm-dsp)

* `libs/vamp-pyin/`

   VAMP plugins for pitch and note-tracking (uses qm-dsp), offline analysis

* `libs/vst3/`

   Stripped down version of Steinberg's VST3 SDK
   <https://github.com/steinbergmedia/vst3sdk/>
   use `tools/update_vst3.sh` to update from upstream

* `libs/zita-convolver/`

   A convolution kernel, so far only available to Lua scripts.

* `libs/zita-resampler/`

   Efficient resampler with variable rate, useful for adaptive resampling.
   Mainly used for vari-speed playback. This has been customized for multiple mono
   channel processing (ArdourZita::VMResampler), and optimized to skip processing for a ratio of 1:1.


## Resource Files

These are platform independent files, and bundled as-is.

* `share/export/`

  Export Format and Presets (see <https://manual.ardour.org/exporting/edit-export-format-profile/>)

* `share/mcp/`

  Mackie control surface device files (see <https://manual.ardour.org/using-control-surfaces/devices-using-mackielogic-control-protocol/>)

* `share/midi_maps/`

  Generic %MIDI control surface presets

* `share/osc/`

  [TouchOSC](https://hexler.net/touchosc) layouts for use with Ardour's OSC control surface.

* `share/patchfiles/`

  MIDNAM files, %MIDI synth descriptions (note-names, CC, PGM names)

* `share/scripts/`

  Lua scripts (files with a leading underscore are not bundled).
  see `share/scripts/README` for file name convention.

* `share/templates/`

  Session templates (currently none)

## Miscellaneous

* `doc/`

  Misc developer oriented documentation files and Doxygen

* `patches/`

  Some .diff files for the build-stack.

* `tools/`

  Various developer tools, most notably packaging scripts
