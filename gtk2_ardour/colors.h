/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/* no guard #ifdef's - this should only be included in ui_config.{cc,h}, where it is
 * used with some preprocessor tricks.
 */

CANVAS_COLOR(ActiveCrossfade,"active crossfade", "colorD", HSV(0,-0.179775,-0.301961,0.180392)) /*0 */
CANVAS_COLOR(canvasvar_ArrangeBase,"arrange base", "colorLightGray", HSV(0,0,-0.14902,1)) /*53.6471 */
CANVAS_COLOR(AudioBusBase,"audio bus base", "colorDdark", HSV(-18,-0.751634,-0.4,0.407843)) /*17.5666 */
CANVAS_COLOR(AudioTrackBase,"audio track base", "colorDlight", HSV(18,-0.80102,-0.231373,0.407843)) /*17.5023 */
CANVAS_COLOR(AudioMasterBusBase,"audio master bus base", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(AutomationLine,"automation line", "colorC", HSV(-0,-0.361702,-0.262745,1)) /*0.117647 */
CANVAS_COLOR(AutomationTrackFill,"automation track fill", "colorDdark", HSV(-0,-0.776699,-0.192157,0.407843)) /*0.404092 */
CANVAS_COLOR(AutomationTrackOutline,"automation track outline", "colorMidGray", HSV(0,0,0.0235294,1)) /*8.47059 */
CANVAS_COLOR(CDMarkerBar,"cd marker bar", "colorDdark", HSV(-0,-0.907975,-0.360784,0.8)) /*1.88235 */
CANVAS_COLOR(CrossfadeEditorBase,"crossfade editor base", "colorDdark", HSV(0,-0.547945,-0.713725,1)) /*0.663102 */
CANVAS_COLOR(CrossfadeEditorLine,"crossfade editor line", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(CrossfadeEditorLineShading,"crossfade editor line shading", "colorDlight", HSV(0,0,-0.180392,0.329412)) /*0.203771 */
CANVAS_COLOR(CrossfadeEditorPointFill,"crossfade editor point fill", "colorC", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(CrossfadeEditorPointOutline,"crossfade editor point outline", "colorDdark", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(CrossfadeEditorWave,"crossfade editor wave", "colorLightest", HSV(0,0,0,0.156863)) /*0 */
CANVAS_COLOR(SelectedCrossfadeEditorWaveFill,"selected crossfade editor wave fill", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(CrossfadeLine,"crossfade line", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(EditPoint,"edit point", "colorDdark", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(EnteredAutomationLine,"entered automation line", "colorA", HSV(0,-0.447964,-0.133333,1)) /*0 */
CANVAS_COLOR(ControlPointFill,"control point fill", "colorLightest", HSV(0,0,0,0.4)) /*0 */
CANVAS_COLOR(ControlPointOutline,"control point outline", "colorA", HSV(0,0,0,0.933333)) /*0 */
CANVAS_COLOR(ControlPointSelected,"control point selected", "colorD", HSV(0,-0.416667,-0.2,1)) /*0 */
CANVAS_COLOR(EnteredGainLine,"entered gain line", "colorA", HSV(0,-0.447964,-0.133333,1)) /*0 */
CANVAS_COLOR(EnteredMarker,"entered marker", "colorA", HSV(0,-0.447964,-0.133333,1)) /*0 */
CANVAS_COLOR(FrameHandle,"frame handle", "colorDA", HSV(0,0,0,0.588235)) /*0 */
CANVAS_COLOR(GainLine,"gain line", "colorC", HSV(-0,0,-0.262745,1)) /*0.0538173 */
CANVAS_COLOR(GainLineInactive,"gain line inactive", "colorC", HSV(0,-0.845745,-0.262745,0.772549)) /*0.0892495 */
CANVAS_COLOR(GhostTrackBase,"ghost track base", "colorDA", HSV(-0,-0.5,-0.513725,0.776471)) /*0.117647 */
CANVAS_COLOR(GhostTrackMidiOutline,"ghost track midi outline", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(GhostTrackWave,"ghost track wave", "colorMidGray", HSV(0,0,-0.00784314,0.85098)) /*2.82353 */
CANVAS_COLOR(GhostTrackWaveFill,"ghost track wave fill", "colorMidGray", HSV(0,0,-0.00784314,0.376471)) /*2.82353 */
CANVAS_COLOR(GhostTrackWaveClip,"ghost track wave clip", "colorMidGray", HSV(0,0,-0.00784314,0.85098)) /*2.82353 */
CANVAS_COLOR(GhostTrackZeroLine,"ghost track zero line", "colorAlight", HSV(0,0,-0.101961,0.4)) /*0.143848 */
CANVAS_COLOR(ImageTrack,"image track", "colorB", HSV(-0,-0.977376,-0.133333,1)) /*6.11765 */
CANVAS_COLOR(InactiveCrossfade,"inactive crossfade", "colorB", HSV(-0,-0.257384,-0.0705882,0.466667)) /*0.254011 */
CANVAS_COLOR(InactiveFadeHandle,"inactive fade handle", "colorLightGray", HSV(0,0,0.235294,0.666667)) /*84.7059 */
CANVAS_COLOR(InactiveGroupTab,"inactive group tab", "colorMidGray", HSV(0,0,0.129412,1)) /*46.5882 */
CANVAS_COLOR(LocationCDMarker,"location cd marker", "colorCD", HSV(-0,-0.12931,-0.0901961,1)) /*0.236459 */
CANVAS_COLOR(LocationLoop,"location loop", "colorCD", HSV(-18,-0.353333,-0.411765,1)) /*17.9939 */
CANVAS_COLOR(LocationMarker,"location marker", "colorB", HSV(18,-0.0696721,-0.0431373,1)) /*17.7766 */
CANVAS_COLOR(LocationPunch,"location punch", "colorA", HSV(0,-0.467742,-0.513725,1)) /*0 */
CANVAS_COLOR(LocationRange,"location range", "colorCD", HSV(-18,-0.598361,-0.521569,1)) /*17.6279 */
CANVAS_COLOR(MarkerBar,"marker bar", "colorDdark", HSV(-18,-0.884393,-0.321569,0.8)) /*17.8824 */
CANVAS_COLOR(MarkerBarSeparator,"marker bar separator", "colorLightGray", HSV(0,0,-0.164706,1)) /*59.2941 */
CANVAS_COLOR(MarkerDragLine,"marker drag line", "colorC", HSV(-0,0,-0.690196,0.976471)) /*0.0416977 */
CANVAS_COLOR(MarkerLabel,"marker label", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(MarkerTrack,"marker track", "colorB", HSV(-0,-0.977376,-0.133333,1)) /*6.11765 */
CANVAS_COLOR(MeasureLineBar,"measure line bar", "colorLightest", HSV(0,0,0,0.611765)) /*0 */
CANVAS_COLOR(MeasureLineBeat,"measure line beat", "colorA", HSV(0,-0.975309,-0.364706,0.462745)) /*0 */
CANVAS_COLOR(MeterBar,"meter bar", "colorDdark", HSV(0,-0.875,-0.560784,0.8)) /*1.83193 */
CANVAS_COLOR(MeterBridgePeakLabel,"meterbridge peaklabel", "colorA", HSV(0,-0.0666667,0,1)) /*0 */
CANVAS_COLOR(MeterColorBBC,"meter color BBC", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor0,"meter fill: 0", "colorC", HSV(0,0,-0.466667,1)) /*0.0588235 */
CANVAS_COLOR(MeterColor1,"meter fill: 1", "colorC", HSV(-0,0,-0.333333,1)) /*0.117647 */
CANVAS_COLOR(MeterColor2,"meter fill: 2", "colorC", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor3,"meter fill: 3", "colorC", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor4,"meter fill: 4", "colorB", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor5,"meter fill: 5", "colorB", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor6,"meter fill: 6", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor7,"meter fill: 7", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor8,"meter fill: 8", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterColor9,"meter fill: 9", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MidiMeterColor0,"midi meter fill: 0", "colorClight", HSV(-18,-0.644,-0.0196078,1)) /*17.7475 */
CANVAS_COLOR(MidiMeterColor1,"midi meter fill: 1", "colorABlight", HSV(-0,-0.516529,-0.0509804,1)) /*0.102564 */
CANVAS_COLOR(MidiMeterColor2,"midi meter fill: 2", "colorABlight", HSV(-0,-0.516529,-0.0509804,1)) /*0.102564 */
CANVAS_COLOR(MidiMeterColor3,"midi meter fill: 3", "colorAB", HSV(0,-0.336066,-0.0431373,1)) /*0.0305011 */
CANVAS_COLOR(MidiMeterColor4,"midi meter fill: 4", "colorAB", HSV(0,-0.336066,-0.0431373,1)) /*0.0305011 */
CANVAS_COLOR(MidiMeterColor5,"midi meter fill: 5", "colorAB", HSV(-0,-0.0766129,-0.027451,1)) /*0.0390444 */
CANVAS_COLOR(MidiMeterColor6,"midi meter fill: 6", "colorAB", HSV(-0,-0.0766129,-0.027451,1)) /*0.0390444 */
CANVAS_COLOR(MidiMeterColor7,"midi meter fill: 7", "colorC", HSV(0,-0.713568,-0.219608,1)) /*0.198142 */
CANVAS_COLOR(MidiMeterColor8,"midi meter fill: 8", "colorC", HSV(0,-0.713568,-0.219608,1)) /*0.198142 */
CANVAS_COLOR(MidiMeterColor9,"midi meter fill: 9", "colorC", HSV(18,0,-0.0431373,0)) /*17.9807 */
CANVAS_COLOR(MeterBackgroundBot,"meter background: bottom", "colorMidGray", HSV(0,0,0.0666667,1)) /*24 */
CANVAS_COLOR(MeterBackgroundTop,"meter background: top", "colorMidGray", HSV(0,0,0.133333,1)) /*48 */
CANVAS_COLOR(MeterMarker,"meter marker", "colorA", HSV(0,-0.272727,-0.0509804,1)) /*0 */
CANVAS_COLOR(MidiBusBase,"midi bus base", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MidiFrameBase,"midi frame base", "colorC", HSV(-18,-0.901639,-0.760784,0.4)) /*16.1176 */
CANVAS_COLOR(MidiNoteInactiveChannel,"midi note inactive channel", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MidiNoteColorBase,"midi note color min", "colorClight", HSV(0,-0.5,-0.670588,1)) /*0.117647 */
CANVAS_COLOR(MidiNoteColorMid,"midi note color mid", "colorClight", HSV(0,-0.5,-0.341176,1)) /*0.117647 */
CANVAS_COLOR(MidiNoteColorTop,"midi note color max", "colorClight", HSV(-0,-0.501961,0,1)) /*0.118573 */
CANVAS_COLOR(SelectedMidiNoteColorBase,"selected midi note color min", "colorDdark", HSV(0,-0.588235,-0.8,1)) /*0.403361 */
CANVAS_COLOR(SelectedMidiNoteColorMid,"selected midi note color mid", "colorDdark", HSV(-0,-0.586957,-0.458824,1)) /*0.198142 */
CANVAS_COLOR(SelectedMidiNoteColorTop,"selected midi note color max", "colorDdark", HSV(0,-0.59009,-0.129412,1)) /*0.183581 */
CANVAS_COLOR(MidiNoteSelected,"midi note selected", "colorDdark", HSV(-0,-0.698039,0,1)) /*0.116119 */
CANVAS_COLOR(MidiNoteVelocityText,"midi note velocity text", "colorB", HSV(-0,-0.0819672,-0.0431373,0.737255)) /*0.0105042 */
CANVAS_COLOR(MidiPatchChangeFill,"midi patch change fill", "colorDdark", HSV(-18,-0.888889,-0.647059,0.627451)) /*17.8824 */
CANVAS_COLOR(MidiPatchChangeOutline,"midi patch change outline", "colorDdark", HSV(-18,-0.950495,-0.207843,1)) /*17.8824 */
CANVAS_COLOR(MidiPatchChangeInactiveChannelFill,"midi patch change inactive channel fill", "colorDdark", HSV(-18,-0.888889,-0.647059,0.752941)) /*17.8824 */
CANVAS_COLOR(MidiPatchChangeInactiveChannelOutline,"midi patch change inactive channel outline", "colorDdark", HSV(-18,-0.761905,-0.835294,0.752941)) /*17.8824 */
CANVAS_COLOR(MidiSysExFill,"midi sysex fill", "colorB", HSV(0,-0.236515,-0.054902,0.627451)) /*0.0127877 */
CANVAS_COLOR(MidiSysExOutline,"midi sysex outline", "colorDdark", HSV(-0,-0.787736,-0.168627,1)) /*0.54902 */
CANVAS_COLOR(MidiSelectRectFill,"midi select rect fill", "colorDdark", HSV(0,-0.533333,0,0.533333)) /*0.0672269 */
CANVAS_COLOR(MidiSelectRectOutline,"midi select rect outline", "colorDdark", HSV(0,-0.333333,0,1)) /*0.117647 */
CANVAS_COLOR(MidiTrackBase,"midi track base", "colorClight", HSV(-0,-0.79902,-0.2,0.372549)) /*0.61406 */
CANVAS_COLOR(NameHighlightFill,"name highlight fill", "colorDdark", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(NameHighlightOutline,"name highlight outline", "colorDA", HSV(0,0,0,0.588235)) /*0 */
CANVAS_COLOR(PianoRollBlackOutline,"piano roll black outline", "colorLightest", HSV(0,0,-0.0431373,0.462745)) /*15.5294 */
CANVAS_COLOR(PianoRollBlack,"piano roll black", "colorClight", HSV(0,-0.963636,-0.568627,0.419608)) /*0.117647 */
CANVAS_COLOR(PianoRollWhite,"piano roll white", "colorC", HSV(-18,-0.96129,-0.392157,0.396078)) /*16.1176 */
CANVAS_COLOR(PlayHead,"play head", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(ProcessorAutomationLine,"processor automation line", "colorDdark", HSV(-18,-0.48996,-0.0235294,1)) /*17.9768 */
CANVAS_COLOR(PunchLine,"punch line", "colorA", HSV(0,0,-0.341176,1)) /*0 */
CANVAS_COLOR(RangeDragBarRect,"range drag bar rect", "colorLightGray", HSV(0,0,0.0901961,0.776471)) /*32.4706 */
CANVAS_COLOR(RangeDragRect,"range drag rect", "colorC", HSV(18,-0.656566,-0.223529,0.776471)) /*17.7059 */
CANVAS_COLOR(RangeMarkerBar,"range marker bar", "colorDdark", HSV(0,-0.892857,-0.45098,0.8)) /*2.11765 */
CANVAS_COLOR(RecordingRect,"recording rect", "colorA", HSV(0,-0.196078,-0.2,1)) /*0 */
CANVAS_COLOR(RecWaveFormFill,"recorded waveform fill", "colorLightest", HSV(0,0,0,0.85098)) /*0 */
CANVAS_COLOR(RecWaveForm,"recorded waveform outline", "colorDdark", HSV(-0,-0.483871,-0.878431,1)) /*1.38235 */
CANVAS_COLOR(RubberBandRect,"rubber band rect", "colorLightest", HSV(0,0,-0.223529,0.34902)) /*80.4706 */
CANVAS_COLOR(RulerBase,"ruler base", "colorA", HSV(0,-0.75,-0.827451,1)) /*0 */
CANVAS_COLOR(RulerText,"ruler text", "colorLightest", HSV(0,0,-0.101961,1)) /*36.7059 */
CANVAS_COLOR(SelectedCrossfadeEditorLine,"selected crossfade editor line", "colorD", HSV(0,0,-0.141176,1)) /*0 */
CANVAS_COLOR(SelectedCrossfadeEditorWave,"selected crossfade editor wave", "colorB", HSV(-0,-0.0803213,-0.0235294,0.627451)) /*0.143848 */
CANVAS_COLOR(SelectedFrameBase,"selected region base", "colorDlight", HSV(18,-0.907216,-0.619608,1)) /*15.451 */
CANVAS_COLOR(SelectedWaveFormFill,"selected waveform fill", "colorABlight", HSV(0,0,0,0.85098)) /*0 */
CANVAS_COLOR(SelectedWaveForm,"selected waveform outline", "colorDarkGray", HSV(0,0,-0.0313725,0.8)) /*11.2941 */
CANVAS_COLOR(SelectionRect,"selection rect", "colorClight", HSV(0,-0.864754,-0.0431373,0.466667)) /*1.02674 */
CANVAS_COLOR(Selection,"selection", "colorLightGray", HSV(0,0,-0.109804,0.698039)) /*39.5294 */
CANVAS_COLOR(Shuttle,"shuttle", "colorClight", HSV(0,-0.175824,-0.286275,1)) /*0.117647 */
CANVAS_COLOR(Silence,"silence", "colorD", HSV(0,-0.619608,0,0.478431)) /*0 */
CANVAS_COLOR(SilenceText,"silence text", "colorDdark", HSV(18,-0.0555556,-0.576471,1)) /*17.8824 */
CANVAS_COLOR(MonoPannerOutline,"mono panner outline", "colorDdark", HSV(-18,-0.542553,-0.631373,1)) /*17.6033 */
CANVAS_COLOR(MonoPannerFill,"mono panner fill", "colorDlight", HSV(18,-0.598039,-0.2,0.788235)) /*17.9713 */
CANVAS_COLOR(MonoPannerText,"mono panner text", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(MonoPannerBackground,"mono panner bg", "colorA", HSV(0,-0.891304,-0.819608,1)) /*0 */
CANVAS_COLOR(MonoPannerPositionFill,"mono panner position fill", "colorDlight", HSV(18,-0.681564,-0.298039,1)) /*17.9071 */
CANVAS_COLOR(MonoPannerPositionOutline,"mono panner position outline", "colorDdark", HSV(-18,-0.542553,-0.631373,1)) /*17.6033 */
CANVAS_COLOR(StereoPannerOutline,"stereo panner outline", "colorDdark", HSV(-18,-0.542553,-0.631373,1)) /*17.6033 */
CANVAS_COLOR(StereoPannerFill,"stereo panner fill", "colorDlight", HSV(18,-0.598039,-0.2,0.788235)) /*17.9713 */
CANVAS_COLOR(StereoPannerText,"stereo panner text", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(StereoPannerBackground,"stereo panner bg", "colorA", HSV(0,-0.891304,-0.819608,1)) /*0 */
CANVAS_COLOR(StereoPannerRule,"stereo panner rule", "colorDdark", HSV(-18,-0.543307,-0.501961,1)) /*17.6755 */
CANVAS_COLOR(StereoPannerMonoOutline,"stereo panner mono outline", "colorABlight", HSV(0,0,-0.372549,1)) /*0 */
CANVAS_COLOR(StereoPannerMonoFill,"stereo panner mono fill", "colorAB", HSV(0,-0.446352,-0.0862745,0.792157)) /*0.0218878 */
CANVAS_COLOR(StereoPannerMonoText,"stereo panner mono text", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(StereoPannerMonoBackground,"stereo panner mono bg", "colorA", HSV(0,-0.891304,-0.819608,1)) /*0 */
CANVAS_COLOR(StereoPannerInvertedOutline,"stereo panner inverted outline", "colorA", HSV(0,0,-0.25098,1)) /*0 */
CANVAS_COLOR(StereoPannerInvertedFill,"stereo panner inverted fill", "colorA", HSV(0,-0.684211,-0.105882,0.788235)) /*0 */
CANVAS_COLOR(StereoPannerInvertedText,"stereo panner inverted text", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(StereoPannerInvertedBackground,"stereo panner inverted bg", "colorA", HSV(0,-0.891304,-0.819608,1)) /*0 */
CANVAS_COLOR(TempoBar,"tempo bar", "colorDdark", HSV(-0,-0.88189,-0.501961,0.8)) /*1.88235 */
CANVAS_COLOR(TempoMarker,"tempo marker", "colorA", HSV(0,-0.272727,-0.0509804,1)) /*0 */
CANVAS_COLOR(TimeAxisFrame,"time axis frame", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(SelectedTimeAxisFrame,"selected time axis frame", "colorA", HSV(0,0,-0.0666667,1)) /*0 */
CANVAS_COLOR(TimeStretchFill,"time stretch fill", "colorA", HSV(0,-0.800885,-0.113725,0.588235)) /*0 */
CANVAS_COLOR(TimeStretchOutline,"time stretch outline", "colorLightGray", HSV(0,0,-0.109804,0.588235)) /*39.5294 */
CANVAS_COLOR(MonitorKnobArcStart,"monitor knob: arc start", "colorDlight", HSV(0,-0.528409,-0.309804,1)) /*0.189936 */
CANVAS_COLOR(MonitorKnobArcEnd,"monitor knob: arc end", "colorDlight", HSV(0,-0.190909,-0.568627,1)) /*0.319894 */
CANVAS_COLOR(TransportDragRect,"transport drag rect", "colorLightGray", HSV(0,0,0.0901961,0.776471)) /*32.4706 */
CANVAS_COLOR(TransportLoopRect,"transport loop rect", "colorC", HSV(-0,-0.252101,-0.533333,0.976471)) /*0.0502313 */
CANVAS_COLOR(TransportMarkerBar,"transport marker bar", "colorDdark", HSV(0,-0.921053,-0.403922,0.8)) /*1.11765 */
CANVAS_COLOR(TransportPunchRect,"transport punch rect", "colorA", HSV(0,-0.366972,-0.572549,0.898039)) /*0 */
CANVAS_COLOR(TrimHandleLocked,"trim handle locked", "colorA", HSV(0,-0.0641026,-0.0823529,0.156863)) /*0 */
CANVAS_COLOR(TrimHandle,"trim handle", "colorDA", HSV(-18,0,0,0.266667)) /*18.1176 */
CANVAS_COLOR(VerboseCanvasCursor,"verbose canvas cursor", "colorB", HSV(-0,-0.180392,0,0.737255)) /*0.146355 */
CANVAS_COLOR(VestigialFrame,"vestigial frame", "colorDarkGray", HSV(0,0,-0.0901961,0.0588235)) /*32.4706 */
CANVAS_COLOR(VideoBar,"video timeline bar", "colorMidGray", HSV(0,0,0.054902,1)) /*19.7647 */
CANVAS_COLOR(FrameBase,"region base", "colorDdark", HSV(-18,-0.909722,-0.435294,1)) /*16.9593 */
CANVAS_COLOR(CoveredRegion,"region area covered by another region", "colorMidGray", HSV(0,0,0.180392,0.690196)) /*64.9412 */
CANVAS_COLOR(WaveForm,"waveform outline", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(WaveFormClip,"clipped waveform", "colorA", HSV(0,0,0,0.898039)) /*0 */
CANVAS_COLOR(WaveFormFill,"waveform fill", "colorLightest", HSV(0,0,0,0.85098)) /*0 */
CANVAS_COLOR(ZeroLine,"zero line", "colorLightGray", HSV(0,0,0,0.878431)) /*0 */
CANVAS_COLOR(MonitorSectionKnob,"monitor knob", "colorA", HSV(0,-0.941176,-0.666667,1)) /*0 */
CANVAS_COLOR(ButtonBorder,"border color", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(ProcessorPreFaderFill,"processor prefader: fill", "colorA", HSV(0,-0.444444,-0.470588,1)) /*0 */
CANVAS_COLOR(ProcessorPreFaderFillActive,"processor prefader: fill active", "colorA", HSV(0,-0.552083,-0.623529,1)) /*0 */
CANVAS_COLOR(ProcessorPreFaderLED,"processor prefader: led", "colorC", HSV(-18,-0.164706,-0.666667,1)) /*17.9486 */
CANVAS_COLOR(ProcessorPreFaderLEDActive,"processor prefader: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(ProcessorPreFaderText,"processor prefader: text", "colorB", HSV(-0,-0.958824,-0.333333,1)) /*2.68908 */
CANVAS_COLOR(ProcessorPreFaderTextActive,"processor prefader: text active", "colorB", HSV(0,-0.991597,-0.0666667,1)) /*5.88235 */
CANVAS_COLOR(ProcessorFaderFill,"processor fader: fill", "colorDlight", HSV(0,-0.528409,-0.309804,1)) /*0.189936 */
CANVAS_COLOR(ProcessorFaderFillActive,"processor fader: fill active", "colorDlight", HSV(0,-0.258741,-0.439216,1)) /*0.230855 */
CANVAS_COLOR(ProcessorFaderLED,"processor fader: led", "colorC", HSV(-18,-0.164706,-0.666667,1)) /*17.9486 */
CANVAS_COLOR(ProcessorFaderLEDActive,"processor fader: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(ProcessorFaderText,"processor fader: text", "colorB", HSV(-0,-0.958824,-0.333333,1)) /*2.68908 */
CANVAS_COLOR(ProcessorFaderTextActive,"processor fader: text active", "colorB", HSV(0,-0.991597,-0.0666667,1)) /*5.88235 */
CANVAS_COLOR(ProcessorPostFaderFill,"processor postfader: fill", "colorClight", HSV(18,-0.666667,-0.647059,1)) /*18.1176 */
CANVAS_COLOR(ProcessorPostFaderFillActive,"processor postfader: fill active", "colorC", HSV(-0,-0.536232,-0.729412,1)) /*0.492647 */
CANVAS_COLOR(ProcessorPostFaderLED,"processor postfader: led", "colorC", HSV(-18,-0.164706,-0.666667,1)) /*17.9486 */
CANVAS_COLOR(ProcessorPostFaderLEDActive,"processor postfader: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(ProcessorPostFaderText,"processor postfader: text", "colorB", HSV(-0,-0.958824,-0.333333,1)) /*2.68908 */
CANVAS_COLOR(ProcessorPostFaderTextActive,"processor postfader: text active", "colorB", HSV(0,-0.991597,-0.0666667,1)) /*5.88235 */
CANVAS_COLOR(ProcessorControlButtonFill,"processor control button: fill", "colorMidGray", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(ProcessorControlButtonFillActive,"processor control button: fill active", "colorMidGray", HSV(0,0,0.0666667,1)) /*24 */
CANVAS_COLOR(ProcessorControlButtonLED,"processor control button: led", "colorDarkGray", HSV(0,0,-0.027451,1)) /*9.88235 */
CANVAS_COLOR(ProcessorControlButtonLEDActive,"processor control button: led active", "colorDlight", HSV(0,-0.528409,-0.309804,1)) /*0.189936 */
CANVAS_COLOR(ProcessorControlButtonText,"processor control button: text", "colorLightest", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(ProcessorControlButtonTextActive,"processor control button: text active", "colorLightest", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorButtonFill,"monitor button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(MonitorButtonFillActive,"monitor button: fill active", "colorABlight", HSV(-0,-0.0253807,-0.227451,1)) /*0.0625 */
CANVAS_COLOR(MonitorButtonLED,"monitor button: led", "colorA", HSV(0,0,-0.6,1)) /*0 */
CANVAS_COLOR(MonitorButtonLEDActive,"monitor button: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorButtonText,"monitor button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorButtonTextActive,"monitor button: text active", "colorDarkGray", HSV(0,0,0.0117647,1)) /*4.23529 */
CANVAS_COLOR(SoloIsolateButtonFill,"solo isolate: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(SoloIsolateButtonFillActive,"solo isolate: fill active", "colorAB", HSV(-0,-0.837209,-0.662745,1)) /*0.97479 */
CANVAS_COLOR(SoloIsolateButtonLED,"solo isolate: led", "colorA", HSV(0,0,-0.6,1)) /*0 */
CANVAS_COLOR(SoloIsolateButtonLEDActive,"solo isolate: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SoloIsolateButtonText,"solo isolate: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(SoloIsolateButtonTextActive,"solo isolate: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(SoloSafeButtonFill,"solo safe: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(SoloSafeButtonFillActive,"solo safe: fill active", "colorAB", HSV(-0,-0.837209,-0.662745,1)) /*0.97479 */
CANVAS_COLOR(SoloSafeButtonLED,"solo safe: led", "colorA", HSV(0,0,-0.6,1)) /*0 */
CANVAS_COLOR(SoloSafeButtonLEDActive,"solo safe: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SoloSafeButtonText,"solo safe: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(SoloSafeButtonTextActive,"solo safe: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MidiDeviceButtonFill,"midi device: fill", "colorDdark", HSV(-0,-0.903226,-0.635294,1)) /*0.54902 */
CANVAS_COLOR(MidiDeviceButtonFillActive,"midi device: fill active", "colorDdark", HSV(-0,-0.907895,-0.701961,1)) /*2.45378 */
CANVAS_COLOR(MidiDeviceButtonLED,"midi device: led", "colorC", HSV(-0,0,-0.6,1)) /*0.235294 */
CANVAS_COLOR(MidiDeviceButtonLEDActive,"midi device: led active", "colorC", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MidiDeviceButtonText,"midi device: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MidiDeviceButtonTextActive,"midi device: text active", "colorB", HSV(0,-0.991597,-0.0666667,1)) /*5.88235 */
CANVAS_COLOR(MeterBridgePeakIndicatorFill,"meterbridge peakindicator: fill", "colorMidGray", HSV(0,0,0.133333,1)) /*48 */
CANVAS_COLOR(MeterBridgePeakIndicatorFillActive,"meterbridge peakindicator: fill active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgePeakIndicatorLED,"meterbridge peakindicator: led", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgePeakIndicatorLEDActive,"meterbridge peakindicator: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgePeakIndicatorText,"meterbridge peakindicator: text", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgePeakIndicatorTextActive,"meterbridge peakindicator: text active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgeLabelFill,"meterbridge label: fill", "colorMidGray", HSV(0,0,0.133333,1)) /*48 */
CANVAS_COLOR(MeterBridgeLabelFillActive,"meterbridge label: fill active", "colorMidGray", HSV(0,0,0.0666667,1)) /*24 */
CANVAS_COLOR(MeterBridgeLabelLED,"meterbridge label: led", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgeLabelLEDActive,"meterbridge label: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MeterBridgeLabelText,"meterbridge label: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MeterBridgeLabelTextActive,"meterbridge label: text active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionCutButtonFill,"monitor section cut: fill", "colorAB", HSV(-0,-0.926316,-0.627451,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionCutButtonFillActive,"monitor section cut: fill active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionCutButtonLED,"monitor section cut: led", "colorABlight", HSV(0,-0.253521,-0.721569,1)) /*0.226415 */
CANVAS_COLOR(MonitorSectionCutButtonLEDActive,"monitor section cut: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(MonitorSectionCutButtonText,"monitor section cut: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionCutButtonTextActive,"monitor section cut: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(MonitorSectionDimButtonFill,"monitor section dim: fill", "colorAB", HSV(-0,-0.926316,-0.627451,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionDimButtonFillActive,"monitor section dim: fill active", "colorABlight", HSV(-0,-0.0218341,-0.101961,1)) /*0.107143 */
CANVAS_COLOR(MonitorSectionDimButtonLED,"monitor section dim: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MonitorSectionDimButtonLEDActive,"monitor section dim: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(MonitorSectionDimButtonText,"monitor section dim: text", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionDimButtonTextActive,"monitor section dim: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloButtonFill,"monitor section solo: fill", "colorAB", HSV(-0,-0.926316,-0.627451,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloButtonFillActive,"monitor section solo: fill active", "colorClight", HSV(-0,0,-0.266667,1)) /*0.0427807 */
CANVAS_COLOR(MonitorSectionSoloButtonLED,"monitor section solo: led", "colorABlight", HSV(0,-0.253521,-0.721569,1)) /*0.226415 */
CANVAS_COLOR(MonitorSectionSoloButtonLEDActive,"monitor section solo: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionSoloButtonText,"monitor section solo: text", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MonitorSectionSoloButtonTextActive,"monitor section solo: text active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MonitorSectionInvertButtonFill,"monitor section invert: fill", "colorAB", HSV(-0,-0.926316,-0.627451,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionInvertButtonFillActive,"monitor section invert: fill active", "colorDdark", HSV(0,-0.317308,-0.184314,1)) /*0.202154 */
CANVAS_COLOR(MonitorSectionInvertButtonLED,"monitor section invert: led", "colorABlight", HSV(0,-0.253521,-0.721569,1)) /*0.226415 */
CANVAS_COLOR(MonitorSectionInvertButtonLEDActive,"monitor section invert: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(MonitorSectionInvertButtonText,"monitor section invert: text", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MonitorSectionInvertButtonTextActive,"monitor section invert: text active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MonitorSectionMonoButtonFill,"monitor section mono: fill", "colorAB", HSV(-0,-0.926316,-0.627451,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionMonoButtonFillActive,"monitor section mono: fill active", "colorDdark", HSV(0,-0.260417,-0.247059,1)) /*0.202154 */
CANVAS_COLOR(MonitorSectionMonoButtonLED,"monitor section mono: led", "colorABlight", HSV(0,-0.253521,-0.721569,1)) /*0.226415 */
CANVAS_COLOR(MonitorSectionMonoButtonLEDActive,"monitor section mono: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(MonitorSectionMonoButtonText,"monitor section mono: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionMonoButtonTextActive,"monitor section mono: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloModelButtonFill,"monitor section solo model: fill", "colorAB", HSV(-0,-0.924731,-0.635294,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloModelButtonFillActive,"monitor section solo model: fill active", "colorAB", HSV(-0,-0.837209,-0.662745,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloModelButtonLED,"monitor section solo model: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MonitorSectionSoloModelButtonLEDActive,"monitor section solo model: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionSoloModelButtonText,"monitor section solo model: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloModelButtonTextActive,"monitor section solo model: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonFill,"monitor solo override: fill", "colorAB", HSV(-0,-0.924731,-0.635294,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonFillActive,"monitor solo override: fill active", "colorAB", HSV(-0,-0.837209,-0.662745,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonLED,"monitor solo override: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonLEDActive,"monitor solo override: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonText,"monitor solo override: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloOverrideButtonTextActive,"monitor solo override: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonFill,"monitor solo exclusive: fill", "colorAB", HSV(-0,-0.924731,-0.635294,1)) /*0.97479 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonFillActive,"monitor solo exclusive: fill active", "colorAB", HSV(0,-0.825581,-0.662745,1)) /*1.88235 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonLED,"monitor solo exclusive: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonLEDActive,"monitor solo exclusive: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonText,"monitor solo exclusive: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MonitorSectionSoloExclusiveButtonTextActive,"monitor solo exclusive: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(RudeSoloFill,"rude solo: fill", "colorA", HSV(0,-0.740385,-0.592157,1)) /*0 */
CANVAS_COLOR(RudeSoloFillActive,"rude solo: fill active", "colorA", HSV(0,-0.119469,-0.113725,1)) /*0 */
CANVAS_COLOR(RudeSoloLED,"rude solo: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(RudeSoloLEDActive,"rude solo: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(RudeSoloText,"rude solo: text", "colorLightGray", HSV(0,0,0.0901961,1)) /*32.4706 */
CANVAS_COLOR(RudeSoloTextActive,"rude solo: text active", "colorLightest", HSV(0,0,-0.101961,1)) /*36.7059 */
CANVAS_COLOR(RudeIsolateFill,"rude isolate: fill", "colorDlight", HSV(0,-0.417722,-0.690196,1)) /*0.378517 */
CANVAS_COLOR(RudeIsolateFillActive,"rude isolate: fill active", "colorDlight", HSV(-0,-0.719368,-0.00784314,1)) /*0.135874 */
CANVAS_COLOR(RudeIsolateLED,"rude isolate: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(RudeIsolateLEDActive,"rude isolate: led active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(RudeIsolateText,"rude isolate: text", "colorLightGray", HSV(0,0,0.0941176,1)) /*33.8824 */
CANVAS_COLOR(RudeIsolateTextActive,"rude isolate: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(RudeAuditionFill,"rude audition: fill", "colorA", HSV(0,-0.740385,-0.592157,1)) /*0 */
CANVAS_COLOR(RudeAuditionFillActive,"rude audition: fill active", "colorA", HSV(0,-0.119469,-0.113725,1)) /*0 */
CANVAS_COLOR(RudeAuditionLED,"rude audition: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(RudeAuditionLEDActive,"rude audition: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(RudeAuditionText,"rude audition: text", "colorLightGray", HSV(0,0,0.0941176,1)) /*33.8824 */
CANVAS_COLOR(RudeAuditionTextActive,"rude audition: text active", "colorLightest", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(FeedbackAlertFill,"feedback alert: fill", "colorA", HSV(0,-0.740385,-0.592157,1)) /*0 */
CANVAS_COLOR(FeedbackAlertFillActive,"feedback alert: fill active", "colorA", HSV(0,-0.119469,-0.113725,1)) /*0 */
CANVAS_COLOR(FeedbackAlertLED,"feedback alert: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(FeedbackAlertLEDActive,"feedback alert: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(FeedbackAlertText,"feedback alert: text", "colorLightGray", HSV(0,0,0.0901961,1)) /*32.4706 */
CANVAS_COLOR(FeedbackAlertTextActive,"feedback alert: text active", "colorLightest", HSV(0,0,-0.101961,1)) /*36.7059 */
CANVAS_COLOR(InvertButtonFill,"invert button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(InvertButtonFillActive,"invert button: fill active", "colorDdark", HSV(0,-0.317308,-0.184314,1)) /*0.202154 */
CANVAS_COLOR(InvertButtonLED,"invert button: led", "colorABlight", HSV(0,-0.253521,-0.721569,1)) /*0.226415 */
CANVAS_COLOR(InvertButtonLEDActive,"invert button: led active", "colorC", HSV(-18,-0.384236,-0.203922,1)) /*18.1176 */
CANVAS_COLOR(InvertButtonText,"invert button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(InvertButtonTextActive,"invert button: text active", "colorLightGray", HSV(0,0,0.25098,1)) /*90.3529 */
CANVAS_COLOR(MuteButtonFill,"mute button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(MuteButtonFillActive,"mute button: fill active", "colorB", HSV(-0,0,-0.266667,1)) /*0.213904 */
CANVAS_COLOR(MuteButtonLED,"mute button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MuteButtonLEDActive,"mute button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MuteButtonText,"mute button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(MuteButtonTextActive,"mute button: text active", "colorDarkGray", HSV(0,0,0.00784314,1)) /*2.82353 */
CANVAS_COLOR(SoloButtonFill,"solo button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(SoloButtonFillActive,"solo button: fill active", "colorClight", HSV(-0,0,-0.266667,1)) /*0.0427807 */
CANVAS_COLOR(SoloButtonLED,"solo button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(SoloButtonLEDActive,"solo button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(SoloButtonText,"solo button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(SoloButtonTextActive,"solo button: text active", "colorDarkGray", HSV(0,0,0.00784314,1)) /*2.82353 */
CANVAS_COLOR(RecEnableButtonFill,"record enable button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(RecEnableButtonFillActive,"record enable button: fill active", "colorA", HSV(0,-0.0773481,-0.290196,1)) /*0 */
CANVAS_COLOR(RecEnableButtonLED,"record enable button: led", "colorAlight", HSV(36,-0.430894,-0.517647,1)) /*36.1176 */
CANVAS_COLOR(RecEnableButtonLEDActive,"record enable button: led active", "colorAlight", HSV(36,-0.639216,0,1)) /*35.8568 */
CANVAS_COLOR(RecEnableButtonText,"record enable button: text", "colorLightGray", HSV(0,0,0.14902,1)) /*53.6471 */
CANVAS_COLOR(RecEnableButtonTextActive,"record enable button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(SendButtonFill,"send alert button: fill", "colorClight", HSV(0,-0.825581,-0.662745,1)) /*2.11765 */
CANVAS_COLOR(SendButtonFillActive,"send alert button: fill active", "colorClight", HSV(0,-0.157205,-0.101961,1)) /*0.273087 */
CANVAS_COLOR(SendButtonLED,"send alert button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(SendButtonLEDActive,"send alert button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(SendButtonText,"send alert button: text", "colorLightest", HSV(0,0,-0.2,1)) /*72 */
CANVAS_COLOR(SendButtonTextActive,"send alert button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(TransportButtonFill,"transport button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(TransportButtonFillActive,"transport button: fill active", "colorC", HSV(-0,0,-0.360784,1)) /*0.228077 */
CANVAS_COLOR(TransportButtonLED,"transport button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportButtonLEDActive,"transport button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportButtonText,"transport button: text", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportButtonTextActive,"transport button: text active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportRecenableButtonFill,"transport recenable button: fill", "colorA", HSV(0,-0.663158,-0.627451,1)) /*0 */
CANVAS_COLOR(TransportRecenableButtonFillActive,"transport recenable button: fill active", "colorA", HSV(0,-0.0773481,-0.290196,1)) /*0 */
CANVAS_COLOR(TransportRecenableButtonLED,"transport recenable button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportRecenableButtonLEDActive,"transport recenable button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportRecenableButtonText,"transport recenable button: text", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportRecenableButtonTextActive,"transport recenable button: text active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(TransportOptionButtonFill,"transport option button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(TransportOptionButtonFillActive,"transport option button: fill active", "colorDdark", HSV(-0,-0.91358,-0.682353,1)) /*2.45378 */
CANVAS_COLOR(TransportOptionButtonLED,"transport option button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(TransportOptionButtonLEDActive,"transport option button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TransportOptionButtonText,"transport option button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(TransportOptionButtonTextActive,"transport option button: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(TransportActiveOptionButtonFill,"transport active option button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(TransportActiveOptionButtonFillActive,"transport active option button: fill active", "colorC", HSV(-0,0,-0.360784,1)) /*0.228077 */
CANVAS_COLOR(TransportActiveOptionButtonLED,"transport active option button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(TransportActiveOptionButtonLEDActive,"transport active option button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TransportActiveOptionButtonText,"transport active option button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(TransportActiveOptionButtonTextActive,"transport active option button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(TrackNumberLabelFill,"tracknumber label: fill", "colorMidGray", HSV(0,0,0.133333,1)) /*48 */
CANVAS_COLOR(TrackNumberLabelFillActive,"tracknumber label: fill active", "colorMidGray", HSV(0,0,0.0666667,1)) /*24 */
CANVAS_COLOR(TrackNumberLabelLED,"tracknumber label: led", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TrackNumberLabelLEDActive,"tracknumber label: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TrackNumberLabelText,"tracknumber label: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(TrackNumberLabelTextActive,"tracknumber label: text active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(PluginBypassButtonFill,"plugin bypass button: fill", "colorAB", HSV(-0,-0.924731,-0.635294,1)) /*0.97479 */
CANVAS_COLOR(PluginBypassButtonFillActive,"plugin bypass button: fill active", "colorAB", HSV(-0,-0.837209,-0.662745,1)) /*0.97479 */
CANVAS_COLOR(PluginBypassButtonLED,"plugin bypass button: led", "colorA", HSV(0,0,-0.6,1)) /*0 */
CANVAS_COLOR(PluginBypassButtonLEDActive,"plugin bypass button: led active", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(PluginBypassButtonText,"plugin bypass button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(PluginBypassButtonTextActive,"plugin bypass button: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(PunchButtonFill,"punch button: fill", "colorA", HSV(0,-0.65625,-0.623529,1)) /*0 */
CANVAS_COLOR(PunchButtonFillActive,"punch button: fill active", "colorA", HSV(0,-0.133333,-0.0588235,1)) /*0 */
CANVAS_COLOR(PunchButtonLED,"punch button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(PunchButtonLEDActive,"punch button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(PunchButtonText,"punch button: text", "colorLightGray", HSV(0,0,0.14902,1)) /*53.6471 */
CANVAS_COLOR(PunchButtonTextActive,"punch button: text active", "colorLightest", HSV(0,0,-0.152941,1)) /*55.0588 */
CANVAS_COLOR(MouseModeButtonFill,"mouse mode button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(MouseModeButtonFillActive,"mouse mode button: fill active", "colorC", HSV(-0,0,-0.301961,1)) /*0.0502313 */
CANVAS_COLOR(MouseModeButtonLED,"mouse mode button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MouseModeButtonLEDActive,"mouse mode button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MouseModeButtonText,"mouse mode button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(MouseModeButtonTextActive,"mouse mode button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(NudgeButtonFill,"nudge button: fill", "colorA", HSV(0,-0.653846,-0.592157,1)) /*0 */
CANVAS_COLOR(NudgeButtonFillActive,"nudge button: fill active", "colorDdark", HSV(0,-0.927536,-0.729412,1)) /*6.11765 */
CANVAS_COLOR(NudgeButtonLED,"nudge button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(NudgeButtonLEDActive,"nudge button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(NudgeButtonText,"nudge button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(NudgeButtonTextActive,"nudge button: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(ZoomButtonFill,"zoom button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(ZoomButtonFillActive,"zoom button: fill active", "colorC", HSV(-0,0,-0.360784,1)) /*0.228077 */
CANVAS_COLOR(ZoomButtonLED,"zoom button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(ZoomButtonLEDActive,"zoom button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(ZoomButtonText,"zoom button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(ZoomButtonTextActive,"zoom button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(ZoomMenuFill,"zoom menu: fill", "colorB", HSV(0,-0.79085,-0.4,0.313725)) /*0.257353 */
CANVAS_COLOR(ZoomMenuFillActive,"zoom menu: fill active", "colorDdark", HSV(0,-0.927536,-0.729412,1)) /*6.11765 */
CANVAS_COLOR(ZoomMenuLED,"zoom menu: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(ZoomMenuLEDActive,"zoom menu: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(ZoomMenuText,"zoom menu: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(ZoomMenuTextActive,"zoom menu: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(RouteButtonFill,"route button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(RouteButtonFillActive,"route button: fill active", "colorDarkGray", HSV(0,0,-0.0196078,1)) /*7.05882 */
CANVAS_COLOR(RouteButtonLED,"route button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(RouteButtonLEDActive,"route button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(RouteButtonText,"route button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(RouteButtonTextActive,"route button: text active", "colorDarkGray", HSV(0,0,0.00784314,1)) /*2.82353 */
CANVAS_COLOR(MixerStripButtonFill,"mixer strip button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(MixerStripButtonFillActive,"mixer strip button: fill active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MixerStripButtonLED,"mixer strip button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MixerStripButtonLEDActive,"mixer strip button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MixerStripButtonText,"mixer strip button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(MixerStripButtonTextActive,"mixer strip button: text active", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(MixerStripNameButtonFill,"mixer strip name button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(MixerStripNameButtonFillActive,"mixer strip name button: fill active", "colorDarkGray", HSV(0,0,-0.0196078,1)) /*7.05882 */
CANVAS_COLOR(MixerStripNameButtonLED,"mixer strip name button: led", "colorABlight", HSV(-0,0,-0.690196,1)) /*0.303797 */
CANVAS_COLOR(MixerStripNameButtonLEDActive,"mixer strip name button: led active", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(MixerStripNameButtonText,"mixer strip name button: text", "colorDdark", HSV(-0,-0.926724,-0.0901961,1)) /*0.941176 */
CANVAS_COLOR(MixerStripNameButtonTextActive,"mixer strip name button: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(MidiInputButtonFill,"midi input button: fill", "colorCD", HSV(-0,-0.971154,-0.592157,1)) /*2.11765 */
CANVAS_COLOR(MidiInputButtonFillActive,"midi input button: fill active", "colorC", HSV(-0,0,-0.360784,1)) /*0.228077 */
CANVAS_COLOR(MidiInputButtonLED,"midi input button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MidiInputButtonLEDActive,"midi input button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MidiInputButtonText,"midi input button: text", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(MidiInputButtonTextActive,"midi input button: text active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(LockButtonFill,"lock button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(LockButtonFillActive,"lock button: fill active", "colorDdark", HSV(0,-0.927536,-0.729412,1)) /*6.11765 */
CANVAS_COLOR(LockButtonLED,"lock button: led", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(LockButtonLEDActive,"lock button: led active", "colorDarkGray", HSV(0,0,-0.0901961,0)) /*32.4706 */
CANVAS_COLOR(LockButtonText,"lock button: text", "colorDdark", HSV(-0,0,-0.858824,1)) /*0.54902 */
CANVAS_COLOR(LockButtonTextActive,"lock button: text active", "colorDdark", HSV(-0,-0.921659,-0.14902,1)) /*0.941176 */
CANVAS_COLOR(GenericButtonFill,"generic button: fill", "colorDdark", HSV(-0,-0.932692,-0.592157,1)) /*2.45378 */
CANVAS_COLOR(GenericButtonFillActive,"generic button: fill active", "colorA", HSV(0,0,-0.00784314,1)) /*0 */
CANVAS_COLOR(GenericButtonLED,"generic button: led", "colorDdark", HSV(-0,-0.43038,-0.690196,1)) /*0.54902 */
CANVAS_COLOR(GenericButtonLEDActive,"generic button: led active", "colorDdark", HSV(0,-0.133333,0,1)) /*0.144796 */
CANVAS_COLOR(GenericButtonText,"generic button: text", "colorDdark", HSV(-0,-0.921296,-0.152941,1)) /*0.941176 */
CANVAS_COLOR(GenericButtonTextActive,"generic button: text active", "colorDarkGray", HSV(0,0,0.00784314,1)) /*2.82353 */
CANVAS_COLOR(TransportClockBackground,"transport clock: background", "colorMidGray", HSV(0,0,0.0156863,1)) /*5.64706 */
CANVAS_COLOR(TransportClockText,"transport clock: text", "colorClight", HSV(-0,-0.141129,-0.027451,1)) /*0.023198 */
CANVAS_COLOR(TransportClockEditedText,"transport clock: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TransportClockCursor,"transport clock: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SecondaryClockBackground,"secondary clock: background", "colorMidGray", HSV(0,0,0.0156863,1)) /*5.64706 */
CANVAS_COLOR(SecondaryClockText,"secondary clock: text", "colorClight", HSV(-0,-0.141129,-0.027451,1)) /*0.023198 */
CANVAS_COLOR(SecondaryClockEditedText,"secondary clock: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SecondaryClockCursor,"secondary clock: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TransportDeltaClockBackground,"transport delta clock: background", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(TransportDeltaClockText,"transport delta clock: text", "colorDlight", HSV(-0,-0.564516,-0.027451,1)) /*0.104575 */
CANVAS_COLOR(TransportDeltaClockEditedText,"transport delta clock: edited text", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(TransportDeltaClockCursor,"transport delta clock: cursor", "colorA", HSV(0,0,-0.054902,1)) /*0 */
CANVAS_COLOR(SecondaryDeltaClockBackground,"secondary delta clock: background", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(SecondaryDeltaClockText,"secondary delta clock: text", "colorDlight", HSV(-0,-0.564516,-0.027451,1)) /*0.104575 */
CANVAS_COLOR(SecondaryDeltaClockEditedText,"secondary delta clock: edited text", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SecondaryDeltaClockCursor,"secondary delta clock: cursor", "colorA", HSV(0,0,-0.054902,1)) /*0 */
CANVAS_COLOR(BigClockBackground,"big clock: background", "colorDarkGray", HSV(0,0,-0.0823529,1)) /*29.6471 */
CANVAS_COLOR(BigClockText,"big clock: text", "colorLightest", HSV(0,0,-0.0588235,1)) /*21.1765 */
CANVAS_COLOR(BigClockEditedText,"big clock: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(BigClockCursor,"big clock: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(BigClockActiveBackground,"big clock active: background", "colorDarkGray", HSV(0,0,-0.0823529,1)) /*29.6471 */
CANVAS_COLOR(BigClockActiveText,"big clock active: text", "colorA", HSV(0,0,-0.054902,1)) /*0 */
CANVAS_COLOR(BigClockActiveEditedText,"big clock active: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(BigClockActiveCursor,"big clock active: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(PunchClockBackground,"punch clock: background", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(PunchClockText,"punch clock: text", "colorClight", HSV(0,-0.175824,-0.286275,1)) /*0.117647 */
CANVAS_COLOR(PunchClockEditedText,"punch clock: edited text", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(PunchClockCursor,"punch clock: cursor", "colorA", HSV(0,0,-0.054902,1)) /*0 */
CANVAS_COLOR(SelectionClockBackground,"selection clock: background", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(SelectionClockText,"selection clock: text", "colorClight", HSV(0,-0.175824,-0.286275,1)) /*0.117647 */
CANVAS_COLOR(SelectionClockEditedText,"selection clock: edited text", "colorA", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(SelectionClockCursor,"selection clock: cursor", "colorA", HSV(0,0,-0.054902,1)) /*0 */
CANVAS_COLOR(NudgeClockBackground,"nudge clock: background", "colorMidGray", HSV(0,0,0.0156863,1)) /*5.64706 */
CANVAS_COLOR(NudgeClockText,"nudge clock: text", "colorClight", HSV(0,-0.175824,-0.286275,1)) /*0.117647 */
CANVAS_COLOR(NudgeClockEditedText,"nudge clock: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(NudgeClockCursor,"nudge clock: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(GenericClockBackground,"clock: background", "colorDarkGray", HSV(0,0,-0.0901961,1)) /*32.4706 */
CANVAS_COLOR(GenericClockText,"clock: text", "colorClight", HSV(0,-0.175824,-0.286275,1)) /*0.117647 */
CANVAS_COLOR(GenericClockEditedText,"clock: edited text", "colorABlight", HSV(0,0,0,1)) /*0 */
CANVAS_COLOR(GenericClockCursor,"clock: cursor", "colorABlight", HSV(0,0,0,1)) /*0 */
