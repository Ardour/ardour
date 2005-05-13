			    ARDOUR README.es

	  Paul Davis <paul@linuxaudiosystems.com> June 2003

Bienvenido a Ardour.  Este programa aun esta bajo desarrollo,
pero ha llegado a un estado en el cual es productivo y util tener
a otras personas probandolo y tal vez (con suerte!) arreglando errores 
y agregando nuevas caracteristicas.

**** SEGURIDAD *******************************************************

Para ejecutar Ardour con la mas baja latencia posible, es necesario usar 
POSIX Real-Time Scheduling (tiempo Real) como tambien bloquear toda la 
memoria que usa en la memoria fisica de la RAM. Estos requerimientos solo 
se pueden cumplir si Ardour es ejecutado con privilegios de usuario root. 

Por otro lado, esto no es tan malo.  Si no planea instalar a Ardour
usando el comando "setuid root"(lo cual no funcionaria de todas formas),
entonces alguna persona que use su estacion de trabajo debera tener
que haber "ganado" privilegios de acceso root para hacerlo funcionar
de esa forma.Si esa otra persona ya tiene acceso de tipo root, Ardour
es la menor de sus preocupaciones. Asi que, relajese. Usaremos
capabilities (privilegios root) una vez que los Kernels de Linux empiecen
a aparecer con estas ya activadas, aunque esto no ayudara mucho a la
seguridad, ya que las mencionadas "capabilities" habilitarian a cualquier
hacker astatuto a hacer lo que quiciera.

Alternativamente, usted puede elegir ejecutar a Ardour sin Scheduling
de Tiempo Real, lo cual no es tan terrible. Simplemente no va a ser util
en situaciones que demandan baja latencia, las cuales son deseables en
la mayoria de los ambientes de estudios.
Note que esto pierde importancia en el caso que usted disponga de 
hardware de audio capaz de hacer "monitorizacion por hardware". Esto 
hace recaer gran parte del peso de procesamiento sobre el dispositivo 
de audio y no sobre el CPU como es el caso de la "monitorizacion por 
software". En el caso de monitorizacion por hardware, la falta de baja
latencia hara que los controles de la interfaz visual de Ardour 
reaccionen con menos fluidez, sin embargo la monitorizacion durante la
captura sera excelente.

**** COMPATIBILIDAD DE HARDWARE *************************************

Ardour usa JACK para todo el manejo de entradas y salidas de audio,
lo cual provee conecciones directas al hardware de audio y a otras 
aplicaciones compatibles con JACK. Este no es el lugar mas apropiado
para discutir acerca de JACK, pero en caso de que se estubiera 
preguntando:

Aunque JACK usa la libreria ALSA 0.9.0, JACK la aprovecha de una forma
que ninguna otra aplicacion lo ha hecho hasta ahora y, tambien intenta
usar ciertas caracteristicas de hardware que nuevamente, ninguna de las
actuales aplicaciones usa. Como resultado, aunque una completa 
portabilidad a todo el hardware soportado por ALSA es un objetivo 
eventualmente realizable, puede ser que nazcan problemas relacionados
con la compatibildad de hardware. Por favor recuerde que mi objetivo
principal con JCK es el de crear un sistema profesional de audio y, con
Ardour, una estacion de trabajo de audio digital profesional. Si estos
terminan siendo utiles para personas con placas de 2/4 canales, muy bien,
pero ese no es mi foco de interes principal.

Otro punto importante es que su dispositivo de sonido debe soportar
full duplex de entrada/salida (reproduccion y grabacion simultaneas)
con el mismo formato para la captura y la reproduccion (no se puede
usar una frecuencia de muestreo de 44.1 khz para reproducir y una
de 48khz para grabar, ambas deben ser iguales, lo mismo sucede para
la resolucion en bits. Esto significa, por ejemplo, que la placa
SoundBlaster AWE no puede ser usada con JACK en modo full duplex.
-Esta placa solo soporta fullduplex si una de las dos (grabacion o
reproduccion) usa 8 bits y la otra 16 bits.
Este tipo de limitacion existe solo en algunas placas y, de ser asi
no son adecuadas para el uso en aplicaciones como JACK y ARDOUR por 
otras razones.

Hasta la fecha, JACK fue probado con las siguientes interfaces de audio:

   RME Hammerfall (Digi9652)              (26 channels in, 26 channels out)
   RME Hammerfall DSP (hdsp)              (26 channels in, 26 channels out)
   RME Hammerfall Light (Digi9636)        (18 channels in, 18 channels out)
   Midiman Delta series (ice1712 chipset) (12 channels in, 10 channels out)
  
   Varios chips de sonido de nivel de consumidor (relativamente baratas),
   tipicamente con 2 canales de entrada y 2/4 de salida,incluyendo:
   
   Hoontech 4Dwave-NX (chipset Trident)
   Ensoniq 5880
   Soundblaster 32
   Soundblaster 64
   Creative SBLive64

y muchas mas.

Asi que, basicamente, parece funcionar con practicamente todas aquellas
placas que son soportadas por ALSA, lo cual es el objetivo.
  
**********************************************************************

REPORTES DE ERRORES EN CODIGO (BUGS)
-------------------------------------

Los bugs deben ser reportados a http://ardour.org/mantis/ . Es mas probable
que estos sean recordados y analizados alli. Por favor, chequee alli la lista
de bugs ya reportados para asegurarse que el que usted encontro no haya sido
reportado aun o haya sido resuelto en CVS.

PARA COMPILAR ARDOUR
--------------------
Vea el archivo "BUILD" (por ahora en ingles, espaniol mas adelante).

EJECUTANDO ARDOUR
-----------------

NOTA: Debe haber ya un server JACK corriendo antes de ejecutar Ardour
      --------------------------------------------------------------- 

* Escribiendo ardour en una consola y presionando ENTER o INTRO deberia iniciar
  el programa.

* "ardour --help" muestra las opciones disponibles desde la linea de comando









Nota de Traduccion (Spanish Translation Note)
---------------------------------------------
#Nota del tipeo:la letra pronunciada ENIE aparece en este archivo 
#como ni (letra "n" y letra "i") para mayor compatibilidad con todos 
#los visores de texto.
#Asi mismo no se han aplicado las tildes(acentos).
#Estos no son errores de tipeo. Si llegara a encontrar algun otro error
#en cualquiera de los archivos con extension ".es" por favor 
#hagamelo saber a alexkrohn@fastmail.fm
#  Muchas gracias
#      Alex

