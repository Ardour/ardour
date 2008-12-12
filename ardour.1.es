.TH ARDOUR 1 2002-12-29
.SH NOMBRE
ardour \- una estación de trabajo de audio digital
.SH SINOPSIS
ardour
.B \-b
.RB [ \-U
.IR archivo ]
.RI [ sesión ]
.br
ardour
.B \-\-help
.SH DESCRIPCIÓN
Ardour graba multiples canales simultáneos a disco rigido (HDR) y es una
estación de trabajo de audio digital (DAW).
Es capaz de grabar 24 o mas canales simultáneamente con calidad de audio
de 32 bit y a 48khz.
La inteción de Ardour es funcionar como un sistema HDR "profesional",
reemplazando soluciones de hardware dedicado como la Mackie HDR, el 
Tascam 2424 y otros sistemas tradicionales que emplean cinta como la 
linea de ADATs de Alesis.
Tambien se busca igualar o mejorar la       funcionalidad de sistemas
basados en software como ProTools,         Samplitude, Logic Audio, 
Nuendo y Cubase VST (reconocemos a estos y a todos los nombres ya 
mencionados, como marcas registradas de sus respectivos dueños). 
Ardour soporta Control de Maquina MIDI, por lo que puede ser controlado
desde cualquier controladora MMC, como la "Mackie Digital 8 Bus Mixer"
y otros mixers digitales modernos.
.SH OPCIONES
.TP
.B \-b
Muestra todos los comandos asignables a teclas del teclado.
.TP
.B \-U
Especifica el archivo de interface visual.
El que viene provisto por Ardour se llama
.B ardour_ui.rc
y se lo puede encontrar en el primer nivel de la carpeta del código de Ardour.
Este archivo controla todos los colores y fuentes usados por Ardour.
Ardour funcionará sin este, pero se verá, uhm, feo.
.TP
.B \-\-help
Muestra el mensaje de ayuda.
.SH ARCHIVOS
.TP
.B ardour.rc
Configuraciones preestablecidas y de inicio de Ardour.
.TP
.B ardour_ui.rc
Configuraciones de la interface visual de Ardour.
.SH VARIABLES
.TP
.B ARDOUR_RC
Ubicación del archivo ardour.rc.
.TP
.B ARDOUR_SYSTEM_RC
Ubicación del archivo ardour_system.rc.
.TP
.B LADSPA_PATH
Ubicación de plugins LADSPA.
.SH BUGS (Errores en el codigo)
Si.
.SH AUTOR
Paul Davis.
.I No
intente
contactarlo directamente.
En cambio,
mande un email a <ardour-dev@lists.ardour.org>.
Usted puede suscribirse a:
.br
http://lists.ardour.org/listinfo.cgi/ardour-dev-ardour.org
