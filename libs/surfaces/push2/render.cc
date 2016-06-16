#include <libusb.h>
#include <unistd.h>
#include <assert.h>

#include <pangomm/layout.h>
#include <cairomm/context.h>
#include <cairomm/surface.h>

int
deliver_image_surface (libusb_device_handle* handle, Cairo::RefPtr<Cairo::ImageSurface> surface)
{
	static uint8_t headerPkt[] = { 0xef, 0xcd, 0xab, 0x89, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	static uint16_t dataPkt[160*1024];

	const Cairo::Format format = surface->get_format();

	if (format != Cairo::FORMAT_ARGB32) {
		return -1;
	}

	unsigned char *data = surface->get_data ();
	const int width = surface->get_width();
	const int height = surface->get_height();
	const int stride = surface->get_stride();

	assert (width == 960);
	assert (height == 160);

	/* fill a data packet (320kB) */

	uint16_t* pkt_ptr = (uint16_t*) dataPkt;

	for (int row = 0; row < height; ++row) {

		uint8_t* dp = data + row * stride;

		for (int col = 0; col < width; ++col) {

			/* fetch r, g, b (range 0..255). Ignore alpha */
			const int r = (*((uint32_t*)dp) >> 16) & 0xff;
			const int g = (*((uint32_t*)dp) >> 8) & 0xff;
			const int b = *((uint32_t*)dp) & 0xff;

			/* convert to 5 bits, 6 bits, 5 bits, respectively */
			/* generate 16 bit BGB565 value */

			*pkt_ptr++ = (r >> 3) | ((g & 0xfc) << 3) | ((b & 0xf8) << 8);

			dp += 4;
		}

		/* skip 128 bytes to next line. This is filler, used to avoid line borders occuring in the middle of 512
		   byte USB buffers
		*/

		pkt_ptr += 64; /* 128 bytes = 64 int16_t */
	}

	int transferred = 0;

	libusb_bulk_transfer (handle, 0x01, headerPkt, sizeof(headerPkt), &transferred, 1000);
	libusb_bulk_transfer (handle, 0x01, (uint8_t*) dataPkt, sizeof(dataPkt), &transferred, 1000);
}

