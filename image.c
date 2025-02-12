/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// image.c -- image loading

#include "quakedef.h"


/*
=================================================================

	IMAGE LOADING

=================================================================
*/

char loadfilename[MAX_OSPATH]; // file scope so that error messages can use it

//==============================================================================
//
//  TGA
//
//==============================================================================

#define TARGAHEADERSIZE 18 // size on disk

typedef struct targaheader_s {
	byte 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	byte	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	byte	pixel_size, attributes;
} targaheader_t;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
=============
Image_LoadTGA
=============
*/
byte *Image_LoadTGA (FILE *f, int *width, int *height)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*rgba_data;
	int				realrow; //johnfitz -- fix for upside-down targas
	qboolean		upside_down; //johnfitz -- fix for upside-down targas
	targaheader_t header;

	header.id_length = fgetc(f);
	header.colormap_type = fgetc(f);
	header.image_type = fgetc(f);

	header.colormap_index = fgetLittleShort(f);
	header.colormap_length = fgetLittleShort(f);
	header.colormap_size = fgetc(f);
	header.x_origin = fgetLittleShort(f);
	header.y_origin = fgetLittleShort(f);
	header.width = fgetLittleShort(f);
	header.height = fgetLittleShort(f);
	header.pixel_size = fgetc(f);
	header.attributes = fgetc(f);

	if (header.image_type!=2 && header.image_type!=10)
		Sys_Error ("Image_LoadTGA: %s is not a type 2 or type 10 targa", loadfilename);

	if (header.colormap_type !=0 || (header.pixel_size!=32 && header.pixel_size!=24))
		Sys_Error ("Image_LoadTGA: %s is not a 24bit or 32bit targa", loadfilename);

	columns = header.width;
	rows = header.height;
	numPixels = columns * rows;
	upside_down = !(header.attributes & 0x20); //johnfitz -- fix for upside-down targas

	rgba_data = Hunk_Alloc (numPixels*4);

	if (header.id_length != 0)
		fseek(f, header.id_length, SEEK_CUR);  // skip TARGA image comment

	if (header.image_type==2) // Uncompressed, RGB images
	{
		for(row=rows-1; row>=0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = rgba_data + realrow*columns*4;
			//johnfitz
			for(column=0; column<columns; column++)
			{
				byte red,green,blue,alphabyte;
				switch (header.pixel_size)
				{
				case 24:
					blue = getc(f);
					green = getc(f);
					red = getc(f);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = getc(f);
					green = getc(f);
					red = getc(f);
					alphabyte = getc(f);
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if (header.image_type==10) // Runlength encoded RGB images
	{
		byte red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--)
		{
			//johnfitz -- fix for upside-down targas
			realrow = upside_down ? row : rows - 1 - row;
			pixbuf = rgba_data + realrow*columns*4;
			//johnfitz
			for(column=0; column<columns; )
			{
				packetHeader=getc(f);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) // run-length packet
				{
					switch (header.pixel_size)
					{
					case 24:
						blue = getc(f);
						green = getc(f);
						red = getc(f);
						alphabyte = 255;
						break;
					case 32:
						blue = getc(f);
						green = getc(f);
						red = getc(f);
						alphabyte = getc(f);
						break;
					default: /* avoid compiler warnings */
						blue = green = red = alphabyte = 0;
					}

					for(j=0;j<packetSize;j++)
					{
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) // run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = rgba_data + realrow*columns*4;
							//johnfitz
						}
					}
				}
				else // non run-length packet
				{
					for(j=0;j<packetSize;j++)
					{
						switch (header.pixel_size)
						{
						case 24:
							blue = getc(f);
							green = getc(f);
							red = getc(f);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = getc(f);
							green = getc(f);
							red = getc(f);
							alphabyte = getc(f);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						default: /* avoid compiler warnings */
							blue = green = red = alphabyte = 0;
						}
						column++;
						if (column==columns) // pixel packet run spans across rows
						{
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							//johnfitz -- fix for upside-down targas
							realrow = upside_down ? row : rows - 1 - row;
							pixbuf = rgba_data + realrow*columns*4;
							//johnfitz
						}
					}
				}
			}
			breakOut:;
		}
	}

	fclose(f);

	*width = (int)(header.width);
	*height = (int)(header.height);

	return rgba_data;
}

//==============================================================================
//
//  PCX
//
//==============================================================================

typedef struct
{
    char			signature;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hdpi,vdpi;
    byte			colortable[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
} pcxheader_t;

/*
============
Image_LoadPCX
============
*/
byte *Image_LoadPCX (FILE *f, int *width, int *height)
{
	pcxheader_t	header;
	int			x, y, w, h, readbyte, runlength, start;
	byte		*p, *rgb_data;
	byte		palette[768];

	start = ftell (f); // save start of file (since we might be inside a pak file, SEEK_SET might not be the start of the pcx)

	fread(&header, sizeof(header), 1, f);
	header.xmin = (unsigned short)LittleShort (header.xmin);
	header.ymin = (unsigned short)LittleShort (header.ymin);
	header.xmax = (unsigned short)LittleShort (header.xmax);
	header.ymax = (unsigned short)LittleShort (header.ymax);
	header.bytes_per_line = (unsigned short)LittleShort (header.bytes_per_line);

	if (header.signature != 0x0A)
		Sys_Error ("Image_LoadPCX: '%s' is not a valid PCX file", loadfilename);

	if (header.version != 5)
		Sys_Error ("Image_LoadPCX: '%s' is version %i, should be 5", loadfilename, header.version);

	if (header.encoding != 1 || header.bits_per_pixel != 8 || header.color_planes != 1)
		Sys_Error ("Image_LoadPCX: '%s' has wrong encoding or bit depth", loadfilename);

	w = header.xmax - header.xmin + 1;
	h = header.ymax - header.ymin + 1;

	rgb_data = Hunk_Alloc ((w*h+1)*4); // +1 to allow reading padding byte on last line

	// load palette
	fseek (f, start + com_filesize - 768, SEEK_SET);
	fread (palette, 1, 768, f);

	// back to start of image data
	fseek (f, start + sizeof(header), SEEK_SET);

	for (y=0; y<h; y++)
	{
		p = rgb_data + y * w * 4;

		for (x=0; x<(header.bytes_per_line); ) // read the extra padding byte if necessary
		{
			readbyte = fgetc(f);

			if(readbyte >= 0xC0)
			{
				runlength = readbyte & 0x3F;
				readbyte = fgetc(f);
			}
			else
				runlength = 1;

			while(runlength--)
			{
				p[0] = palette[readbyte*3];
				p[1] = palette[readbyte*3+1];
				p[2] = palette[readbyte*3+2];
				p[3] = 255;
				p += 4;
				x++;
			}
		}
	}

	fclose(f);

	*width = w;
	*height = h;

	return rgb_data;
}


/*
============
Image_LoadImage

returns a pointer to hunk allocated RGBA data

TODO: search order: tga png jpg pcx lmp
============
*/
byte *Image_LoadImage (char *name, int *width, int *height)
{
	FILE	*f;

	sprintf (loadfilename, "%s.tga", name);
	COM_FOpenFile (loadfilename, &f, NULL);
	if (f)
		return Image_LoadTGA (f, width, height);

	sprintf (loadfilename, "%s.pcx", name);
	COM_FOpenFile (loadfilename, &f, NULL);
	if (f)
		return Image_LoadPCX (f, width, height);

	return NULL;
}

/*
=================================================================

	IMAGE SAVING

=================================================================
*/

//==============================================================================
//
//  Write TARGA
//
//==============================================================================


/*
============
Image_WriteTGA -- writes RGB or RGBA data to a TGA file

returns true if successful

TODO: support BGRA and BGR formats (since opengl can return them, and we don't have to swap)
============
*/
qboolean Image_WriteTGA (char *name, byte *data, int width, int height, int bpp, qboolean upsidedown)
{
	int		handle, i, temp, size, bytes;
	char	pathname[MAX_OSPATH];
	byte	header[TARGAHEADERSIZE];

	sprintf (pathname, "%s/%s", com_gamedir, name);
	handle = Sys_FileOpenWrite (pathname);
	if (handle == -1)
		return false;

	memset (&header, 0, TARGAHEADERSIZE);
	header[2] = 2; // uncompressed type
	header[12] = width&255;
	header[13] = width>>8;
	header[14] = height&255;
	header[15] = height>>8;
	header[16] = bpp; // pixel size
	if (upsidedown)
		header[17] = 0x20; // upside-down attribute

	bytes = bpp/8;
	size = width*height*bytes;
	// swap red and blue bytes
	for (i=0; i<size; i+=bytes)
	{
		temp = data[i];
		data[i] = data[i+2];
		data[i+2] = temp;
	}

	Sys_FileWrite (handle, &header, TARGAHEADERSIZE);
	Sys_FileWrite (handle, data, size);
	Sys_FileClose (handle);

	return true;
}

