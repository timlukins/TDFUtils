/**************************************************
.....
a2c3d
.....

Description::
	Convert ascii file of 3D points to .c3d format.

Supports::
	ASCII matrix format for n points by t samples:

        x1+0 y1+0 z1+0 ... xn+0 yn+0 zn+0
        ...
        x1+t y1+t z1+t ... xn+t yn+t zn+t

        i.e. frames (cols) x points (rows)

Build::
	gcc -o a2c3d a2c3d.c

Usage::
	a2c3d <ascii-file> <c3d-file>

Note::
        "Validity"
        This is a radically reduced version of a c3D
        file - it should meet all acceptable parsing
        criteria, but (because it does not include
        any parameter info, etc.) it might fail when
        treated by certain applications.

****************************************************/

/**
* Includes
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
* Defines
*/

#define MAXLINE 1000000

/* Typedefs and structs */

/**
* A C3D Word is 16bits.
* i.e. a short = 2 * bytes.
*/

typedef short   C3DWord;

/**
* A C3D Word2 is 32bits.
* i.e. a float = 4 * 1 byte.
*/

typedef float   C3DWord2;

/**
* A C3D Byte is 8bits.
* i.e. a char
*/

typedef char    C3DByte;

/**
* A C3D Block is 512 bytes.
*/

typedef C3DByte C3DBlock[512];


/**
* Entry
*/

int
main(int pv_argc, char **pp_argv)
{
	/**
  * A C3D Header is as single record of 256 Words.
  * i.e. 512 bytes = 1 C3DBlock.
  */

	struct C3DHeader
	{
		C3DByte         firstblock;
		C3DByte         id;
		C3DWord         npoints;
		C3DWord         nmeasures;
		C3DWord         firstframe;
		C3DWord         lastframe;
		C3DWord         maxinterpolgap;
		C3DWord2        scalefactor;
		C3DWord         datastart;
		C3DWord         nsamples;
		C3DWord2        framerate;
		C3DWord         _futureuse[135];
		C3DWord         labelpresent;
		C3DWord         labelblock;
		C3DWord         bigeventlabels;
		C3DWord         ntimeevents;
		C3DWord         __futureuse;
		C3DWord         eventtimes[36];
		C3DWord         eventflags[9];
		C3DWord         ___futureuse;
		C3DWord         eventlabels[36];
		C3DWord         ____futureuse[22];
	}               lv_header;


	/**
  * A C3D Real XYZ Data format.
  */

	struct C3DRealXYZ
	{
		C3DWord2        x;
		C3DWord2        y;
		C3DWord2        z;
		C3DByte         cameras;
		C3DByte         residual;
	}               lv_realXYZ;


	FILE           *lp_ascii;
	FILE           *lp_ctd;
	char            la_line[MAXLINE];
	long            lv_nframes;
	long            lv_npoints;
	long            lv_frame;
	long            lv_point;
	long            lv_bytes;
	long            lv_nblocks;
	C3DBlock        lv_params;
	float           lv_value;

	/* Check params... */

	if (pv_argc != 3)
	{
		fprintf(stderr, "Usage: a2c3d <ascii-file> <c3d-file>\n");
		return 0;
	}

	/*Open input file and count frames / points... */

	if ((lp_ascii = fopen(pp_argv[1], "r")) == NULL)
	{
		fprintf(stderr, "Unable to open ASCII file!\n");
		return 0;
	}
	lv_nframes = 0;
	lv_npoints = 0;

	while (fgets(la_line, MAXLINE, lp_ascii) != NULL)
	{
		if (lv_nframes == 0)
		{
			/* Just count the first line. */
				strtok(la_line, "\t ");
			while (strtok(NULL, "\t "))
			{
				lv_npoints++;
			}
			lv_npoints++;
			lv_npoints = lv_npoints / 3;
		}
		lv_nframes++;
	}

	/* Rewind ascii file and open c3d ready for data */

	rewind(lp_ascii);

	if ((lp_ctd = fopen(pp_argv[2], "w")) == NULL)
	{
		fprintf(stderr, "Unable to create c3d file!\n");
		return 0;
	}
	/* Set necessary fields and write header... */

	lv_header.firstblock = 2;
	lv_header.id = 80;
	lv_header.npoints = lv_npoints;
	lv_header.nmeasures = 0;
	lv_header.firstframe = 1;
	lv_header.lastframe = lv_nframes;
	lv_header.scalefactor = -1;
	lv_header.datastart = 3;
	lv_header.nsamples = 0;
	lv_header.framerate = 60.0;
	lv_header.labelpresent = 0;
	lv_header.labelblock = 0;

	lv_nblocks = 1;
	lv_bytes = sizeof(C3DBlock) * lv_nblocks;
	if (fwrite(&lv_header, lv_bytes, lv_nblocks, lp_ctd) < lv_nblocks)
	{
		fprintf(stderr, "Problems writing header!\n");
		return 0;
	}

	/* Write empty parameters / event section... */

	lv_nblocks = 1;
	lv_bytes = sizeof(C3DBlock) * lv_nblocks;
	if (fwrite(&lv_params, lv_bytes, lv_nblocks, lp_ctd) < lv_nblocks)
	{
		fprintf(stderr, "Problems writing parameter blocks!\n");
		return 0;
	}

	/* Iterate though and write points... */

	for (lv_frame = lv_header.firstframe; lv_frame <= lv_header.lastframe; lv_frame++)
	{

		fgets(la_line, MAXLINE, lp_ascii);


		for (lv_point = 0; lv_point < lv_header.npoints; lv_point++)
		{

			if (lv_point == 0)
				lv_value = atof(strtok(la_line, "\t "));
			else
				lv_value = atof(strtok(NULL, "\t "));
			lv_realXYZ.x = lv_value;
			lv_value = atof(strtok(NULL, "\t "));
			lv_realXYZ.y = lv_value;
			lv_value = atof(strtok(NULL, "\t "));
			lv_realXYZ.z = lv_value;

			/* printf("%f %f %f\n", lv_realXYZ.x, lv_realXYZ.y, lv_realXYZ.z); */

			lv_bytes = sizeof(lv_realXYZ);
			if (!fwrite(&lv_realXYZ, lv_bytes, 1, lp_ctd))
			{
				fprintf(stderr, "Problems writing data!\n");
				return 0;
			}
		}
	}

	/* End... */

	fclose(lp_ctd);
	fclose(lp_ascii);

	return 1;

}
