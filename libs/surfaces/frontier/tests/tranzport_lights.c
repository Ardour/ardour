/*
 * tranzport 0.1 <tranzport.sf.net>
 * oct 18, 2005
 * arthur@artcmusic.com
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#define VENDORID  0x165b
#define PRODUCTID 0x8101

#define READ_ENDPOINT  0x81
#define WRITE_ENDPOINT 0x02

enum {
	LIGHT_RECORD = 0,
	LIGHT_TRACKREC,
	LIGHT_TRACKMUTE,
	LIGHT_TRACKSOLO,
	LIGHT_ANYSOLO,
	LIGHT_LOOP,
	LIGHT_PUNCH
};

#define BUTTONMASK_BATTERY     0x00004000
#define BUTTONMASK_BACKLIGHT   0x00008000
#define BUTTONMASK_TRACKLEFT   0x04000000
#define BUTTONMASK_TRACKRIGHT  0x40000000
#define BUTTONMASK_TRACKREC    0x00040000
#define BUTTONMASK_TRACKMUTE   0x00400000
#define BUTTONMASK_TRACKSOLO   0x00000400
#define BUTTONMASK_UNDO        0x80000000
#define BUTTONMASK_IN          0x02000000
#define BUTTONMASK_OUT         0x20000000
#define BUTTONMASK_PUNCH       0x00800000
#define BUTTONMASK_LOOP        0x00080000
#define BUTTONMASK_PREV        0x00020000
#define BUTTONMASK_ADD         0x00200000
#define BUTTONMASK_NEXT        0x00000200
#define BUTTONMASK_REWIND      0x01000000
#define BUTTONMASK_FASTFORWARD 0x10000000
#define BUTTONMASK_STOP        0x00010000
#define BUTTONMASK_PLAY        0x00100000
#define BUTTONMASK_RECORD      0x00000100
#define BUTTONMASK_SHIFT       0x08000000

#define STATUS_OFFLINE 0xff
#define STATUS_ONLINE  0x01
#define STATUS_OK  0x00

struct tranzport_s {
	int *dev;
	int udev;
};

typedef struct tranzport_s tranzport_t;

void log_entry(FILE *fp, char *format, va_list ap)
{
	vfprintf(fp, format, ap);
	fputc('\n', fp);
}

void log_error(char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_entry(stderr, format, ap);
	va_end(ap);
}

void vlog_error(char *format, va_list ap)
{
	log_entry(stderr, format, ap);
}

void die(char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vlog_error(format, ap);
	va_end(ap);
	exit(1);
}

tranzport_t *open_tranzport_core()
{
	tranzport_t *z;
	int val;

	z = malloc(sizeof(tranzport_t));
	if (!z)
		die("not enough memory");
	memset(z, 0, sizeof(tranzport_t));

	z->udev = open("/dev/tranzport0",O_RDWR);
	if (!z->udev)
		die("unable to open tranzport");

	return z;
}

tranzport_t *open_tranzport()
{
return open_tranzport_core();
}

void close_tranzport(tranzport_t *z)
{
	int val;

	val = close(z->udev);
	if (val < 0)
		log_error("unable to release tranzport");

	free(z);
}

int tranzport_write_core(tranzport_t *z, uint8_t *cmd, int timeout)
{
	int val;
	val = write(z->udev, cmd, 8);
	if (val < 0)
		return val;
	if (val != 8)
		return -1;
	return 0;
}

int tranzport_lcdwrite(tranzport_t *z, uint8_t cell, char *text, int timeout)
{
	uint8_t cmd[8];

	if (cell > 9) {
		return -1;
	}

	cmd[0] = 0x00;
	cmd[1] = 0x01;
	cmd[2] = cell;
	cmd[3] = text[0];
	cmd[4] = text[1];
	cmd[5] = text[2];
	cmd[6] = text[3];
	cmd[7] = 0x00;

	return tranzport_write_core(z, cmd, timeout);
}

int tranzport_lighton(tranzport_t *z, uint8_t light, int timeout)
{
	uint8_t cmd[8];

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = light;
	cmd[3] = 0x01;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;

	return tranzport_write_core(z, &cmd[0], timeout);
}

int tranzport_lightoff(tranzport_t *z, uint8_t light, int timeout)
{
	uint8_t cmd[8];

	cmd[0] = 0x00;
	cmd[1] = 0x00;
	cmd[2] = light;
	cmd[3] = 0x00;
	cmd[4] = 0x00;
	cmd[5] = 0x00;
	cmd[6] = 0x00;
	cmd[7] = 0x00;

	return tranzport_write_core(z, &cmd[0], timeout);
}

int tranzport_read(tranzport_t *z, uint8_t *status, uint32_t *buttons, uint8_t *datawheel, int timeout)
{
	uint8_t buf[8];
	int val;

	memset(buf, 0xff, 8);
	val = read(z->udev, buf, 8);
	if (val < 0) {
		// printf("errno: %d\n",errno);
		return val;
	}
	if (val != 8)
		return -1;

	/*printf("read: %02x %02x %02x %02x %02x %02x %02x %02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);*/

	*status = buf[1];

	*buttons = 0;
	*buttons |= buf[2] << 24;
	*buttons |= buf[3] << 16;
	*buttons |= buf[4] << 8;
	*buttons |= buf[5];

	*datawheel = buf[6];

	return 0;
}

void lights_core(tranzport_t *z, uint32_t buttons, uint32_t buttonmask, uint8_t light)
{
	if (buttons & buttonmask) {
		if (buttons & BUTTONMASK_SHIFT) {
			tranzport_lightoff(z, light, 1000);
		} else {
			tranzport_lighton(z, light, 1000);
		}
	}
}

void do_lights(tranzport_t *z, uint32_t buttons)
{
	lights_core(z, buttons, BUTTONMASK_RECORD, LIGHT_RECORD);
	lights_core(z, buttons, BUTTONMASK_TRACKREC, LIGHT_TRACKREC);
	lights_core(z, buttons, BUTTONMASK_TRACKMUTE, LIGHT_TRACKMUTE);
	lights_core(z, buttons, BUTTONMASK_TRACKSOLO, LIGHT_TRACKSOLO);
	lights_core(z, buttons, BUTTONMASK_TRACKSOLO, LIGHT_ANYSOLO);
	lights_core(z, buttons, BUTTONMASK_PUNCH, LIGHT_PUNCH);
	lights_core(z, buttons, BUTTONMASK_LOOP, LIGHT_LOOP);
}

void buttons_core(tranzport_t *z, uint32_t buttons, uint32_t buttonmask, char *str)
{
	if (buttons & buttonmask)
		printf(" %s", str);
}

void do_buttons(tranzport_t *z, uint32_t buttons, uint8_t datawheel)
{
	printf("buttons: %x ", buttons);
	buttons_core(z, buttons, BUTTONMASK_BATTERY, "battery");
	buttons_core(z, buttons, BUTTONMASK_BACKLIGHT, "backlight");
	buttons_core(z, buttons, BUTTONMASK_TRACKLEFT, "trackleft");
	buttons_core(z, buttons, BUTTONMASK_TRACKRIGHT, "trackright");
	buttons_core(z, buttons, BUTTONMASK_TRACKREC, "trackrec");
	buttons_core(z, buttons, BUTTONMASK_TRACKMUTE, "trackmute");
	buttons_core(z, buttons, BUTTONMASK_TRACKSOLO, "tracksolo");
	buttons_core(z, buttons, BUTTONMASK_UNDO, "undo");
	buttons_core(z, buttons, BUTTONMASK_IN, "in");
	buttons_core(z, buttons, BUTTONMASK_OUT, "out");
	buttons_core(z, buttons, BUTTONMASK_PUNCH, "punch");
	buttons_core(z, buttons, BUTTONMASK_LOOP, "loop");
	buttons_core(z, buttons, BUTTONMASK_PREV, "prev");
	buttons_core(z, buttons, BUTTONMASK_ADD, "add");
	buttons_core(z, buttons, BUTTONMASK_NEXT, "next");
	buttons_core(z, buttons, BUTTONMASK_REWIND, "rewind");
	buttons_core(z, buttons, BUTTONMASK_FASTFORWARD, "fastforward");
	buttons_core(z, buttons, BUTTONMASK_STOP, "stop");
	buttons_core(z, buttons, BUTTONMASK_PLAY, "play");
	buttons_core(z, buttons, BUTTONMASK_RECORD, "record");
	buttons_core(z, buttons, BUTTONMASK_SHIFT, "shift");
	if (datawheel)
		printf(" datawheel=%02x", datawheel);
	printf("\n");
}

void do_lcd(tranzport_t *z)
{
	tranzport_lcdwrite(z, 0, "    ", 1000);
	tranzport_lcdwrite(z, 1, "DISL", 1000);
	tranzport_lcdwrite(z, 2, "EXIA", 1000);
	tranzport_lcdwrite(z, 3, " FOR", 1000);
	tranzport_lcdwrite(z, 4, "    ", 1000);

	tranzport_lcdwrite(z, 5, "    ", 1000);
	tranzport_lcdwrite(z, 6, " CUR", 1000);
	tranzport_lcdwrite(z, 7, "E FO", 1000);
	tranzport_lcdwrite(z, 8, "UND ", 1000);
	tranzport_lcdwrite(z, 9, "    ", 1000);
}

void do_lcd2(tranzport_t *z)
{
	tranzport_lcdwrite(z, 0, "THE ", 1000);
	tranzport_lcdwrite(z, 1, "TRAN", 1000);
	tranzport_lcdwrite(z, 2, "ZPOR", 1000);
	tranzport_lcdwrite(z, 3, "T RO", 1000);
	tranzport_lcdwrite(z, 4, "  KS", 1000);

	tranzport_lcdwrite(z, 5, "AWES", 1000);
	tranzport_lcdwrite(z, 6, "OMEE", 1000);
	tranzport_lcdwrite(z, 7, "LEEE", 1000);
	tranzport_lcdwrite(z, 8, "UND ", 1000);
	tranzport_lcdwrite(z, 9, "GROK", 1000);
}

lights_off(tranzport_t *z) {
int i;
	for(i=0;i<7;i++) {
	tranzport_lightoff(z, i, 1000);
	}
}

lights_on(tranzport_t *z) {
int i;
	for(i=0;i<7;i++) {
	tranzport_lighton(z, i, 1000);
	}
}

int main()
{
	tranzport_t *z;
	uint8_t status;
	uint32_t buttons;
	uint8_t datawheel;
	int val;

	z = open_tranzport();

	do_lcd(z);

	for(;;) {

	do_lcd(z);
	lights_on(z);
	do_lcd2(z);
	lights_off(z);

//		val = tranzport_read(z, &status, &buttons, &datawheel, 60000);
 		val = -1;
		if (val < 0)
			continue;

		if (status == STATUS_OFFLINE) {
			printf("offline: ");
			continue;
		}

		if (status == STATUS_ONLINE) {
			printf("online: ");
			do_lcd(z);
		}

		do_lights(z, buttons);
		do_buttons(z, buttons, datawheel);
	}

	close_tranzport(z);

	return 0;
}

