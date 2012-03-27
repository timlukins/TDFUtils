/**************************************************
......
z2tiff
......

Description::
	Convert Renderman 'z file' raw depth format to IEEE floating point tiff.

Supports::
	"Aqsis" formated z files.

Requires::
	libtiff (3.6.*) [www.libtiff.org]

Build::
	gcc -o z2tiff -I<libtiff-includes> -L<libtiff-libraries> -ltiff z2tiff.c

Usage::
	z2tiff <z-file> <tiff-file>

Note::
	Assumes LITTLE ENDIAN x86 architecture
	Currently hard-coded rescale range operation.

****************************************************/

/* Includes */

#include <stdio.h>
#include <stdlib.h>
#include <tiffio.h>

/* Globals */

float          *gp_data;
int             gv_width = -1;
int             gv_height = -1;

/* Prototypes */

int             readAqsisZFILE(char *);
int             writeFloat32TIFF(char *);
int             rescaleRange(float, float);

/* Entry */

int
main(int pv_argc, char **pp_argv)
{

	if (pv_argc != 3)
	{
		fprintf(stderr, "Usage: z2tiff <z-file> <tiff-file>\n");
		return 0;
	}
	/* Read data from z file */

	if (!readAqsisZFILE(pp_argv[1]))
	{
		fprintf(stderr, "Error reading Z file %s\n", pp_argv[1]);
		return 0;
	}

	/* Write date to tiff */

	if (!writeFloat32TIFF(pp_argv[2]))
	{
		fprintf(stderr, "Error writing TIFF %s\n", pp_argv[2]);
		return 0;
	}
	return 1;
}

/* Read Aqsis ZFILE */

int
readAqsisZFILE(char *pp_infile)
{
	FILE           *pl_zfile;

	/* Aqsis header format */

	struct AqsisHeader
	{
		char            format[16];
		int             width;
		int             height;
		float           worldcamera[16];
		float           camerascreen[16];
	}              *ps_header;

	int             lv_headersize;
	unsigned long   lv_nbytes;

	/* Open file */

	if ((pl_zfile = fopen(pp_infile, "r")) == NULL)
	{
		return 0;
	}
	/* Read header */

	lv_headersize = sizeof(*ps_header);
	if (fread(ps_header, 1, lv_headersize, pl_zfile) < lv_headersize)
		return 0;

	/* Read data */

	gv_width = ps_header->width;
	gv_height = ps_header->height;
	lv_nbytes = gv_width * gv_height * sizeof(float);

	gp_data = malloc(lv_nbytes);
	if (fread(gp_data, 1, lv_nbytes, pl_zfile) < lv_nbytes)
		return 0;

	/* Close */

	fclose(pl_zfile);

	return 1;
}

/* Write Float32 TIFF */

int
writeFloat32TIFF(char *pp_outfile)
{

	TIFF           *pl_tiff;
	int             lv_row;

	if ((pl_tiff = TIFFOpen(pp_outfile, "w")) == NULL)
	{
		return 0;
	}
	TIFFSetField(pl_tiff, TIFFTAG_IMAGEWIDTH, gv_width);
	TIFFSetField(pl_tiff, TIFFTAG_IMAGELENGTH, gv_height);
	TIFFSetField(pl_tiff, TIFFTAG_BITSPERSAMPLE, 32);
	TIFFSetField(pl_tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
	TIFFSetField(pl_tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

	TIFFSetField(pl_tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(pl_tiff, TIFFTAG_SAMPLESPERPIXEL, 1);

	TIFFSetField(pl_tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(pl_tiff, TIFFTAG_SOFTWARE, TIFFGetVersion());
	TIFFSetField(pl_tiff, TIFFTAG_DOCUMENTNAME, pp_outfile);

	TIFFSetField(pl_tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_IEEEFP);

	int             lv_rowsperstrip = TIFFDefaultStripSize(pl_tiff, lv_rowsperstrip);
	TIFFSetField(pl_tiff, TIFFTAG_ROWSPERSTRIP, lv_rowsperstrip);

	for (lv_row = 0; lv_row < gv_height; lv_row += lv_rowsperstrip)
	{
		unsigned char  *lv_raster_strip;
		int             lv_rows_to_write;
		int             lv_bytes_per_pixel;

		lv_raster_strip = (unsigned char *) (gp_data + lv_row * gv_width);
		lv_bytes_per_pixel = 4;

		if (lv_row + lv_rowsperstrip > gv_height)
			lv_rows_to_write = gv_height - lv_row;
		else
			lv_rows_to_write = lv_rowsperstrip;

		if (TIFFWriteEncodedStrip(pl_tiff, lv_row / lv_rowsperstrip, lv_raster_strip, lv_bytes_per_pixel * lv_rows_to_write * gv_width) == -1)
		{
			return 0;
		}
	}

	TIFFClose(pl_tiff);

	return 1;
}

/* Rescale range */

int
rescaleRange(float pv_from, float pv_to)
{

	int             lv_npoints, lv_atpt;
	float           lv_max;
	float           lv_min;
	float           lv_scale;
	float          *lp_at;

	lv_npoints = gv_width * gv_height;
	lv_min = 99999.0;
	lv_max = 0.0;
	lv_scale = 1.0;

	/* Find current max and min */

	for (lv_atpt = 0; lv_atpt < lv_npoints; lv_atpt++)
	{
		lp_at = gp_data + lv_atpt;
		if (*lp_at < lv_min)
			lv_min = *lp_at;
		if (*lp_at > lv_max)
			lv_max = *lp_at;
	}

	/* Calculate scale value */

	lv_scale = (1.0 / (lv_max - lv_min)) * (pv_to - pv_from);

	/* Apply rescale */

	for (lv_atpt = 0; lv_atpt < lv_npoints; lv_atpt++)
	{
		lp_at = gp_data + lv_atpt;
		*lp_at = pv_from + (*lp_at * lv_scale);
	}
}
