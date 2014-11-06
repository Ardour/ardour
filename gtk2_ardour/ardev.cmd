set truelala=%path%
set TOP=%ADWIN%\tracks


set LIBS=%TOP%\build\libs

rem 
rem  when running ardev, the various parts of Ardour have not been consolidated into the locations that they 
rem  would normally end up after an install. We therefore need to set up environment variables so that we
rem  can find all the components. 
rem 

set ARDOUR_PATH=%TOP%\gtk2_ardour\icons;%TOP%\gtk2_ardour\pixmaps;%TOP%\build\gtk2_ardour;%TOP%\gtk2_ardour;.
set ARDOUR_SURFACES_PATH=%LIBS%\surfaces\osc;%LIBS%\surfaces\generic_midi;%LIBS%\surfaces\tranzport;%LIBS%\surfaces\powermate;%LIBS%\surfaces\mackie;%LIBS%\surfaces\wiimote
set ARDOUR_PANNER_PATH=%LIBS%\panners
set ARDOUR_DATA_PATH=%TOP%;%TOP%\build;%TOP%\gtk2_ardour;%TOP%\build\gtk2_ardour;.
set ARDOUR_MIDIMAPS_PATH=%TOP%\midi_maps;.
set ARDOUR_MCP_PATH=%TOP%\mcp;.
set ARDOUR_EXPORT_FORMATS_PATH=%TOP%\export;.
set ARDOUR_BACKEND_PATH=%LIBS%\backends\jack;%LIBS%\backends\wavesaudio

rem 
rem  even though we set the above variables, ardour requires that these 
rem  two also be set. the above settings will override them.
rem 

set ARDOUR_CONFIG_PATH=%TOP%;%TOP%\gtk2_ardour;%TOP%\build;%TOP%\build\gtk2_ardour
set ARDOUR_DLL_PATH=%LIBS%

set GTK_PATH=%LIBS%\clearlooks-newer
set VAMP_PATH=%LIBS%\vamp-plugins

set PATH=%A3%\inst\bin;%GTK%\inst\bin;c:\mingw64win32\x86_64-w64-mingw32\bin;c:\mingw64win32\bin;%LIBS%\qm-dsp;%LIBS%\vamp-sdk;%LIBS%\surfaces;%LIBS%\surfaces\control_protocol;%LIBS%\ardour;%LIBS%\midi++2;%LIBS%\pbd;%LIBS%\rubberband;%LIBS%\soundtouch;%LIBS%\gtkmm2ext;%LIBS%\gnomecanvas;%LIBS%\libsndfile;%LIBS%\appleutility;%LIBS%\taglib;%LIBS%\evoral;%LIBS%\evoral\src\libsmf;%LIBS%\audiographer;%LIBS%\timecode;%LIBS%\libltc;%LIBS%\canvas;%PATH%
echo "------------------------------------------------------------------"
%ADWIN%\tracks\build\gtk2_ardour\trackslive.exe %1
echo "------------------------------------------------------------------"
set path=%truelala%