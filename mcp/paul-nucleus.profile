<?xml version="1.0" encoding="UTF-8"?>
<MackieDeviceProfile>
  <Name value="Basic SSL Nucleus"/>
  <Buttons>
    <Button name="Name/Value" shift="Transport/ToggleExternalSync"/>
    <Button name="F1" plain="MouseMode/set-mouse-mode-object" shift="Transport/GotoStart" cmdalt="options/SendMidiClock"/>
    <Button name="F2" plain="MouseMode/set-mouse-mode-range" shift="Transport/ToggleExternalSync"/>
    <Button name="F3" plain="Transport/ToggleClick" control="Transport/ToggleAutoReturn" shift="Transport/ToggleAutoPlay"/>
    <Button name="F4" plain="Transport/TogglePunch" control="Transport/TogglePunchOut" shift="Transport/TogglePunchIn" option="Transport/ToggleExternalSync"/>
    <Button name="F5" plain="Zoom/zoom-focus-center" control="Zoom/zoom-focus-mouse" shift="Zoom/zoom-focus-left" option="Zoom/zoom-focus-playhead" cmdalt="Zoom/zoom-focus-right" shiftcontrol="Zoom/zoom-focus-edit"/>
    <Button name="F6" plain="Common/toggle-audio-connection-manager" control="Common/ToggleBigClock" shift="Common/toggle-midi-connection-manager" option="Common/toggle-mixer-on-top"/>
    <Button name="F7" plain="Transport/ToggleExternalSync"/>
    <Button name="F8" plain="Editor/cycle-snap-mode" control="Editor/prev-snap-choice" shift="Editor/next-snap-choice" option="Editor/prev-snap-choice"/>
    <Button name="Marker" plain="Editor/jump-forward-to-mark"/>
    <Button name="Loop" shift="Region/loop-region"/>
    <Button name="Nudge" plain="Region/nudge-forward" control="Region/naturalize-region" shift="Region/nudge-backward" option="Transport/ToggleExternalSync"/>
    <Button name="Drop" plain="Editor/jump-backward-to-mark"/>
  </Buttons>
</MackieDeviceProfile>
