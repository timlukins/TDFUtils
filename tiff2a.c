/**************************************************
......
tiff2a
......

Description::
	Convert IEEE floating point tiff to ascii array (suitable for Matlab).
        Can also handle a colour file - if specifying what channel to extract.
	Can also scale results by specified amount (useful for flow fields).
        Can also cut a specific area from the original image.
        If scaling not specified, all values (of whatever channel are normalised 0->1).

Requires::
	libtiff (3.6.*) [www.libtiff.org]

Build::
	gcc -o tiff2a -I<libtiff-includes> -L<libtiff-libraries> -ltiff tiff2a.c

Usage::
	tiff2a <tiff-file> <ascii-file> [base,scale]

Note::
	Assumes LITTLE ENDIAN x86 architecture

****************************************************/

/* Includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <tiffio.h>

/* Globals */

float          *gp_data;
unsigned char  *gp_cdata;
float           gv_max = 0.0;
float           gv_min = FLT_MAX;
int             gv_width = -1;
int             gv_height = -1;
float           gv_scale = 1.0;
float           gv_base = 0.0;
long            gv_x = 0;
long            gv_y = 0;
long            gv_w = 0;
long            gv_h = 0;
enum
{
	NONE, RED, GREEN, BLUE, MONO
};
int             gv_colour = NONE;
char           *gp_token;
float           gv_scale;

/* Prototypes */

int             ReadFloat32TIFF(char *);
int             ReadChar8TIFF(char *);
int             WriteFloat32Ascii(char *);

/* Entry */

int
main(int pv_argc, char **pp_argv)
{
	int             lv_at;

	/* Parse any options */

	for (lv_at = 1; lv_at < pv_argc - 2; lv_at += 2)
	{
		if (pp_argv[lv_at][0] != '-')
			break;
		switch (pp_argv[lv_at][1])	/* i.e. jump '-' char */
		{
		case 'r':
			if ((gp_token = strtok(pp_argv[lv_at + 1], ",")) == NULL)
			{
				fprintf(stderr, "No from base provided!\n");
				gv_scale = atof(pp_argv[lv_at + 1]);
			} else
			{
				gv_base = atof(gp_token);
				if ((gp_token = strtok(NULL, ",")) == NULL)
					fprintf(stderr, "No scale by provided!\n");
				else
					gv_scale = atof(gp_token);
			}
			break;
		case 'c':
			if ((gp_token = strtok(pp_argv[lv_at + 1], ",")) == NULL)
				fprintf(stderr, "No cut x location provided!\n");
			gv_x = atoi(gp_token);
			if ((gp_token = strtok(NULL, ",")) == NULL)
				fprintf(stderr, "No cut y location provided!\n");
			gv_y = atoi(gp_token);
			if ((gp_token = strtok(NULL, ",")) == NULL)
				fprintf(stderr, "No cut width provided!\n");
			gv_w = atoi(gp_token);
			if ((gp_token = strtok(NULL, ",")) == NULL)
				fprintf(stderr, "No cut height provided!\n");
			gv_h = atoi(gp_token);
			break;
		case 'e':
			switch (pp_argv[lv_at + 1][0])
			{
			case 'r':
				gv_colour = RED;
				break;
			case 'g':
				gv_colour = GREEN;
				break;
			case 'b':
				gv_colour = BLUE;
				break;
			case 'm':
				gv_colour = MONO;
				break;
			default:
				fprintf(stderr, "Can't extract %s\n", pp_argv[lv_at + 1]);
			}
			break;
		case 's':
			gv_scale = atoi(pp_argv[lv_at + 1]);
			if (gv_scale < 0.0 || gv_scale > 100.0)
			{
				fprintf(stderr, "Scale not in range 0-100 setting to 100\n");
				gv_scale = 100.0;
			}
			break;
		default:
			fprintf(stderr, "Unrecognised option: %s\n", pp_argv[lv_at]);
			fprintf(stderr, "Usage: -rescale from,by -cut x,y,w,h -extract (red|green|blue) -scale x <tiff_file> <ascii-file>\n");
			return 0;

		}
	}

	/* Read data from tiff file - depending if colour extraction or not */

	if (gv_colour == NONE)
	{
		if (!ReadFloat32TIFF(pp_argv[pv_argc - 2]))
		{
			fprintf(stderr, "Error reading TIFF file %s\n", pp_argv[pv_argc - 2]);
			return 0;
		}
	} else
	{
		if (!ReadChar8TIFF(pp_argv[pv_argc - 2]))
		{
			fprintf(stderr, "Error reading TIFF file %s\n", pp_argv[pv_argc - 2]);
			return 0;
		}
	}

	/* Check what to do if any cut was actually given */

	if (gv_w == 0 && gv_x == 0)
		gv_w = gv_width;
	if (gv_h == 0 && gv_y == 0)
		gv_h = gv_height;

	/* Write data to ascii */

	if (!WriteFloat32Ascii(pp_argv[pv_argc - 1]))
	{
		fprintf(stderr, "Error writing ASCII %s\n", pp_argv[pv_argc - 1]);
		return 0;
	}
	return 1;
}

int
ReadChar8TIFF(char *pp_file)
{
	TIFF           *lp_tif;
	char           *lp_pixels;
	unsigned char  *lp_tiffdata;
	int             lv_nstrips;
	int             lv_stripsize;
	int             lv_counter;
	int             lv_pixel;
	int             lv_npixels;
	int             lv_step;
	tdata_t         lp_buf;
	tstrip_t        lv_strip;
	uint16          lv_bitsper;

	if (lp_tif = TIFFOpen(pp_file, "r"))
	{
		TIFFGetField(lp_tif, TIFFTAG_BITSPERSAMPLE, &lv_bitsper);
		if (lv_bitsper != 8)
		{
			fprintf(stderr, "Colour data not 8 bit per channel!\n");;
			TIFFClose(lp_tif);
			return 0;
		}
		TIFFGetField(lp_tif, TIFFTAG_IMAGEWIDTH, &gv_width);
		TIFFGetField(lp_tif, TIFFTAG_IMAGELENGTH, &gv_height);

		gp_cdata = (unsigned char *) malloc(gv_width * gv_height * sizeof(unsigned char));

		lv_pixel = 0;
		lv_npixels = gv_width * gv_height;
		if (lp_tif)
		{
			if (gv_colour != MONO)
				lv_step = 3;
			else
				lv_step = 1;
			lv_stripsize = TIFFStripSize(lp_tif);
			lv_nstrips = TIFFNumberOfStrips(lp_tif);
			lp_buf = _TIFFmalloc(TIFFStripSize(lp_tif));
			lv_counter = 0;
			for (lv_strip = 0; lv_strip < lv_nstrips; lv_strip++)
			{
				TIFFReadEncodedStrip(lp_tif, lv_strip, lp_buf, (tsize_t) - 1);
				lp_tiffdata = (unsigned char *) lp_buf;
				for (lv_pixel = 0; lv_pixel < lv_stripsize; lv_pixel += lv_step)	/* NOTE 3 Channel */
				{
					if (lv_counter < lv_npixels)
					{
						switch (gv_colour)
						{
						case RED:
						case MONO:
							gp_cdata[lv_counter] = lp_tiffdata[lv_pixel];
							break;
						case GREEN:
							gp_cdata[lv_counter] = lp_tiffdata[lv_pixel + 1];
							break;
						case BLUE:
							gp_cdata[lv_counter] = lp_tiffdata[lv_pixel + 2];
							break;
						}
						lv_counter++;
					}
				}
			}
			_TIFFfree(lp_buf);
		}
		TIFFClose(lp_tif);
	} else
		return 0;
	return 1;
}

int
ReadFloat32TIFF(char *pp_file)
{
	TIFF           *lp_tif;
	float          *lp_pixels;
	float          *lp_tiffdata;
	int             lv_nstrips;
	int             lv_stripsize;
	int             lv_counter;
	int             lv_pixel;
	int             lv_npixels;
	uint16          lv_bitsper;
	tdata_t         lp_buf;
	tstrip_t        lv_strip;
	char           *tmp;

	if (lp_tif = TIFFOpen(pp_file, "r"))
	{
		TIFFGetField(lp_tif, TIFFTAG_BITSPERSAMPLE, &lv_bitsper);
		if (lv_bitsper != 32)
		{
			fprintf(stderr, "Depth Image is not 32 bit float (it's %d)!\n", lv_bitsper);
			TIFFClose(lp_tif);
			return 0;
		}
		TIFFGetField(lp_tif, TIFFTAG_IMAGEWIDTH, &gv_width);
		TIFFGetField(lp_tif, TIFFTAG_IMAGELENGTH, &gv_height);

		gp_data = malloc(gv_width * gv_height * sizeof(float));

		lv_pixel = 0;
		lv_npixels = gv_width * gv_height;
		if (lp_tif)
		{
			lv_stripsize = TIFFStripSize(lp_tif) / 4;
			lv_nstrips = TIFFNumberOfStrips(lp_tif);
			lp_buf = _TIFFmalloc(TIFFStripSize(lp_tif));
			lv_counter = 0;
			for (lv_strip = 0; lv_strip < lv_nstrips; lv_strip++)
			{
				TIFFReadEncodedStrip(lp_tif, lv_strip, lp_buf, (tsize_t) - 1);
				lp_tiffdata = (float *) lp_buf;
				for (lv_pixel = 0; lv_pixel < lv_stripsize; lv_pixel++)
				{
					if (lv_counter < lv_npixels)
						gp_data[lv_counter] = lp_tiffdata[lv_pixel];
					if (gp_data[lv_counter] > gv_max)
						gv_max = gp_data[lv_counter];
					if (gp_data[lv_counter] < gv_min)
						gv_min = gp_data[lv_counter];

					lv_counter++;
				}
			}
			_TIFFfree(lp_buf);
		}
		TIFFClose(lp_tif);
	} else
		return 0;
	return 1;
}

int
WriteFloat32Ascii(char *pp_file)
{

	FILE           *lp_afile;
	long            lv_atx;
	long            lv_aty;

	if ((lp_afile = fopen(pp_file, "w")) == NULL)
	{
		return 0;
	}
	for (lv_aty = gv_y; lv_aty < (gv_y + gv_h); lv_aty++)
	{
		for (lv_atx = gv_x; lv_atx < (gv_x + gv_w); lv_atx++)
		{
			if (gv_colour == NONE)
				fprintf(lp_afile, "%f ", gv_base + (gv_scale * (gp_data[(lv_aty * gv_width) + lv_atx])));
			/*
			 * fprintf(lp_afile,"%f
			 * ",gv_base+(gv_scale*(gp_data[(lv_aty*gv_width)+lv_a
			 * tx]/gv_max)));
			 */
			else
				fprintf(lp_afile, "%f ", gv_base + (gv_scale * (gp_cdata[(lv_aty * gv_width) + lv_atx] / 255.0)));
		}
		fprintf(lp_afile, "\n");
	}

	fclose(lp_afile);

	return 1;
}
