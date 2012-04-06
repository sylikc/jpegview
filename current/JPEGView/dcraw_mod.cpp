#include "stdafx.h"

/*
   dcraw.c -- Dave Coffin's raw photo decoder
   Copyright 1997-2008 by Dave Coffin, dcoffin a cybercom o net

   This is a command-line ANSI C program to convert raw photos from
   any digital camera on any computer running any operating system.

   No license is required to download and use dcraw.c.  However,
   to lawfully redistribute dcraw, you must either (a) offer, at
   no extra charge, full source code* for all executable files
   containing RESTRICTED functions, (b) distribute this code under
   the GPL Version 2 or later, (c) remove all RESTRICTED functions,
   re-implement them, or copy them from an earlier, unrestricted
   Revision of dcraw.c, or (d) purchase a license from the author.

   The functions that process Foveon images have been RESTRICTED
   since Revision 1.237.  All other code remains free for all uses.

   *If you have not modified dcraw.c in any way, a link to my
   homepage qualifies as "full source code".

   $Revision: 1.403 $
   $Date: 2008/04/29 18:18:53 $
 */

#define VERSION "8.86"

#define _GNU_SOURCE
#define _USE_MATH_DEFINES
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <direct.h>
#include <sys/types.h>
#include "TJPEGWrapper.h"
#include "dcraw_mod.h"
/*
   NO_JPEG disables decoding of compressed Kodak DC120 files.
   NO_LCMS disables the "-p" option.
 */

#define NO_LCMS
#define DJGPP

#ifndef NO_JPEG
//#include "jpeglib.h"
#endif
#ifndef NO_LCMS
#include <lcms.h>
#endif
#ifdef LOCALEDIR
#include <libintl.h>
#define _(String) gettext(String)
#else
#define _(String) (String)
#endif
#ifdef DJGPP
#define fseeko fseek
#define ftello ftell
#else
#define fgetc getc_unlocked
#endif
#ifdef __CYGWIN__
#include <io.h>
#endif
#ifdef WIN32
#include <sys/utime.h>
//#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
typedef __int64 INT64;
typedef unsigned __int64 UINT64;
#else
#include <unistd.h>
#include <utime.h>
#include <netinet/in.h>
typedef long long INT64;
typedef unsigned long long UINT64;
#endif

#ifdef LJPEG_DECODE
#error Please compile dcraw.c by itself.
#error Do not link it with ljpeg_decode.
#endif

#ifndef LONG_BIT
#define LONG_BIT (8 * sizeof (long))
#endif

#define ushort UshORt
typedef unsigned char uchar;
typedef unsigned short ushort;


#pragma warning(disable:4305 4244 4996 4146 4804)

/*
   All global variables are defined here, and all functions that
   access them are prefixed with "CLASS".  Note that a thread-safe
   C++ class cannot have non-const static local variables.
 */
FILE *ifp;
short order;
char *ifname, *meta_data;
char cdesc[5], desc[512], make[64], model[64], model2[64], artist[64];
float flash_used, canon_ev, iso_speed, shutter, aperture, focal_len;
time_t timestamp;
unsigned shot_order, kodak_cbpp, filters, exif_cfa, unique_id;
off_t    strip_offset, data_offset;
off_t    thumb_offset, meta_offset, profile_offset;
unsigned thumb_length, meta_length, profile_length;
unsigned thumb_misc, *oprof, fuji_layout, shot_select=0, multi_out=0;
unsigned tiff_nifds, tiff_samples, tiff_bps, tiff_compress;
unsigned black, maximum, mix_green, raw_color, use_gamma, zero_is_bad;
unsigned zero_after_ff, is_raw, dng_version, is_foveon, data_error;
unsigned tile_width, tile_length, gpsdata[32];
ushort raw_height, raw_width, height, width, top_margin, left_margin;
ushort shrink, iheight, iwidth, fuji_width, thumb_width, thumb_height;
int flip, tiff_flip, colors;
double pixel_aspect, aber[4]={1,1,1,1};
ushort (*image)[4], white[8][8], cr2_slice[3], sraw_mul[4];
float bright=1, user_mul[4]={0,0,0,0}, threshold=0;
int half_size=0, four_color_rgb=0, document_mode=0, highlight=0;
int verbose=0, use_auto_wb=0, use_camera_wb=0, use_camera_matrix=-1;
int output_color=1, output_bps=8, output_tiff=0, med_passes=0;
int no_auto_bright=0;
unsigned greybox[4] = { 0, 0, UINT_MAX, UINT_MAX };
float cam_mul[4], pre_mul[4], cmatrix[3][4], rgb_cam[3][4];
const double xyz_rgb[3][3] = {			/* XYZ from RGB */
  { 0.412453, 0.357580, 0.180423 },
  { 0.212671, 0.715160, 0.072169 },
  { 0.019334, 0.119193, 0.950227 } };
const float d65_white[3] = { 0.950456, 1, 1.088754 };
//void (*write_thumb)(FILE *), (*write_fun)(FILE *);
void (*write_thumb)(CJPEGImage** Image, bool& bOutOfMemory), (*write_fun)(CJPEGImage** Image, bool& bOutOfMemory);
void (*load_raw)(), (*thumb_load_raw)();


struct decode {
  struct decode *branch[2];
  int leaf;
} first_decode[2048], *second_decode, *free_decode;

struct {
  int width, height, bps, comp, phint, offset, flip, samples, bytes;
} tiff_ifd[10];

struct {
  int format, key_off, black, black_off, split_col, tag_21a;
  float tag_210;
} ph1;

#define CLASS

#define FORC(cnt) for (c=0; c < cnt; c++)
#define FORC3 FORC(3)
#define FORC4 FORC(4)
#define FORCC FORC(colors)

#define SQR(x) ((x)*(x))
#define ABS(x) (((int)(x) ^ ((int)(x) >> 31)) - ((int)(x) >> 31))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LIM(x,min,max) MAX(min,MIN(x,max))
#define ULIM(x,y,z) ((y) < (z) ? LIM(x,y,z) : LIM(x,z,y))
#define CLIP(x) LIM(x,0,65535)
#define SWAP(a,b) { a ^= b; a ^= (b ^= a); }

#ifndef __GLIBC__
char *my_memmem (char *haystack, size_t haystacklen,
		  char *needle, size_t needlelen)
{
  char *c;
  for (c = haystack; c <= haystack + haystacklen - needlelen; c++)
	if (!memcmp (c, needle, needlelen))
	  return c;
  return 0;
}
#define memmem my_memmem
#endif


void CLASS derror()
{
  if (!data_error) {
	fprintf (stderr, "%s: ", ifname);
	if (feof(ifp))
	  fprintf (stderr,_("Unexpected end of file\n"));
	else
	  fprintf (stderr,_("Corrupt data near 0x%llx\n"), (INT64) ftello(ifp));
  }
  data_error = 1;
}

ushort CLASS sget2 (uchar *s)
{
  if (order == 0x4949)		/* "II" means little-endian */
	return s[0] | s[1] << 8;
  else				/* "MM" means big-endian */
	return s[0] << 8 | s[1];
}

ushort CLASS get2()
{
  uchar str[2] = { 0xff,0xff };
  fread (str, 1, 2, ifp);
  return sget2(str);
}

unsigned CLASS sget4 (uchar *s)
{
  if (order == 0x4949)
	return s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
  else
	return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}
#define sget4(s) sget4((uchar *)s)

unsigned CLASS get4()
{
  uchar str[4] = { 0xff,0xff,0xff,0xff };
  fread (str, 1, 4, ifp);
  return sget4(str);
}

unsigned CLASS getint (int type)
{
  return type == 3 ? get2() : get4();
}

float CLASS int_to_float (int i)
{
  union { int i; float f; } u;
  u.i = i;
  return u.f;
}

double CLASS getreal (int type)
{
  union { char c[8]; double d; } u;
  int i, rev;

  switch (type) {
	case 3: return (unsigned short) get2();
	case 4: return (unsigned int) get4();
	case 5:  u.d = (unsigned int) get4();
	  return u.d / (unsigned int) get4();
	case 8: return (signed short) get2();
	case 9: return (signed int) get4();
	case 10: u.d = (signed int) get4();
	  return u.d / (signed int) get4();
	case 11: return int_to_float (get4());
	case 12:
	  rev = 7 * ((order == 0x4949) == (ntohs(0x1234) == 0x1234));
	  for (i=0; i < 8; i++)
	u.c[i ^ rev] = fgetc(ifp);
	  return u.d;
	default: return fgetc(ifp);
  }
}

void CLASS read_shorts (ushort *pixel, int count)
{
  if (fread (pixel, 2, count, ifp) < count) derror();
  if ((order == 0x4949) == (ntohs(0x1234) == 0x1234))
	swab ((char *)pixel, (char *)pixel, count*2);
}


void CLASS canon_600_load_raw()
{
}

int CLASS canon_s2is()
{
  unsigned row;

  for (row=0; row < 100; row++) {
	fseek (ifp, row*3340 + 3284, SEEK_SET);
	if (getc(ifp) > 15) return 1;
  }
  return 0;
}

void CLASS canon_a5_load_raw()
{
}

/*
   getbits(-1) initializes the buffer
   getbits(n) where 0 <= n <= 25 returns an n-bit integer
 */
unsigned CLASS getbits (int nbits)
{
  static unsigned bitbuf=0;
  static int vbits=0, reset=0;
  unsigned c;

  if (nbits == -1)
	return bitbuf = vbits = reset = 0;
  if (nbits == 0 || reset) return 0;
  while (vbits < nbits) {
	if ((c = fgetc(ifp)) == EOF) derror();
	if ((reset = zero_after_ff && c == 0xff && fgetc(ifp))) return 0;
	bitbuf = (bitbuf << 8) + (uchar) c;
	vbits += 8;
  }
  vbits -= nbits;
  return bitbuf << (32-nbits-vbits) >> (32-nbits);
}

void CLASS init_decoder()
{
  memset (first_decode, 0, sizeof first_decode);
  free_decode = first_decode;
}

/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit
   3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is

	{ 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
	  0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },

   then the code is

	00		0x04
	010		0x03
	011		0x05
	100		0x06
	101		0x02
	1100		0x07
	1101		0x01
	11100		0x08
	11101		0x09
	11110		0x00
	111110		0x0a
	1111110		0x0b
	1111111		0xff
 */
uchar * CLASS make_decoder (const uchar *source, int level)
{
  struct decode *cur;
  static int leaf;
  int i, next;

  if (level==0) leaf=0;
  cur = free_decode++;
  if (free_decode > first_decode+2048) {
	fprintf (stderr,_("%s: decoder table overflow\n"), ifname);
	return NULL;
  }
  for (i=next=0; i <= leaf && next < 16; )
	i += source[next++];
  if (i > leaf) {
	if (level < next) {
	  cur->branch[0] = free_decode;
	  make_decoder (source, level+1);
	  cur->branch[1] = free_decode;
	  make_decoder (source, level+1);
	} else
	  cur->leaf = source[16 + leaf++];
  }
  return (uchar *) source + 16 + leaf;
}




void CLASS canon_compressed_load_raw()
{
}

/*
   Not a full implementation of Lossless JPEG, just
   enough to decode Canon, Kodak and Adobe DNG images.
 */
struct jhead {
  int bits, high, wide, clrs, sraw, psv, restart, vpred[4];
  struct CLASS decode *huff[4];
  ushort *row;
};

int CLASS ljpeg_start (struct jhead *jh, int info_only)
{
  int i, tag, len;
  uchar data[0x10000], *dp;

  init_decoder();
  memset (jh, 0, sizeof *jh);
  for (i=0; i < 4; i++)
	jh->huff[i] = free_decode;
  jh->restart = INT_MAX;
  fread (data, 2, 1, ifp);
  if (data[1] != 0xd8) return 0;
  do {
	fread (data, 2, 2, ifp);
	tag =  data[0] << 8 | data[1];
	len = (data[2] << 8 | data[3]) - 2;
	if (tag <= 0xff00) return 0;
	fread (data, 1, len, ifp);
	switch (tag) {
	  case 0xffc3:
	jh->sraw = data[7] == 0x21;
	  case 0xffc0:
	jh->bits = data[0];
	jh->high = data[1] << 8 | data[2];
	jh->wide = data[3] << 8 | data[4];
	jh->clrs = data[5] + jh->sraw;
	if (len == 9 && !dng_version) getc(ifp);
	break;
	  case 0xffc4:
	if (info_only) break;
	for (dp = data; dp < data+len && *dp < 4; ) {
	  jh->huff[*dp] = free_decode;
	  dp = make_decoder (++dp, 0);
	}
	break;
	  case 0xffda:
	jh->psv = data[1+data[0]*2];
	break;
	  case 0xffdd:
	jh->restart = data[0] << 8 | data[1];
	}
  } while (tag != 0xffda);
  if (info_only) return 1;
  if (jh->sraw) {
	jh->huff[3] = jh->huff[2] = jh->huff[1];
	jh->huff[1] = jh->huff[0];
  }
  jh->row = (ushort *) calloc (jh->wide*jh->clrs, 4);
  return zero_after_ff = 1;
}



void CLASS lossless_jpeg_load_raw()
{
}

void CLASS canon_sraw_load_raw()
{
}

void CLASS adobe_dng_load_raw_lj()
{
}

void CLASS adobe_dng_load_raw_nc()
{
}

void CLASS pentax_k10_load_raw()
{
}

void CLASS nikon_compressed_load_raw()
{
}

void CLASS nikon_load_raw()
{
}

/*
   Figure out if a NEF file is compressed.  These fancy heuristics
   are only needed for the D100, thanks to a bug in some cameras
   that tags all images as "compressed".
 */
int CLASS nikon_is_compressed()
{
  uchar test[256];
  int i;

  fseek (ifp, data_offset, SEEK_SET);
  fread (test, 1, 256, ifp);
  for (i=15; i < 256; i+=16)
	if (test[i]) return 1;
  return 0;
}

/*
   Returns 1 for a Coolpix 995, 0 for anything else.
 */
int CLASS nikon_e995()
{
  int i, histo[256];
  const uchar often[] = { 0x00, 0x55, 0xaa, 0xff };

  memset (histo, 0, sizeof histo);
  fseek (ifp, -2000, SEEK_END);
  for (i=0; i < 2000; i++)
	histo[fgetc(ifp)]++;
  for (i=0; i < 4; i++)
	if (histo[often[i]] < 200)
	  return 0;
  return 1;
}

/*
   Returns 1 for a Coolpix 2100, 0 for anything else.
 */
int CLASS nikon_e2100()
{
  uchar t[12];
  int i;

  fseek (ifp, 0, SEEK_SET);
  for (i=0; i < 1024; i++) {
	fread (t, 1, 12, ifp);
	if (((t[2] & t[4] & t[7] & t[9]) >> 4
	& t[1] & t[6] & t[8] & t[11] & 3) != 3)
	  return 0;
  }
  return 1;
}

void CLASS nikon_3700()
{
  int bits, i;
  uchar dp[24];
  static const struct {
	int bits;
	char make[12], model[15];
  } table[] = {
	{ 0x00, "PENTAX",  "Optio 33WR" },
	{ 0x03, "NIKON",   "E3200" },
	{ 0x32, "NIKON",   "E3700" },
	{ 0x33, "OLYMPUS", "C740UZ" } };

  fseek (ifp, 3072, SEEK_SET);
  fread (dp, 1, 24, ifp);
  bits = (dp[8] & 3) << 4 | (dp[20] & 3);
  for (i=0; i < sizeof table / sizeof *table; i++)
	if (bits == table[i].bits) {
	  strcpy (make,  table[i].make );
	  strcpy (model, table[i].model);
	}
}

/*
   Separates a Minolta DiMAGE Z2 from a Nikon E4300.
 */
int CLASS minolta_z2()
{
  int i;
  char tail[424];

  fseek (ifp, -sizeof tail, SEEK_END);
  fread (tail, 1, sizeof tail, ifp);
  for (i=0; i < sizeof tail; i++)
	if (tail[i]) return 1;
  return 0;
}

/* Here raw_width is in bytes, not pixels. */
void CLASS nikon_e900_load_raw()
{
}

void CLASS nikon_e2100_load_raw()
{
}

void CLASS fuji_load_raw()
{
}

//void CLASS jpeg_thumb (FILE *tfp);
void CLASS jpeg_thumb (CJPEGImage** Image, bool& bOutOfMemory);

//void CLASS ppm_thumb (FILE *tfp)
void CLASS ppm_thumb (CJPEGImage** Image, bool& bOutOfMemory)
{
	byte *thumb;
	thumb_length = thumb_width*thumb_height*3;
	thumb = (byte *) malloc (thumb_length);
	fread(thumb, 1, thumb_length, ifp);
	bOutOfMemory = false;

	INT pwidth = RGBWIDTH(thumb_width);
	INT thumb_size = pwidth * thumb_height;
	uint8 *data = new(std::nothrow) uint8[thumb_size];
	if (data == NULL) {
		bOutOfMemory = true;
		free(thumb);
		return;
	} else {
		for (int i = 0; i < thumb_height; i++)
		{
			byte *p = data + i * pwidth + 2;
			byte *pm = p + thumb_width * 3;
			byte *d = thumb + i * pwidth;
			
			while (p < pm)
			{
				*p-- = *d++;
				*p-- = *d++;
				*p = *d++;
				
				p += 5;
			}
		}
	}
	
	free(thumb);

	(*Image) = new CJPEGImage(thumb_width, thumb_height, data, 0, 3, 0, IF_WindowsBMP);
}

//void CLASS layer_thumb (FILE *tfp)
void CLASS layer_thumb (CJPEGImage** Image, bool& bOutOfMemory)
{
/*
  int i, c;
  char *thumb, map[][4] = { "012","102" };

  colors = thumb_misc >> 5 & 7;
  thumb_length = thumb_width*thumb_height;
  thumb = (char *) calloc (colors, thumb_length);
  fprintf (tfp, "P%d\n%d %d\n255\n",
	5 + (colors >> 1), thumb_width, thumb_height);
  fread (thumb, thumb_length, colors, ifp);
  for (i=0; i < thumb_length; i++)
	FORCC putc (thumb[i+thumb_length*(map[thumb_misc >> 8][c]-'0')], tfp);
  free (thumb);
  */
}

//void CLASS rollei_thumb (FILE *tfp)
void CLASS rollei_thumb (CJPEGImage** Image, bool& bOutOfMemory)
{
/*
  unsigned i;
  ushort *thumb;

  thumb_length = thumb_width * thumb_height;
  thumb = (ushort *) calloc (thumb_length, 2);
  fprintf (tfp, "P6\n%d %d\n255\n", thumb_width, thumb_height);
  read_shorts (thumb, thumb_length);
  for (i=0; i < thumb_length; i++) {
	putc (thumb[i] << 3, tfp);
	putc (thumb[i] >> 5  << 2, tfp);
	putc (thumb[i] >> 11 << 3, tfp);
  }
  free (thumb);
  */
}

void CLASS rollei_load_raw()
{
}

void CLASS phase_one_load_raw()
{
}

void CLASS phase_one_load_raw_c()
{
}

void CLASS eight_bit_load_raw() 
{
}

void CLASS hasselblad_load_raw()
{
}

void CLASS leaf_hdr_load_raw()
{
}

void CLASS kodak_262_load_raw()
{
}

void CLASS unpacked_load_raw();

void CLASS sinar_4shot_load_raw()
{
}

void CLASS imacon_full_load_raw()
{
}

void CLASS packed_12_load_raw()
{
}

void CLASS unpacked_load_raw()
{
}

void CLASS nokia_load_raw()
{
}

void CLASS panasonic_load_raw()
{
}

void CLASS olympus_e300_load_raw()
{
}

void CLASS olympus_e410_load_raw()
{
}

void CLASS olympus_cseries_load_raw()
{
}

void CLASS minolta_rd175_load_raw()
{
}

void CLASS casio_qv5700_load_raw()
{
}

void CLASS quicktake_100_load_raw()
{
}

void CLASS kodak_radc_load_raw()
{
}


void CLASS kodak_jpeg_load_raw()
{
}

void CLASS kodak_dc120_load_raw()
{
}

void CLASS kodak_65000_load_raw()
{
}

void CLASS kodak_ycbcr_load_raw()
{
}

void CLASS kodak_rgb_load_raw()
{
}

void CLASS kodak_thumb_load_raw()
{
}

void CLASS sony_decrypt (unsigned *data, int len, int start, int key)
{
  static unsigned pad[128], p;

  if (start) {
	for (p=0; p < 4; p++)
	  pad[p] = key = key * 48828125 + 1;
	pad[3] = pad[3] << 1 | (pad[0]^pad[2]) >> 31;
	for (p=4; p < 127; p++)
	  pad[p] = (pad[p-4]^pad[p-2]) << 1 | (pad[p-3]^pad[p-1]) >> 31;
	for (p=0; p < 127; p++)
	  pad[p] = htonl(pad[p]);
  }
  while (len--)
	*data++ ^= pad[p++ & 127] = pad[(p+1) & 127] ^ pad[(p+65) & 127];
}

void CLASS sony_load_raw()
{
}

void CLASS sony_arw_load_raw()
{
}

void CLASS sony_arw2_load_raw()
{
}

void CLASS smal_v6_load_raw()
{
}

void CLASS smal_v9_load_raw()
{
}

/* RESTRICTED code starts here */

void CLASS foveon_thumb (CJPEGImage** Image, bool& bOutOfMemory)
{
}

void CLASS foveon_load_raw()
{
}

/* RESTRICTED code ends here */


void CLASS pseudoinverse (double (*in)[3], double (*out)[3], int size)
{
  double work[3][6], num;
  int i, j, k;

  for (i=0; i < 3; i++) {
	for (j=0; j < 6; j++)
	  work[i][j] = j == i+3;
	for (j=0; j < 3; j++)
	  for (k=0; k < size; k++)
	work[i][j] += in[k][i] * in[k][j];
  }
  for (i=0; i < 3; i++) {
	num = work[i][i];
	for (j=0; j < 6; j++)
	  work[i][j] /= num;
	for (k=0; k < 3; k++) {
	  if (k==i) continue;
	  num = work[k][i];
	  for (j=0; j < 6; j++)
	work[k][j] -= work[i][j] * num;
	}
  }
  for (i=0; i < size; i++)
	for (j=0; j < 3; j++)
	  for (out[i][j]=k=0; k < 3; k++)
	out[i][j] += work[j][k+3] * in[i][k];
}

void CLASS cam_xyz_coeff (double cam_xyz[4][3])
{
  double cam_rgb[4][3], inverse[4][3], num;
  int i, j, k;

  for (i=0; i < colors; i++)		/* Multiply out XYZ colorspace */
	for (j=0; j < 3; j++)
	  for (cam_rgb[i][j] = k=0; k < 3; k++)
	cam_rgb[i][j] += cam_xyz[i][k] * xyz_rgb[k][j];

  for (i=0; i < colors; i++) {		/* Normalize cam_rgb so that */
	for (num=j=0; j < 3; j++)		/* cam_rgb * (1,1,1) is (1,1,1,1) */
	  num += cam_rgb[i][j];
	for (j=0; j < 3; j++)
	  cam_rgb[i][j] /= num;
	pre_mul[i] = 1 / num;
  }
  pseudoinverse (cam_rgb, inverse, colors);
  for (raw_color = i=0; i < 3; i++)
	for (j=0; j < colors; j++)
	  rgb_cam[i][j] = inverse[j][i];
}

void CLASS tiff_get (unsigned base,
	unsigned *tag, unsigned *type, unsigned *len, unsigned *save)
{
  *tag  = get2();
  *type = get2();
  *len  = get4();
  *save = ftell(ifp) + 4;
  if (*len * ("11124811248488"[*type < 14 ? *type:0]-'0') > 4)
	fseek (ifp, get4()+base, SEEK_SET);
}

void CLASS parse_thumb_note (int base, unsigned toff, unsigned tlen)
{
  unsigned entries, tag, type, len, save;

  entries = get2();
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	if (tag == toff) thumb_offset = get4()+base;
	if (tag == tlen) thumb_length = get4();
	fseek (ifp, save, SEEK_SET);
  }
}

int CLASS parse_tiff_ifd (int base);

void CLASS parse_makernote (int base, int uptag)
{
  static const uchar xlat[2][256] = {
  { 0xc1,0xbf,0x6d,0x0d,0x59,0xc5,0x13,0x9d,0x83,0x61,0x6b,0x4f,0xc7,0x7f,0x3d,0x3d,
	0x53,0x59,0xe3,0xc7,0xe9,0x2f,0x95,0xa7,0x95,0x1f,0xdf,0x7f,0x2b,0x29,0xc7,0x0d,
	0xdf,0x07,0xef,0x71,0x89,0x3d,0x13,0x3d,0x3b,0x13,0xfb,0x0d,0x89,0xc1,0x65,0x1f,
	0xb3,0x0d,0x6b,0x29,0xe3,0xfb,0xef,0xa3,0x6b,0x47,0x7f,0x95,0x35,0xa7,0x47,0x4f,
	0xc7,0xf1,0x59,0x95,0x35,0x11,0x29,0x61,0xf1,0x3d,0xb3,0x2b,0x0d,0x43,0x89,0xc1,
	0x9d,0x9d,0x89,0x65,0xf1,0xe9,0xdf,0xbf,0x3d,0x7f,0x53,0x97,0xe5,0xe9,0x95,0x17,
	0x1d,0x3d,0x8b,0xfb,0xc7,0xe3,0x67,0xa7,0x07,0xf1,0x71,0xa7,0x53,0xb5,0x29,0x89,
	0xe5,0x2b,0xa7,0x17,0x29,0xe9,0x4f,0xc5,0x65,0x6d,0x6b,0xef,0x0d,0x89,0x49,0x2f,
	0xb3,0x43,0x53,0x65,0x1d,0x49,0xa3,0x13,0x89,0x59,0xef,0x6b,0xef,0x65,0x1d,0x0b,
	0x59,0x13,0xe3,0x4f,0x9d,0xb3,0x29,0x43,0x2b,0x07,0x1d,0x95,0x59,0x59,0x47,0xfb,
	0xe5,0xe9,0x61,0x47,0x2f,0x35,0x7f,0x17,0x7f,0xef,0x7f,0x95,0x95,0x71,0xd3,0xa3,
	0x0b,0x71,0xa3,0xad,0x0b,0x3b,0xb5,0xfb,0xa3,0xbf,0x4f,0x83,0x1d,0xad,0xe9,0x2f,
	0x71,0x65,0xa3,0xe5,0x07,0x35,0x3d,0x0d,0xb5,0xe9,0xe5,0x47,0x3b,0x9d,0xef,0x35,
	0xa3,0xbf,0xb3,0xdf,0x53,0xd3,0x97,0x53,0x49,0x71,0x07,0x35,0x61,0x71,0x2f,0x43,
	0x2f,0x11,0xdf,0x17,0x97,0xfb,0x95,0x3b,0x7f,0x6b,0xd3,0x25,0xbf,0xad,0xc7,0xc5,
	0xc5,0xb5,0x8b,0xef,0x2f,0xd3,0x07,0x6b,0x25,0x49,0x95,0x25,0x49,0x6d,0x71,0xc7 },
  { 0xa7,0xbc,0xc9,0xad,0x91,0xdf,0x85,0xe5,0xd4,0x78,0xd5,0x17,0x46,0x7c,0x29,0x4c,
	0x4d,0x03,0xe9,0x25,0x68,0x11,0x86,0xb3,0xbd,0xf7,0x6f,0x61,0x22,0xa2,0x26,0x34,
	0x2a,0xbe,0x1e,0x46,0x14,0x68,0x9d,0x44,0x18,0xc2,0x40,0xf4,0x7e,0x5f,0x1b,0xad,
	0x0b,0x94,0xb6,0x67,0xb4,0x0b,0xe1,0xea,0x95,0x9c,0x66,0xdc,0xe7,0x5d,0x6c,0x05,
	0xda,0xd5,0xdf,0x7a,0xef,0xf6,0xdb,0x1f,0x82,0x4c,0xc0,0x68,0x47,0xa1,0xbd,0xee,
	0x39,0x50,0x56,0x4a,0xdd,0xdf,0xa5,0xf8,0xc6,0xda,0xca,0x90,0xca,0x01,0x42,0x9d,
	0x8b,0x0c,0x73,0x43,0x75,0x05,0x94,0xde,0x24,0xb3,0x80,0x34,0xe5,0x2c,0xdc,0x9b,
	0x3f,0xca,0x33,0x45,0xd0,0xdb,0x5f,0xf5,0x52,0xc3,0x21,0xda,0xe2,0x22,0x72,0x6b,
	0x3e,0xd0,0x5b,0xa8,0x87,0x8c,0x06,0x5d,0x0f,0xdd,0x09,0x19,0x93,0xd0,0xb9,0xfc,
	0x8b,0x0f,0x84,0x60,0x33,0x1c,0x9b,0x45,0xf1,0xf0,0xa3,0x94,0x3a,0x12,0x77,0x33,
	0x4d,0x44,0x78,0x28,0x3c,0x9e,0xfd,0x65,0x57,0x16,0x94,0x6b,0xfb,0x59,0xd0,0xc8,
	0x22,0x36,0xdb,0xd2,0x63,0x98,0x43,0xa1,0x04,0x87,0x86,0xf7,0xa6,0x26,0xbb,0xd6,
	0x59,0x4d,0xbf,0x6a,0x2e,0xaa,0x2b,0xef,0xe6,0x78,0xb6,0x4e,0xe0,0x2f,0xdc,0x7c,
	0xbe,0x57,0x19,0x32,0x7e,0x2a,0xd0,0xb8,0xba,0x29,0x00,0x3c,0x52,0x7d,0xa8,0x49,
	0x3b,0x2d,0xeb,0x25,0x49,0xfa,0xa3,0xaa,0x39,0xa7,0xc5,0xa7,0x50,0x11,0x36,0xfb,
	0xc6,0x67,0x4a,0xf5,0xa5,0x12,0x65,0x7e,0xb0,0xdf,0xaf,0x4e,0xb3,0x61,0x7f,0x2f } };
  unsigned offset=0, entries, tag, type, len, save, c;
  unsigned ver97=0, serial=0, i, wbi=0, wb[4]={0,0,0,0};
  uchar buf97[324], ci, cj, ck;
  short sorder=order;
  char buf[10];
/*
   The MakerNote might have its own TIFF header (possibly with
   its own byte-order!), or it might just be a table.
 */
  fread (buf, 1, 10, ifp);
  if (!strncmp (buf,"KDK" ,3) ||	/* these aren't TIFF tables */
	  !strncmp (buf,"VER" ,3) ||
	  !strncmp (buf,"IIII",4) ||
	  !strncmp (buf,"MMMM",4)) return;
  if (!strncmp (buf,"KC"  ,2) ||	/* Konica KD-400Z, KD-510Z */
	  !strncmp (buf,"MLY" ,3)) {	/* Minolta DiMAGE G series */
	order = 0x4d4d;
	while ((i=ftell(ifp)) < data_offset && i < 16384) {
	  wb[0] = wb[2];  wb[2] = wb[1];  wb[1] = wb[3];
	  wb[3] = get2();
	  if (wb[1] == 256 && wb[3] == 256 &&
	  wb[0] > 256 && wb[0] < 640 && wb[2] > 256 && wb[2] < 640)
	FORC4 cam_mul[c] = wb[c];
	}
	goto quit;
  }
  if (!strcmp (buf,"Nikon")) {
	base = ftell(ifp);
	order = get2();
	if (get2() != 42) goto quit;
	offset = get4();
	fseek (ifp, offset-8, SEEK_CUR);
  } else if (!strcmp (buf,"OLYMPUS")) {
	base = ftell(ifp)-10;
	fseek (ifp, -2, SEEK_CUR);
	order = get2();  get2();
  } else if (!strncmp (buf,"FUJIFILM",8) ||
		 !strncmp (buf,"SONY",4) ||
		 !strcmp  (buf,"Panasonic")) {
	order = 0x4949;
	fseek (ifp,  2, SEEK_CUR);
  } else if (!strcmp (buf,"OLYMP") ||
		 !strcmp (buf,"LEICA") ||
		 !strcmp (buf,"Ricoh") ||
		 !strcmp (buf,"EPSON"))
	fseek (ifp, -2, SEEK_CUR);
  else if (!strcmp (buf,"AOC") ||
	   !strcmp (buf,"QVC"))
	fseek (ifp, -4, SEEK_CUR);
  else fseek (ifp, -10, SEEK_CUR);

  entries = get2();
  if (entries > 1000) return;
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	tag |= uptag << 16;
	if (tag == 2 && strstr(make,"NIKON"))
	  iso_speed = (get2(),get2());
	if (tag == 4 && len > 26 && len < 35) {
	  iso_speed = 50 * pow (2, (get4(),get2())/32.0 - 4);
	  if ((i=(get2(),get2())) != 0x7fff)
	aperture = pow (2, i/64.0);
	  if ((i=get2()) != 0xffff)
	shutter = pow (2, (short) i/-32.0);
	  wbi = (get2(),get2());
	  shot_order = (get2(),get2());
	}
	if (tag == 8 && type == 4)
	  shot_order = get4();
	if (tag == 9 && !strcmp(make,"Canon"))
	  fread (artist, 64, 1, ifp);
	if (tag == 0xc && len == 4) {
	  cam_mul[0] = getreal(type);
	  cam_mul[2] = getreal(type);
	}
	if (tag == 0x10 && type == 4)
	  unique_id = get4();
	if (tag == 0x11 && is_raw && !strncmp(make,"NIKON",5)) {
	  fseek (ifp, get4()+base, SEEK_SET);
	  parse_tiff_ifd (base);
	}
	if (tag == 0x14 && len == 2560 && type == 7) {
	  fseek (ifp, 1248, SEEK_CUR);
	  goto get2_256;
	}
	if (tag == 0x15 && type == 2 && is_raw)
	  fread (model, 64, 1, ifp);
	if (strstr(make,"PENTAX")) {
	  if (tag == 0x1b) tag = 0x1018;
	  if (tag == 0x1c) tag = 0x1017;
	}
	if (tag == 0x1d)
	  while ((c = fgetc(ifp)) && c != EOF)
	serial = serial*10 + (isdigit(c) ? c - '0' : c % 10);
	if (tag == 0x81 && type == 4) {
	  data_offset = get4();
	  fseek (ifp, data_offset + 41, SEEK_SET);
	  raw_height = get2() * 2;
	  raw_width  = get2();
	  filters = 0x61616161;
	}
	if (tag == 0x29 && type == 1) {
	  c = wbi < 18 ? "012347800000005896"[wbi]-'0' : 0;
	  fseek (ifp, 8 + c*32, SEEK_CUR);
	  FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get4();
	}
	if ((tag == 0x81  && type == 7) ||
	(tag == 0x100 && type == 7) ||
	(tag == 0x280 && type == 1)) {
	  thumb_offset = ftell(ifp);
	  thumb_length = len;
	}
	if (tag == 0x88 && type == 4 && (thumb_offset = get4()))
	  thumb_offset += base;
	if (tag == 0x89 && type == 4)
	  thumb_length = get4();
	if (tag == 0x8c || tag == 0x96)
	  meta_offset = ftell(ifp);
	if (tag == 0x97) {
	  for (i=0; i < 4; i++)
	ver97 = (ver97 << 4) + fgetc(ifp)-'0';
	  switch (ver97) {
	case 0x100:
	  fseek (ifp, 68, SEEK_CUR);
	  FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
	  break;
	case 0x102:
	  fseek (ifp, 6, SEEK_CUR);
	  goto get2_rggb;
	case 0x103:
	  fseek (ifp, 16, SEEK_CUR);
	  FORC4 cam_mul[c] = get2();
	  }
	  if (ver97 >> 8 == 2) {
	if (ver97 != 0x205) fseek (ifp, 280, SEEK_CUR);
	fread (buf97, 324, 1, ifp);
	  }
	}
	if (tag == 0xa4 && type == 3) {
	  fseek (ifp, wbi*48, SEEK_CUR);
	  FORC3 cam_mul[c] = get2();
	}
	if (tag == 0xa7 && ver97 >> 8 == 2) {
	  ci = xlat[0][serial & 0xff];
	  cj = xlat[1][fgetc(ifp)^fgetc(ifp)^fgetc(ifp)^fgetc(ifp)];
	  ck = 0x60;
	  for (i=0; i < 324; i++)
	buf97[i] ^= (cj += ci * ck++);
	  FORC4 cam_mul[c ^ (c >> 1)] =
	sget2 (buf97 + (ver97 == 0x205 ? 14:6) + c*2);
	  if (ver97 == 0x209)
	FORC4 cam_mul[c ^ (c >> 1) ^ 1] =
	  sget2 (buf97 + 10 + c*2);
	}
	if (tag == 0x200 && len == 3)
	  shot_order = (get4(),get4());
	if (tag == 0x200 && len == 4)
	  black = (get2()+get2()+get2()+get2())/4;
	if (tag == 0x201 && len == 4)
	  goto get2_rggb;
	if (tag == 0x401 && len == 4) {
	  black = (get4()+get4()+get4()+get4())/4;
	}
	if (tag == 0xe01) {		/* Nikon Capture Note */
	  type = order;
	  order = 0x4949;
	  fseek (ifp, 22, SEEK_CUR);
	  for (offset=22; offset+22 < len; offset += 22+i) {
	tag = get4();
	fseek (ifp, 14, SEEK_CUR);
	i = get4()-4;
	if (tag == 0x76a43207) flip = get2();
	else fseek (ifp, i, SEEK_CUR);
	  }
	  order = type;
	}
	if (tag == 0xe80 && len == 256 && type == 7) {
	  fseek (ifp, 48, SEEK_CUR);
	  cam_mul[0] = get2() * 508 * 1.078 / 0x10000;
	  cam_mul[2] = get2() * 382 * 1.173 / 0x10000;
	}
	if (tag == 0xf00 && type == 7) {
	  if (len == 614)
	fseek (ifp, 176, SEEK_CUR);
	  else if (len == 734 || len == 1502)
	fseek (ifp, 148, SEEK_CUR);
	  else goto next;
	  goto get2_256;
	}
	if ((tag == 0x1011 && len == 9) || tag == 0x20400200)
	  for (i=0; i < 3; i++)
	FORC3 cmatrix[i][c] = ((short) get2()) / 256.0;
	if ((tag == 0x1012 || tag == 0x20400600) && len == 4)
	  for (black = i=0; i < 4; i++)
	black += get2() << 2;
	if (tag == 0x1017 || tag == 0x20400100)
	  cam_mul[0] = get2() / 256.0;
	if (tag == 0x1018 || tag == 0x20400100)
	  cam_mul[2] = get2() / 256.0;
	if (tag == 0x2011 && len == 2) {
get2_256:
	  order = 0x4d4d;
	  cam_mul[0] = get2() / 256.0;
	  cam_mul[2] = get2() / 256.0;
	}
	if (tag == 0x2010 && type == 13)
	  load_raw = &CLASS olympus_e410_load_raw;
	if (tag == 0x2020)
	  parse_thumb_note (base, 257, 258);
	if (tag == 0x2040)
	  parse_makernote (base, 0x2040);
	if (tag == 0xb028) {
	  fseek (ifp, get4(), SEEK_SET);
	  parse_thumb_note (base, 136, 137);
	}
	if (tag == 0x4001 && type == 3) {
	  i = len == 582 ? 50 : len == 653 ? 68 : 126;
	  fseek (ifp, i, SEEK_CUR);
get2_rggb:
	  FORC4 cam_mul[c ^ (c >> 1)] = get2();
	  fseek (ifp, 22, SEEK_CUR);
	  FORC4 sraw_mul[c ^ (c >> 1)] = get2();
	}
next:
	fseek (ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
}

/*
   Since the TIFF DateTime string has no timezone information,
   assume that the camera's clock was set to Universal Time.
 */
void CLASS get_timestamp (int reversed)
{
  struct tm t;
  char str[20];
  int i;

  str[19] = 0;
  if (reversed)
	for (i=19; i--; ) str[i] = fgetc(ifp);
  else
	fread (str, 19, 1, ifp);
  memset (&t, 0, sizeof t);
  if (sscanf (str, "%d:%d:%d %d:%d:%d", &t.tm_year, &t.tm_mon,
	&t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
	return;
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  if (mktime(&t) > 0)
	timestamp = mktime(&t);
}

void CLASS parse_exif (int base)
{
  unsigned kodak, entries, tag, type, len, save, c;
  double expo;

  kodak = !strncmp(make,"EASTMAN",7);
  entries = get2();
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	switch (tag) {
	  case 33434:  shutter = getreal(type);		break;
	  case 33437:  aperture = getreal(type);		break;
	  case 34855:  iso_speed = get2();			break;
	  case 36867:
	  case 36868:  get_timestamp(0);			break;
	  case 37377:  if ((expo = -getreal(type)) < 128)
			 shutter = pow (2, expo);		break;
	  case 37378:  aperture = pow (2, getreal(type)/2);	break;
	  case 37386:  focal_len = getreal(type);		break;
	  case 37500:  parse_makernote (base, 0);		break;
	  case 40962:  if (kodak) raw_width  = get4();	break;
	  case 40963:  if (kodak) raw_height = get4();	break;
	  case 41730:
	if (get4() == 0x20002)
	  for (exif_cfa=c=0; c < 8; c+=2)
		exif_cfa |= fgetc(ifp) * 0x01010101 << c;
	}
	fseek (ifp, save, SEEK_SET);
  }
}

void CLASS parse_gps (int base)
{
  unsigned entries, tag, type, len, save, c;

  entries = get2();
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	switch (tag) {
	  case 1: case 3: case 5:
	gpsdata[29+tag/2] = getc(ifp);			break;
	  case 2: case 4: case 7:
	FORC(6) gpsdata[tag/3*6+c] = get4();		break;
	  case 6:
	FORC(2) gpsdata[18+c] = get4();			break;
	  case 18: case 29:
	fgets ((char *) (gpsdata+14+tag/3), MIN(len,12), ifp);
	}
	fseek (ifp, save, SEEK_SET);
  }
}

void CLASS romm_coeff (float romm_cam[3][3])
{
  static const float rgb_romm[3][3] =	/* ROMM == Kodak ProPhoto */
  { {  2.034193, -0.727420, -0.306766 },
	{ -0.228811,  1.231729, -0.002922 },
	{ -0.008565, -0.153273,  1.161839 } };
  int i, j, k;

  for (i=0; i < 3; i++)
	for (j=0; j < 3; j++)
	  for (cmatrix[i][j] = k=0; k < 3; k++)
	cmatrix[i][j] += rgb_romm[i][k] * romm_cam[k][j];
}

void CLASS parse_mos (int offset)
{
  char data[40];
  int skip, from, i, c, neut[4], planes=0, frot=0;
  static const char *mod[] =
  { "","DCB2","Volare","Cantare","CMost","Valeo 6","Valeo 11","Valeo 22",
	"Valeo 11p","Valeo 17","","Aptus 17","Aptus 22","Aptus 75","Aptus 65",
	"Aptus 54S","Aptus 65S","Aptus 75S" };
  float romm_cam[3][3];

  fseek (ifp, offset, SEEK_SET);
  while (1) {
	if (get4() != 0x504b5453) break;
	get4();
	fread (data, 1, 40, ifp);
	skip = get4();
	from = ftell(ifp);
	if (!strcmp(data,"JPEG_preview_data")) {
	  thumb_offset = from;
	  thumb_length = skip;
	}
	if (!strcmp(data,"icc_camera_profile")) {
	  profile_offset = from;
	  profile_length = skip;
	}
	if (!strcmp(data,"ShootObj_back_type")) {
	  fscanf (ifp, "%d", &i);
	  if ((unsigned) i < sizeof mod / sizeof (*mod))
	strcpy (model, mod[i]);
	}
	if (!strcmp(data,"icc_camera_to_tone_matrix")) {
	  for (i=0; i < 9; i++)
	romm_cam[0][i] = int_to_float(get4());
	  romm_coeff (romm_cam);
	}
	if (!strcmp(data,"CaptProf_color_matrix")) {
	  for (i=0; i < 9; i++)
	fscanf (ifp, "%f", &romm_cam[0][i]);
	  romm_coeff (romm_cam);
	}
	if (!strcmp(data,"CaptProf_number_of_planes"))
	  fscanf (ifp, "%d", &planes);
	if (!strcmp(data,"CaptProf_raw_data_rotation"))
	  fscanf (ifp, "%d", &flip);
	if (!strcmp(data,"CaptProf_mosaic_pattern"))
	  FORC4 {
	fscanf (ifp, "%d", &i);
	if (i == 1) frot = c ^ (c >> 1);
	  }
	if (!strcmp(data,"ImgProf_rotation_angle")) {
	  fscanf (ifp, "%d", &i);
	  flip = i - flip;
	}
	if (!strcmp(data,"NeutObj_neutrals") && !cam_mul[0]) {
	  FORC4 fscanf (ifp, "%d", neut+c);
	  FORC3 cam_mul[c] = (float) neut[0] / neut[c+1];
	}
	parse_mos (from);
	fseek (ifp, skip+from, SEEK_SET);
  }
  if (planes)
	filters = (planes == 1) * 0x01010101 *
	(uchar) "\x94\x61\x16\x49"[(flip/90 + frot) & 3];
}

void CLASS linear_table (unsigned len)
{
}

void CLASS parse_kodak_ifd (int base)
{
  unsigned entries, tag, type, len, save;
  int i, c, wbi=-2, wbtemp=6500;
  float mul[3], num;

  entries = get2();
  if (entries > 1024) return;
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	if (tag == 1020) wbi = getint(type);
	if (tag == 1021 && len == 72) {		/* WB set in software */
	  fseek (ifp, 40, SEEK_CUR);
	  FORC3 cam_mul[c] = 2048.0 / get2();
	  wbi = -2;
	}
	if (tag == 2118) wbtemp = getint(type);
	if (tag == 2130 + wbi)
	  FORC3 mul[c] = getreal(type);
	if (tag == 2140 + wbi && wbi >= 0)
	  FORC3 {
	for (num=i=0; i < 4; i++)
	  num += getreal(type) * pow (wbtemp/100.0, i);
	cam_mul[c] = 2048 / (num * mul[c]);
	  }
	if (tag == 2317) linear_table (len);
	if (tag == 6020) iso_speed = getint(type);
	fseek (ifp, save, SEEK_SET);
  }
}

void CLASS parse_minolta (int base);

int CLASS parse_tiff_ifd (int base)
{
  unsigned entries, tag, type, len, plen=16, save;
  int ifd, use_cm=0, cfa, i, j, c, ima_len=0;
  char software[64], *cbuf, *cp;
  uchar cfa_pat[16], cfa_pc[] = { 0,1,2,3 }, tab[256];
  double dblack, cc[4][4], cm[4][3], cam_xyz[4][3], num;
  double ab[]={ 1,1,1,1 }, asn[] = { 0,0,0,0 }, xyz[] = { 1,1,1 };
  unsigned sony_curve[] = { 0,0,0,0,0,4095 };
  unsigned *buf, sony_offset=0, sony_length=0, sony_key=0;
  struct jhead jh;
  FILE *sfp;

  if (tiff_nifds >= sizeof tiff_ifd / sizeof tiff_ifd[0])
	return 1;
  ifd = tiff_nifds++;
  for (j=0; j < 4; j++)
	for (i=0; i < 4; i++)
	  cc[j][i] = i == j;
  entries = get2();
  if (entries > 512) return 1;
  while (entries--) {
	tiff_get (base, &tag, &type, &len, &save);
	switch (tag) {
	  case 17: case 18:
	if (type == 3 && len == 1)
	  cam_mul[(tag-17)*2] = get2() / 256.0;
	break;
	  case 23:
	if (type == 3) iso_speed = get2();
	break;
	  case 36: case 37: case 38:
	cam_mul[tag-0x24] = get2();
	break;
	  case 39:
	if (len < 50 || cam_mul[0]) break;
	fseek (ifp, 12, SEEK_CUR);
	FORC3 cam_mul[c] = get2();
	break;
	  case 46:
	if (type != 7 || fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8) break;
	thumb_offset = ftell(ifp) - 2;
	thumb_length = len;
	break;
	  case 2: case 256:			/* ImageWidth */
	tiff_ifd[ifd].width = getint(type);
	break;
	  case 3: case 257:			/* ImageHeight */
	tiff_ifd[ifd].height = getint(type);
	break;
	  case 258:				/* BitsPerSample */
	tiff_ifd[ifd].samples = len & 7;
	tiff_ifd[ifd].bps = get2();
	break;
	  case 259:				/* Compression */
	tiff_ifd[ifd].comp = get2();
	break;
	  case 262:				/* PhotometricInterpretation */
	tiff_ifd[ifd].phint = get2();
	break;
	  case 270:				/* ImageDescription */
	fread (desc, 512, 1, ifp);
	break;
	  case 271:				/* Make */
	fgets (make, 64, ifp);
	break;
	  case 272:				/* Model */
	fgets (model, 64, ifp);
	break;
	  case 273:				/* StripOffset */
	  case 513:
	tiff_ifd[ifd].offset = get4()+base;
	if (!tiff_ifd[ifd].bps) {
	  fseek (ifp, tiff_ifd[ifd].offset, SEEK_SET);
	  if (ljpeg_start (&jh, 1)) {
		tiff_ifd[ifd].comp    = 6;
		tiff_ifd[ifd].width   = jh.wide << (jh.clrs == 2);
		tiff_ifd[ifd].height  = jh.high;
		tiff_ifd[ifd].bps     = jh.bits;
		tiff_ifd[ifd].samples = jh.clrs;
	  }
	}
	break;
	  case 274:				/* Orientation */
	tiff_ifd[ifd].flip = "50132467"[get2() & 7]-'0';
	break;
	  case 277:				/* SamplesPerPixel */
	tiff_ifd[ifd].samples = getint(type) & 7;
	break;
	  case 279:				/* StripByteCounts */
	  case 514:
	tiff_ifd[ifd].bytes = get4();
	break;
	  case 305:				/* Software */
	fgets (software, 64, ifp);
	if (!strncmp(software,"Adobe",5) ||
		!strncmp(software,"dcraw",5) ||
		!strncmp(software,"Bibble",6) ||
		!strncmp(software,"Nikon Scan",10) ||
		!strcmp (software,"Digital Photo Professional"))
	  is_raw = 0;
	break;
	  case 306:				/* DateTime */
	get_timestamp(0);
	break;
	  case 315:				/* Artist */
	fread (artist, 64, 1, ifp);
	break;
	  case 322:				/* TileWidth */
	tile_width = getint(type);
	break;
	  case 323:				/* TileLength */
	tile_length = getint(type);
	break;
	  case 324:				/* TileOffsets */
	tiff_ifd[ifd].offset = len > 1 ? ftell(ifp) : get4();
	if (len == 4) {
	  load_raw = &CLASS sinar_4shot_load_raw;
	  is_raw = 5;
	}
	break;
	  case 330:				/* SubIFDs */
	if (!strcmp(model,"DSLR-A100") && tiff_ifd[ifd].width == 3872) {
	  load_raw = &CLASS sony_arw_load_raw;
	  data_offset = get4()+base;
	  ifd++;  break;
	}
	while (len--) {
	  i = ftell(ifp);
	  fseek (ifp, get4()+base, SEEK_SET);
	  if (parse_tiff_ifd (base)) break;
	  fseek (ifp, i+4, SEEK_SET);
	}
	break;
	  case 400:
	strcpy (make, "Sarnoff");
	maximum = 0xfff;
	break;
	  case 28688:
	FORC4 sony_curve[c+1] = get2() >> 2 & 0xfff;
	break;
	  case 29184: sony_offset = get4();  break;
	  case 29185: sony_length = get4();  break;
	  case 29217: sony_key    = get4();  break;
	  case 29264:
	parse_minolta (ftell(ifp));
	raw_width = 0;
	break;
	  case 29443:
	FORC4 cam_mul[c ^ (c < 2)] = get2();
	break;
	  case 33405:			/* Model2 */
	fgets (model2, 64, ifp);
	break;
	  case 33422:			/* CFAPattern */
	  case 64777:			/* Kodak P-series */
	if ((plen=len) > 16) plen = 16;
	fread (cfa_pat, 1, plen, ifp);
	for (colors=cfa=i=0; i < plen; i++) {
	  colors += !(cfa & (1 << cfa_pat[i]));
	  cfa |= 1 << cfa_pat[i];
	}
	if (cfa == 070) memcpy (cfa_pc,"\003\004\005",3);	/* CMY */
	if (cfa == 072) memcpy (cfa_pc,"\005\003\004\001",4);	/* GMCY */
	goto guess_cfa_pc;
	  case 33424:
	fseek (ifp, get4()+base, SEEK_SET);
	parse_kodak_ifd (base);
	break;
	  case 33434:			/* ExposureTime */
	shutter = getreal(type);
	break;
	  case 33437:			/* FNumber */
	aperture = getreal(type);
	break;
	  case 34306:			/* Leaf white balance */
	FORC4 cam_mul[c ^ 1] = 4096.0 / get2();
	break;
	  case 34307:			/* Leaf CatchLight color matrix */
	fread (software, 1, 7, ifp);
	if (strncmp(software,"MATRIX",6)) break;
	colors = 4;
	for (raw_color = i=0; i < 3; i++) {
	  FORC4 fscanf (ifp, "%f", &rgb_cam[i][c^1]);
	  if (!use_camera_wb) continue;
	  num = 0;
	  FORC4 num += rgb_cam[i][c];
	  FORC4 rgb_cam[i][c] /= num;
	}
	break;
	  case 34310:			/* Leaf metadata */
	parse_mos (ftell(ifp));
	  case 34303:
	strcpy (make, "Leaf");
	break;
	  case 34665:			/* EXIF tag */
	fseek (ifp, get4()+base, SEEK_SET);
	parse_exif (base);
	break;
	  case 34853:			/* GPSInfo tag */
	fseek (ifp, get4()+base, SEEK_SET);
	parse_gps (base);
	break;
	  case 34675:			/* InterColorProfile */
	  case 50831:			/* AsShotICCProfile */
	profile_offset = ftell(ifp);
	profile_length = len;
	break;
	  case 37122:			/* CompressedBitsPerPixel */
	kodak_cbpp = get4();
	break;
	  case 37386:			/* FocalLength */
	focal_len = getreal(type);
	break;
	  case 37393:			/* ImageNumber */
	shot_order = getint(type);
	break;
	  case 37400:			/* old Kodak KDC tag */
	for (raw_color = i=0; i < 3; i++) {
	  getreal(type);
	  FORC3 rgb_cam[i][c] = getreal(type);
	}
	break;
	  case 46275:			/* Imacon tags */
	strcpy (make, "Imacon");
	data_offset = ftell(ifp);
	ima_len = len;
	break;
	  case 46279:
	fseek (ifp, 78, SEEK_CUR);
	raw_width  = get4();
	raw_height = get4();
	left_margin = get4() & 7;
	width = raw_width - left_margin - (get4() & 7);
	top_margin = get4() & 7;
	height = raw_height - top_margin - (get4() & 7);
	if (raw_width == 7262) {
	  height = 5444;
	  width  = 7244;
	  left_margin = 7;
	}
	fseek (ifp, 52, SEEK_CUR);
	FORC3 cam_mul[c] = getreal(11);
	fseek (ifp, 114, SEEK_CUR);
	flip = (get2() >> 7) * 90;
	if (width * height * 6 == ima_len) {
	  if (flip % 180 == 90) SWAP(width,height);
	  filters = flip = 0;
	}
	sprintf (model, "Ixpress %d-Mp", height*width/1000000);
	load_raw = &CLASS imacon_full_load_raw;
	if (filters) {
	  if (left_margin & 1) filters = 0x61616161;
	  load_raw = &CLASS unpacked_load_raw;
	}
	maximum = 0xffff;
	break;
	  case 50454:			/* Sinar tag */
	  case 50455:
	if (!(cbuf = (char *) malloc(len))) break;
	fread (cbuf, 1, len, ifp);
	for (cp = cbuf-1; cp && cp < cbuf+len; cp = strchr(cp,'\n'))
	  if (!strncmp (++cp,"Neutral ",8))
		sscanf (cp+8, "%f %f %f", cam_mul, cam_mul+1, cam_mul+2);
	free (cbuf);
	break;
	  case 50459:			/* Hasselblad tag */
	i = order;
	j = ftell(ifp);
	c = tiff_nifds;
	order = get2();
	fseek (ifp, j+(get2(),get4()), SEEK_SET);
	parse_tiff_ifd (j);
	maximum = 0xffff;
	tiff_nifds = c;
	order = i;
	break;
	  case 50706:			/* DNGVersion */
	FORC4 dng_version = (dng_version << 8) + fgetc(ifp);
	break;
	  case 50710:			/* CFAPlaneColor */
	if (len > 4) len = 4;
	colors = len;
	fread (cfa_pc, 1, colors, ifp);
guess_cfa_pc:
	FORCC tab[cfa_pc[c]] = c;
	cdesc[c] = 0;
	for (i=16; i--; )
	  filters = filters << 2 | tab[cfa_pat[i % plen]];
	break;
	  case 50711:			/* CFALayout */
	if (get2() == 2) {
	  fuji_width = 1;
	  filters = 0x49494949;
	}
	break;
	  case 291:
	  case 50712:			/* LinearizationTable */
	linear_table (len);
	break;
	  case 50714:			/* BlackLevel */
	  case 50715:			/* BlackLevelDeltaH */
	  case 50716:			/* BlackLevelDeltaV */
	for (dblack=i=0; i < len; i++)
	  dblack += getreal(type);
	black += dblack/len + 0.5;
	break;
	  case 50717:			/* WhiteLevel */
	maximum = getint(type);
	break;
	  case 50718:			/* DefaultScale */
	pixel_aspect  = getreal(type);
	pixel_aspect /= getreal(type);
	break;
	  case 50721:			/* ColorMatrix1 */
	  case 50722:			/* ColorMatrix2 */
	FORCC for (j=0; j < 3; j++)
	  cm[c][j] = getreal(type);
	use_cm = 1;
	break;
	  case 50723:			/* CameraCalibration1 */
	  case 50724:			/* CameraCalibration2 */
	for (i=0; i < colors; i++)
	  FORCC cc[i][c] = getreal(type);
	  case 50727:			/* AnalogBalance */
	FORCC ab[c] = getreal(type);
	break;
	  case 50728:			/* AsShotNeutral */
	FORCC asn[c] = getreal(type);
	break;
	  case 50729:			/* AsShotWhiteXY */
	xyz[0] = getreal(type);
	xyz[1] = getreal(type);
	xyz[2] = 1 - xyz[0] - xyz[1];
	FORC3 xyz[c] /= d65_white[c];
	break;
	  case 50740:			/* DNGPrivateData */
	if (dng_version) break;
	parse_minolta (j = get4()+base);
	fseek (ifp, j, SEEK_SET);
	parse_tiff_ifd (base);
	break;
	  case 50752:
	read_shorts (cr2_slice, 3);
	break;
	  case 50829:			/* ActiveArea */
	top_margin = getint(type);
	left_margin = getint(type);
	height = getint(type) - top_margin;
	width = getint(type) - left_margin;
	break;
	  case 64772:			/* Kodak P-series */
	fseek (ifp, 16, SEEK_CUR);
	data_offset = get4();
	fseek (ifp, 28, SEEK_CUR);
	data_offset += get4();
	load_raw = &CLASS packed_12_load_raw;
	}
	fseek (ifp, save, SEEK_SET);
  }
  if (sony_length && (buf = (unsigned *) malloc(sony_length))) {
	fseek (ifp, sony_offset, SEEK_SET);
	fread (buf, sony_length, 1, ifp);
	sony_decrypt (buf, sony_length/4, 1, sony_key);
	sfp = ifp;
	if ((ifp = tmpfile())) {
	  fwrite (buf, sony_length, 1, ifp);
	  fseek (ifp, 0, SEEK_SET);
	  parse_tiff_ifd (-sony_offset);
	  fclose (ifp);
	}
	ifp = sfp;
	free (buf);
  }
  for (i=0; i < colors; i++)
	FORCC cc[i][c] *= ab[i];
  if (use_cm) {
	FORCC for (i=0; i < 3; i++)
	  for (cam_xyz[c][i]=j=0; j < colors; j++)
	cam_xyz[c][i] += cc[c][j] * cm[j][i] * xyz[i];
	cam_xyz_coeff (cam_xyz);
  }
  if (asn[0]) {
	cam_mul[3] = 0;
	FORCC cam_mul[c] = 1 / asn[c];
  }
  if (!use_cm)
	FORCC pre_mul[c] /= cc[c][c];
  return 0;
}

void CLASS parse_tiff (int base)
{
  int doff, max_samp=0, raw=-1, thm=-1, i;
  struct jhead jh;

  fseek (ifp, base, SEEK_SET);
  order = get2();
  if (order != 0x4949 && order != 0x4d4d) return;
  get2();
  memset (tiff_ifd, 0, sizeof tiff_ifd);
  tiff_nifds = 0;
  while ((doff = get4())) {
	fseek (ifp, doff+base, SEEK_SET);
	if (parse_tiff_ifd (base)) break;
  }
  thumb_misc = 16;
  if (thumb_offset) {
	fseek (ifp, thumb_offset, SEEK_SET);
	if (ljpeg_start (&jh, 1)) {
	  thumb_misc   = jh.bits;
	  thumb_width  = jh.wide;
	  thumb_height = jh.high;
	}
  }
  for (i=0; i < tiff_nifds; i++) {
	if (max_samp < tiff_ifd[i].samples)
	max_samp = tiff_ifd[i].samples;
	if (max_samp > 3) max_samp = 3;
	if ((tiff_ifd[i].comp != 6 || tiff_ifd[i].samples != 3) &&
	tiff_ifd[i].width*tiff_ifd[i].height > raw_width*raw_height) {
	  raw_width     = tiff_ifd[i].width;
	  raw_height    = tiff_ifd[i].height;
	  tiff_bps      = tiff_ifd[i].bps;
	  tiff_compress = tiff_ifd[i].comp;
	  data_offset   = tiff_ifd[i].offset;
	  tiff_flip     = tiff_ifd[i].flip;
	  tiff_samples  = tiff_ifd[i].samples;
	  raw = i;
	}
  }
  fuji_width *= (raw_width+1)/2;
  if (tiff_ifd[0].flip) tiff_flip = tiff_ifd[0].flip;
  if (raw >= 0 && !load_raw)
	switch (tiff_compress) {
	  case 0:  case 1:
	switch (tiff_bps) {
	  case  8: load_raw = &CLASS eight_bit_load_raw;	break;
	  case 12: load_raw = &CLASS packed_12_load_raw;
		   if (!strncmp(make,"NIKON",5))
			 load_raw = &CLASS nikon_load_raw;
		   if (strncmp(make,"PENTAX",6)) break;
	  case 14:
	  case 16: load_raw = &CLASS unpacked_load_raw;		break;
	}
	if (tiff_ifd[raw].bytes*5 == raw_width*raw_height*8)
	  load_raw = &CLASS olympus_e300_load_raw;
	if (tiff_bps == 12 && tiff_ifd[raw].phint == 2)
	  load_raw = &CLASS olympus_cseries_load_raw;
	break;
	  case 6:  case 7:  case 99:
	load_raw = &CLASS lossless_jpeg_load_raw;		break;
	  case 262:
	load_raw = &CLASS kodak_262_load_raw;			break;
	  case 32767:
	load_raw = &CLASS sony_arw2_load_raw;
	if (tiff_ifd[raw].bytes*8 == raw_width*raw_height*tiff_bps)
	  break;
	raw_height += 8;
	load_raw = &CLASS sony_arw_load_raw;			break;
	  case 32769:
	load_raw = &CLASS nikon_load_raw;			break;
	  case 32773:
	load_raw = &CLASS packed_12_load_raw;			break;
	  case 34713:
	load_raw = &CLASS nikon_compressed_load_raw;		break;
	  case 65535:
	load_raw = &CLASS pentax_k10_load_raw;			break;
	  case 65000:
	switch (tiff_ifd[raw].phint) {
	  case 2: load_raw = &CLASS kodak_rgb_load_raw;   filters = 0;  break;
	  case 6: load_raw = &CLASS kodak_ycbcr_load_raw; filters = 0;  break;
	  case 32803: load_raw = &CLASS kodak_65000_load_raw;
	}
	  case 32867: break;
	  default: is_raw = 0;
	}
  if (!dng_version && tiff_samples == 3)
	if (tiff_ifd[raw].bytes && tiff_bps != 14 && tiff_bps != 2048)
	  is_raw = 0;
  if (!dng_version && tiff_bps == 8 && tiff_compress == 1 &&
	tiff_ifd[raw].phint == 1) is_raw = 0;
  if (tiff_bps == 8 && tiff_samples == 4) is_raw = 0;
  for (i=0; i < tiff_nifds; i++)
	if (i != raw && tiff_ifd[i].samples == max_samp &&
	tiff_ifd[i].width * tiff_ifd[i].height / SQR(tiff_ifd[i].bps+1) >
		  thumb_width *       thumb_height / SQR(thumb_misc+1)) {
	  thumb_width  = tiff_ifd[i].width;
	  thumb_height = tiff_ifd[i].height;
	  thumb_offset = tiff_ifd[i].offset;
	  thumb_length = tiff_ifd[i].bytes;
	  thumb_misc   = tiff_ifd[i].bps;
	  thm = i;
	}
  if (thm >= 0) {
	thumb_misc |= tiff_ifd[thm].samples << 5;
	switch (tiff_ifd[thm].comp) {
	  case 0:
	write_thumb = &CLASS layer_thumb;
	break;
	  case 1:
	if (tiff_ifd[thm].bps > 8)
	  thumb_load_raw = &CLASS kodak_thumb_load_raw;
	else
	  write_thumb = &CLASS ppm_thumb;
	break;
	  case 65000:
	thumb_load_raw = tiff_ifd[thm].phint == 6 ?
		&CLASS kodak_ycbcr_load_raw : &CLASS kodak_rgb_load_raw;
	}
  }
}

void CLASS parse_minolta (int base)
{
  int save, tag, len, offset, high=0, wide=0, i, c;
  short sorder=order;

  fseek (ifp, base, SEEK_SET);
  if (fgetc(ifp) || fgetc(ifp)-'M' || fgetc(ifp)-'R') return;
  order = fgetc(ifp) * 0x101;
  offset = base + get4() + 8;
  while ((save=ftell(ifp)) < offset) {
	for (tag=i=0; i < 4; i++)
	  tag = tag << 8 | fgetc(ifp);
	len = get4();
	switch (tag) {
	  case 0x505244:				/* PRD */
	fseek (ifp, 8, SEEK_CUR);
	high = get2();
	wide = get2();
	break;
	  case 0x574247:				/* WBG */
	get4();
	i = strcmp(model,"DiMAGE A200") ? 0:3;
	FORC4 cam_mul[c ^ (c >> 1) ^ i] = get2();
	break;
	  case 0x545457:				/* TTW */
	parse_tiff (ftell(ifp));
	data_offset = offset;
	}
	fseek (ifp, save+len+8, SEEK_SET);
  }
  raw_height = high;
  raw_width  = wide;
  order = sorder;
}

/*
   CIFF block 0x1030 contains an 8x8 white sample.
   Load this into white[][] for use in scale_colors().
 */
void CLASS ciff_block_1030()
{
  static const ushort key[] = { 0x410, 0x45f3 };
  int i, bpp, row, col, vbits=0;
  unsigned long bitbuf=0;

  if ((get2(),get4()) != 0x80008 || !get4()) return;
  bpp = get2();
  if (bpp != 10 && bpp != 12) return;
  for (i=row=0; row < 8; row++)
	for (col=0; col < 8; col++) {
	  if (vbits < bpp) {
	bitbuf = bitbuf << 16 | (get2() ^ key[i++ & 1]);
	vbits += 16;
	  }
	  white[row][col] =
	bitbuf << (LONG_BIT - vbits) >> (LONG_BIT - bpp);
	  vbits -= bpp;
	}
}

/*
   Parse a CIFF file, better known as Canon CRW format.
 */
void CLASS parse_ciff (int offset, int length)
{
  int tboff, nrecs, c, type, len, save, wbi=-1;
  ushort key[] = { 0x410, 0x45f3 };

  fseek (ifp, offset+length-4, SEEK_SET);
  tboff = get4() + offset;
  fseek (ifp, tboff, SEEK_SET);
  nrecs = get2();
  if (nrecs > 100) return;
  while (nrecs--) {
	type = get2();
	len  = get4();
	save = ftell(ifp) + 4;
	fseek (ifp, offset+get4(), SEEK_SET);
	if ((((type >> 8) + 8) | 8) == 0x38)
	  parse_ciff (ftell(ifp), len);	/* Parse a sub-table */

	if (type == 0x0810)
	  fread (artist, 64, 1, ifp);
	if (type == 0x080a) {
	  fread (make, 64, 1, ifp);
	  fseek (ifp, strlen(make) - 63, SEEK_CUR);
	  fread (model, 64, 1, ifp);
	}
	if (type == 0x1810) {
	  fseek (ifp, 12, SEEK_CUR);
	  flip = get4();
	}
	if (type == 0x1835)			/* Get the decoder table */
	  tiff_compress = get4();
	if (type == 0x2007) {
	  thumb_offset = ftell(ifp);
	  thumb_length = len;
	}
	if (type == 0x1818) {
	  shutter = pow (2, -int_to_float((get4(),get4())));
	  aperture = pow (2, int_to_float(get4())/2);
	}
	if (type == 0x102a) {
	  iso_speed = pow (2, (get4(),get2())/32.0 - 4) * 50;
	  aperture  = pow (2, (get2(),(short)get2())/64.0);
	  shutter   = pow (2,-((short)get2())/32.0);
	  wbi = (get2(),get2());
	  if (wbi > 17) wbi = 0;
	  fseek (ifp, 32, SEEK_CUR);
	  if (shutter > 1e6) shutter = get2()/10.0;
	}
	if (type == 0x102c) {
	  if (get2() > 512) {		/* Pro90, G1 */
	fseek (ifp, 118, SEEK_CUR);
	FORC4 cam_mul[c ^ 2] = get2();
	  } else {				/* G2, S30, S40 */
	fseek (ifp, 98, SEEK_CUR);
	FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get2();
	  }
	}
	if (type == 0x0032) {
	  if (len == 768) {			/* EOS D30 */
	fseek (ifp, 72, SEEK_CUR);
	FORC4 cam_mul[c ^ (c >> 1)] = 1024.0 / get2();
	if (!wbi) cam_mul[0] = -1;	/* use my auto white balance */
	  } else if (!cam_mul[0]) {
	if (get2() == key[0])		/* Pro1, G6, S60, S70 */
	  c = (strstr(model,"Pro1") ?
		  "012346000000000000":"01345:000000006008")[wbi]-'0'+ 2;
	else {				/* G3, G5, S45, S50 */
	  c = "023457000000006000"[wbi]-'0';
	  key[0] = key[1] = 0;
	}
	fseek (ifp, 78 + c*8, SEEK_CUR);
	FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get2() ^ key[c & 1];
	if (!wbi) cam_mul[0] = -1;
	  }
	}
	if (type == 0x10a9) {		/* D60, 10D, 300D, and clones */
	  if (len > 66) wbi = "0134567028"[wbi]-'0';
	  fseek (ifp, 2 + wbi*8, SEEK_CUR);
	  FORC4 cam_mul[c ^ (c >> 1)] = get2();
	}
	if (type == 0x1030 && (0x18040 >> wbi & 1))
	  ciff_block_1030();		/* all that don't have 0x10a9 */
	if (type == 0x1031) {
	  raw_width = (get2(),get2());
	  raw_height = get2();
	}
	if (type == 0x5029) {
	  focal_len = len >> 16;
	  if ((len & 0xffff) == 2) focal_len /= 32;
	}
	if (type == 0x5813) flash_used = int_to_float(len);
	if (type == 0x5814) canon_ev   = int_to_float(len);
	if (type == 0x5817) shot_order = len;
	if (type == 0x5834) unique_id  = len;
	if (type == 0x580e) timestamp  = len;
	if (type == 0x180e) timestamp  = get4();
#ifdef LOCALTIME
	if ((type | 0x4000) == 0x580e)
	  timestamp = mktime (gmtime (&timestamp));
#endif
	fseek (ifp, save, SEEK_SET);
  }
}

void CLASS parse_rollei()
{
  char line[128], *val;
  struct tm t;

  fseek (ifp, 0, SEEK_SET);
  memset (&t, 0, sizeof t);
  do {
	fgets (line, 128, ifp);
	if ((val = strchr(line,'=')))
	  *val++ = 0;
	else
	  val = line + strlen(line);
	if (!strcmp(line,"DAT"))
	  sscanf (val, "%d.%d.%d", &t.tm_mday, &t.tm_mon, &t.tm_year);
	if (!strcmp(line,"TIM"))
	  sscanf (val, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
	if (!strcmp(line,"HDR"))
	  thumb_offset = atoi(val);
	if (!strcmp(line,"X  "))
	  raw_width = atoi(val);
	if (!strcmp(line,"Y  "))
	  raw_height = atoi(val);
	if (!strcmp(line,"TX "))
	  thumb_width = atoi(val);
	if (!strcmp(line,"TY "))
	  thumb_height = atoi(val);
  } while (strncmp(line,"EOHD",4));
  data_offset = thumb_offset + thumb_width * thumb_height * 2;
  t.tm_year -= 1900;
  t.tm_mon -= 1;
  if (mktime(&t) > 0)
	timestamp = mktime(&t);
  strcpy (make, "Rollei");
  strcpy (model,"d530flex");
  write_thumb = &CLASS rollei_thumb;
}

void CLASS parse_sinar_ia()
{
  int entries, off;
  char str[8], *cp;

  order = 0x4949;
  fseek (ifp, 4, SEEK_SET);
  entries = get4();
  fseek (ifp, get4(), SEEK_SET);
  while (entries--) {
	off = get4(); get4();
	fread (str, 8, 1, ifp);
	if (!strcmp(str,"META"))   meta_offset = off;
	if (!strcmp(str,"THUMB")) thumb_offset = off;
	if (!strcmp(str,"RAW0"))   data_offset = off;
  }
  fseek (ifp, meta_offset+20, SEEK_SET);
  fread (make, 64, 1, ifp);
  make[63] = 0;
  if ((cp = strchr(make,' '))) {
	strcpy (model, cp+1);
	*cp = 0;
  }
  raw_width  = get2();
  raw_height = get2();
  load_raw = &CLASS unpacked_load_raw;
  thumb_width = (get4(),get2());
  thumb_height = get2();
  write_thumb = &CLASS ppm_thumb;
  maximum = 0x3fff;
}

void CLASS parse_phase_one (int base)
{
  unsigned entries, tag, type, len, data, save, i, c;
  float romm_cam[3][3];
  char *cp;

  memset (&ph1, 0, sizeof ph1);
  fseek (ifp, base, SEEK_SET);
  order = get4() & 0xffff;
  if (get4() >> 8 != 0x526177) return;		/* "Raw" */
  fseek (ifp, base+get4(), SEEK_SET);
  entries = get4();
  get4();
  while (entries--) {
	tag  = get4();
	type = get4();
	len  = get4();
	data = get4();
	save = ftell(ifp);
	fseek (ifp, base+data, SEEK_SET);
	switch (tag) {
	  case 0x100:  flip = "0653"[data & 3]-'0';  break;
	  case 0x106:
	for (i=0; i < 9; i++)
	  romm_cam[0][i] = getreal(11);
	romm_coeff (romm_cam);
	break;
	  case 0x107:
	FORC3 cam_mul[c] = getreal(11);
	break;
	  case 0x108:  raw_width     = data;	break;
	  case 0x109:  raw_height    = data;	break;
	  case 0x10a:  left_margin   = data;	break;
	  case 0x10b:  top_margin    = data;	break;
	  case 0x10c:  width         = data;	break;
	  case 0x10d:  height        = data;	break;
	  case 0x10e:  ph1.format    = data;	break;
	  case 0x10f:  data_offset   = data+base;	break;
	  case 0x110:  meta_offset   = data+base;
		   meta_length   = len;			break;
	  case 0x112:  ph1.key_off   = save - 4;		break;
	  case 0x210:  ph1.tag_210   = int_to_float(data);	break;
	  case 0x21a:  ph1.tag_21a   = data;		break;
	  case 0x21c:  strip_offset  = data+base;		break;
	  case 0x21d:  ph1.black     = data;		break;
	  case 0x222:  ph1.split_col = data - left_margin;	break;
	  case 0x223:  ph1.black_off = data+base;		break;
	  case 0x301:
	model[63] = 0;
	fread (model, 1, 63, ifp);
	if ((cp = strstr(model," camera"))) *cp = 0;
	}
	fseek (ifp, save, SEEK_SET);
  }
  load_raw = ph1.format < 3 ?
	&CLASS phase_one_load_raw : &CLASS phase_one_load_raw_c;
  maximum = 0xffff;
  strcpy (make, "Phase One");
  if (model[0]) return;
  switch (raw_height) {
	case 2060: strcpy (model,"LightPhase");	break;
	case 2682: strcpy (model,"H 10");		break;
	case 4128: strcpy (model,"H 20");		break;
	case 5488: strcpy (model,"H 25");		break;
  }
}

void CLASS parse_fuji (int offset)
{
  unsigned entries, tag, len, save, c;

  fseek (ifp, offset, SEEK_SET);
  entries = get4();
  if (entries > 255) return;
  while (entries--) {
	tag = get2();
	len = get2();
	save = ftell(ifp);
	if (tag == 0x100) {
	  raw_height = get2();
	  raw_width  = get2();
	} else if (tag == 0x121) {
	  height = get2();
	  if ((width = get2()) == 4284) width += 3;
	} else if (tag == 0x130)
	  fuji_layout = fgetc(ifp) >> 7;
	if (tag == 0x2ff0)
	  FORC4 cam_mul[c ^ 1] = get2();
	fseek (ifp, save+len, SEEK_SET);
  }
  height <<= fuji_layout;
  width  >>= fuji_layout;
}

int CLASS parse_jpeg (int offset)
{
  int len, save, hlen, mark;

  fseek (ifp, offset, SEEK_SET);
  if (fgetc(ifp) != 0xff || fgetc(ifp) != 0xd8) return 0;

  while (fgetc(ifp) == 0xff && (mark = fgetc(ifp)) != 0xda) {
	order = 0x4d4d;
	len   = get2() - 2;
	save  = ftell(ifp);
	if (mark == 0xc0 || mark == 0xc3) {
	  fgetc(ifp);
	  raw_height = get2();
	  raw_width  = get2();
	}
	order = get2();
	hlen  = get4();
	if (get4() == 0x48454150)		/* "HEAP" */
	  parse_ciff (save+hlen, len-hlen);
	parse_tiff (save+6);
	fseek (ifp, save+len, SEEK_SET);
  }
  return 1;
}

void CLASS parse_riff()
{
  unsigned i, size, end;
  char tag[4], date[64], month[64];
  static const char mon[12][4] =
  { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  struct tm t;

  order = 0x4949;
  fread (tag, 4, 1, ifp);
  size = get4();
  if (!memcmp(tag,"RIFF",4) || !memcmp(tag,"LIST",4)) {
	end = ftell(ifp) + size;
	get4();
	while (ftell(ifp) < end)
	  parse_riff();
  } else if (!memcmp(tag,"IDIT",4) && size < 64) {
	fread (date, 64, 1, ifp);
	date[size] = 0;
	memset (&t, 0, sizeof t);
	if (sscanf (date, "%*s %s %d %d:%d:%d %d", month, &t.tm_mday,
	&t.tm_hour, &t.tm_min, &t.tm_sec, &t.tm_year) == 6) {
	  for (i=0; i < 12 && strcmp(mon[i],month); i++);
	  t.tm_mon = i;
	  t.tm_year -= 1900;
	  if (mktime(&t) > 0)
	timestamp = mktime(&t);
	}
  } else
	fseek (ifp, size, SEEK_CUR);
}

void CLASS parse_smal (int offset, int fsize)
{
  int ver;

  fseek (ifp, offset+2, SEEK_SET);
  order = 0x4949;
  ver = fgetc(ifp);
  if (ver == 6)
	fseek (ifp, 5, SEEK_CUR);
  if (get4() != fsize) return;
  if (ver > 6) data_offset = get4();
  raw_height = height = get2();
  raw_width  = width  = get2();
  strcpy (make, "SMaL");
  sprintf (model, "v%d %dx%d", ver, width, height);
  if (ver == 6) load_raw = &CLASS smal_v6_load_raw;
  if (ver == 9) load_raw = &CLASS smal_v9_load_raw;
}

void CLASS parse_cine()
{
  unsigned off_head, off_setup, off_image, i;

  order = 0x4949;
  fseek (ifp, 4, SEEK_SET);
  is_raw = get2() == 2;
  fseek (ifp, 14, SEEK_CUR);
  is_raw *= get4();
  off_head = get4();
  off_setup = get4();
  off_image = get4();
  timestamp = get4();
  if ((i = get4())) timestamp = i;
  fseek (ifp, off_head+4, SEEK_SET);
  raw_width = get4();
  raw_height = get4();
  switch (get2(),get2()) {
	case  8:  load_raw = &CLASS eight_bit_load_raw;  break;
	case 16:  load_raw = &CLASS  unpacked_load_raw;
  }
  fseek (ifp, off_setup+792, SEEK_SET);
  strcpy (make, "CINE");
  sprintf (model, "%d", get4());
  fseek (ifp, 12, SEEK_CUR);
  switch ((i=get4()) & 0xffffff) {
	case  3:  filters = 0x94949494;  break;
	case  4:  filters = 0x49494949;  break;
	default:  is_raw = 0;
  }
  fseek (ifp, 72, SEEK_CUR);
  switch ((get4()+3600) % 360) {
	case 270:  flip = 4;  break;
	case 180:  flip = 1;  break;
	case  90:  flip = 7;  break;
	case   0:  flip = 2;
  }
  cam_mul[0] = getreal(11);
  cam_mul[2] = getreal(11);
  maximum = ~(-1 << get4());
  fseek (ifp, 668, SEEK_CUR);
  shutter = get4()/1000000000.0;
  fseek (ifp, off_image, SEEK_SET);
  if (shot_select < is_raw)
	fseek (ifp, shot_select*8, SEEK_CUR);
  data_offset  = (INT64) get4() + 8;
  data_offset += (INT64) get4() << 32;
}

char * CLASS foveon_gets (int offset, char *str, int len)
{
  int i;
  fseek (ifp, offset, SEEK_SET);
  for (i=0; i < len-1; i++)
	if ((str[i] = get2()) == 0) break;
  str[i] = 0;
  return str;
}

void CLASS parse_foveon()
{
  int entries, img=0, off, len, tag, save, i, wide, high, pent, poff[256][2];
  char name[64], value[64];

  order = 0x4949;			/* Little-endian */
  fseek (ifp, 36, SEEK_SET);
  flip = get4();
  fseek (ifp, -4, SEEK_END);
  fseek (ifp, get4(), SEEK_SET);
  if (get4() != 0x64434553) return;	/* SECd */
  entries = (get4(),get4());
  while (entries--) {
	off = get4();
	len = get4();
	tag = get4();
	save = ftell(ifp);
	fseek (ifp, off, SEEK_SET);
	if (get4() != (0x20434553 | (tag << 24))) return;
	switch (tag) {
	  case 0x47414d49:			/* IMAG */
	  case 0x32414d49:			/* IMA2 */
	fseek (ifp, 12, SEEK_CUR);
	wide = get4();
	high = get4();
	if (wide > raw_width && high > raw_height) {
	  raw_width  = wide;
	  raw_height = high;
	  data_offset = off+24;
	}
	fseek (ifp, off+28, SEEK_SET);
	if (fgetc(ifp) == 0xff && fgetc(ifp) == 0xd8
		&& thumb_length < len-28) {
	  thumb_offset = off+28;
	  thumb_length = len-28;
	  write_thumb = &CLASS jpeg_thumb;
	}
	if (++img == 2 && !thumb_length) {
	  thumb_offset = off+24;
	  thumb_width = wide;
	  thumb_height = high;
	  write_thumb = &CLASS foveon_thumb;
	}
	break;
	  case 0x464d4143:			/* CAMF */
	meta_offset = off+24;
	meta_length = len-28;
	if (meta_length > 0x20000)
		meta_length = 0x20000;
	break;
	  case 0x504f5250:			/* PROP */
	pent = (get4(),get4());
	fseek (ifp, 12, SEEK_CUR);
	off += pent*8 + 24;
	if ((unsigned) pent > 256) pent=256;
	for (i=0; i < pent*2; i++)
	  poff[0][i] = off + get4()*2;
	for (i=0; i < pent; i++) {
	  foveon_gets (poff[i][0], name, 64);
	  foveon_gets (poff[i][1], value, 64);
	  if (!strcmp (name, "ISO"))
		iso_speed = atoi(value);
	  if (!strcmp (name, "CAMMANUF"))
		strcpy (make, value);
	  if (!strcmp (name, "CAMMODEL"))
		strcpy (model, value);
	  if (!strcmp (name, "WB_DESC"))
		strcpy (model2, value);
	  if (!strcmp (name, "TIME"))
		timestamp = atoi(value);
	  if (!strcmp (name, "EXPTIME"))
		shutter = atoi(value) / 1000000.0;
	  if (!strcmp (name, "APERTURE"))
		aperture = atof(value);
	  if (!strcmp (name, "FLENGTH"))
		focal_len = atof(value);
	}
#ifdef LOCALTIME
	timestamp = mktime (gmtime (&timestamp));
#endif
	}
	fseek (ifp, save, SEEK_SET);
  }
  is_foveon = 1;
}

/*
   Thanks to Adobe for providing these excellent CAM -> XYZ matrices!
 */
void CLASS adobe_coeff (char *make, char *model)
{
	// not needed for thumbs only
}

void CLASS simple_coeff (int index)
{
  static const float table[][12] = {
  /* index 0 -- all Foveon cameras */
  { 1.4032,-0.2231,-0.1016,-0.5263,1.4816,0.017,-0.0112,0.0183,0.9113 },
  /* index 1 -- Kodak DC20 and DC25 */
  { 2.25,0.75,-1.75,-0.25,-0.25,0.75,0.75,-0.25,-0.25,-1.75,0.75,2.25 },
  /* index 2 -- Logitech Fotoman Pixtura */
  { 1.893,-0.418,-0.476,-0.495,1.773,-0.278,-1.017,-0.655,2.672 },
  /* index 3 -- Nikon E880, E900, and E990 */
  { -1.936280,  1.800443, -1.448486,  2.584324,
	 1.405365, -0.524955, -0.289090,  0.408680,
	-1.204965,  1.082304,  2.941367, -1.818705 }
  };
  int i, c;

  for (raw_color = i=0; i < 3; i++)
	FORCC rgb_cam[i][c] = table[index][i*colors+c];
}

short CLASS guess_byte_order (int words)
{
  uchar test[4][2];
  int t=2, msb;
  double diff, sum[2] = {0,0};

  fread (test[0], 2, 2, ifp);
  for (words-=2; words--; ) {
	fread (test[t], 2, 1, ifp);
	for (msb=0; msb < 2; msb++) {
	  diff = (test[t^2][msb] << 8 | test[t^2][!msb])
	   - (test[t  ][msb] << 8 | test[t  ][!msb]);
	  sum[msb] += diff*diff;
	}
	t = (t+1) & 3;
  }
  return sum[0] < sum[1] ? 0x4d4d : 0x4949;
}

/*
   Identify which camera created this file, and set global variables
   accordingly.
 */
void CLASS identify()
{
  char head[32], *cp;
  unsigned hlen, fsize, i, c, is_canon;
  struct jhead jh;
  static const struct {
	int fsize;
	char make[12], model[19], withjpeg;
  } table[] = {
	{    62464, "Kodak",    "DC20"       ,0 },
	{   124928, "Kodak",    "DC20"       ,0 },
	{  1652736, "Kodak",    "DCS200"     ,0 },
	{  4159302, "Kodak",    "C330"       ,0 },
	{  4162462, "Kodak",    "C330"       ,0 },
	{   311696, "ST Micro", "STV680 VGA" ,0 },  /* SPYz */
	{   614400, "Kodak",    "KAI-0340"   ,0 },
	{   787456, "Creative", "PC-CAM 600" ,0 },
	{  1138688, "Minolta",  "RD175"      ,0 },
	{  3840000, "Foculus",  "531C"       ,0 },
	{   786432, "AVT",      "F-080C"     ,0 },
	{  1447680, "AVT",      "F-145C"     ,0 },
	{  1920000, "AVT",      "F-201C"     ,0 },
	{  5067304, "AVT",      "F-510C"     ,0 },
	{ 10134608, "AVT",      "F-510C"     ,0 },
	{ 16157136, "AVT",      "F-810C"     ,0 },
	{  1409024, "Sony",     "XCD-SX910CR",0 },
	{  2818048, "Sony",     "XCD-SX910CR",0 },
	{  3884928, "Micron",   "2010"       ,0 },
	{  6624000, "Pixelink", "A782"       ,0 },
	{ 13248000, "Pixelink", "A782"       ,0 },
	{  6291456, "RoverShot","3320AF"     ,0 },
	{  6553440, "Canon",    "PowerShot A460",0 },
	{  6653280, "Canon",    "PowerShot A530",0 },
	{  6573120, "Canon",    "PowerShot A610",0 },
	{  9219600, "Canon",    "PowerShot A620",0 },
	{ 10341600, "Canon",    "PowerShot A720",0 },
	{ 10383120, "Canon",    "PowerShot A630",0 },
	{ 12945240, "Canon",    "PowerShot A640",0 },
	{ 15636240, "Canon",    "PowerShot A650",0 },
	{  7710960, "Canon",    "PowerShot S3 IS",0 },
	{  5939200, "OLYMPUS",  "C770UZ"     ,0 },
	{  1581060, "NIKON",    "E900"       ,1 },  /* or E900s,E910 */
	{  2465792, "NIKON",    "E950"       ,1 },  /* or E800,E700 */
	{  2940928, "NIKON",    "E2100"      ,1 },  /* or E2500 */
	{  4771840, "NIKON",    "E990"       ,1 },  /* or E995, Oly C3030Z */
	{  4775936, "NIKON",    "E3700"      ,1 },  /* or Optio 33WR */
	{  5869568, "NIKON",    "E4300"      ,1 },  /* or DiMAGE Z2 */
	{  5865472, "NIKON",    "E4500"      ,1 },
	{  7438336, "NIKON",    "E5000"      ,1 },  /* or E5700 */
	{  8998912, "NIKON",    "COOLPIX S6" ,1 },
	{  1976352, "CASIO",    "QV-2000UX"  ,1 },
	{  3217760, "CASIO",    "QV-3*00EX"  ,1 },
	{  6218368, "CASIO",    "QV-5700"    ,1 },
	{  6054400, "CASIO",    "QV-R41"     ,1 },
	{  7530816, "CASIO",    "QV-R51"     ,1 },
	{  7684000, "CASIO",    "QV-4000"    ,1 },
	{  4948608, "CASIO",    "EX-S100"    ,1 },
	{  7542528, "CASIO",    "EX-Z50"     ,1 },
	{  7753344, "CASIO",    "EX-Z55"     ,1 },
	{  7426656, "CASIO",    "EX-P505"    ,1 },
	{  9313536, "CASIO",    "EX-P600"    ,1 },
	{ 10979200, "CASIO",    "EX-P700"    ,1 },
	{  3178560, "PENTAX",   "Optio S"    ,1 },
	{  4841984, "PENTAX",   "Optio S"    ,1 },
	{  6114240, "PENTAX",   "Optio S4"   ,1 },  /* or S4i, CASIO EX-Z4 */
	{ 10702848, "PENTAX",   "Optio 750Z" ,1 },
	{ 12582980, "Sinar",    ""           ,0 },
	{ 33292868, "Sinar",    ""           ,0 },
	{ 44390468, "Sinar",    ""           ,0 } };
  static const char *corp[] =
	{ "Canon", "NIKON", "EPSON", "KODAK", "Kodak", "OLYMPUS", "PENTAX",
	  "MINOLTA", "Minolta", "Konica", "CASIO", "Sinar", "Phase One",
	  "SAMSUNG", "Mamiya" };

  tiff_flip = flip = filters = -1;	/* 0 is valid, so -1 is unknown */
  raw_height = raw_width = fuji_width = fuji_layout = cr2_slice[0] = 0;
  maximum = height = width = top_margin = left_margin = 0;
  cdesc[0] = desc[0] = artist[0] = make[0] = model[0] = model2[0] = 0;
  iso_speed = shutter = aperture = focal_len = unique_id = 0;
  memset (gpsdata, 0, sizeof gpsdata);
  memset (white, 0, sizeof white);
  thumb_offset = thumb_length = thumb_width = thumb_height = 0;
  load_raw = thumb_load_raw = 0;
  write_thumb = &CLASS jpeg_thumb;
  data_offset = meta_length = tiff_bps = tiff_compress = 0;
  kodak_cbpp = zero_after_ff = dng_version = 0;
  timestamp = shot_order = tiff_samples = black = is_foveon = 0;
  mix_green = profile_length = data_error = zero_is_bad = 0;
  pixel_aspect = is_raw = raw_color = use_gamma = 1;
  tile_width = tile_length = INT_MAX;
  for (i=0; i < 4; i++) {
	cam_mul[i] = i == 1;
	pre_mul[i] = i < 3;
	FORC3 cmatrix[c][i] = 0;
	FORC3 rgb_cam[c][i] = c == i;
  }
  colors = 3;
  tiff_bps = 12;

  order = get2();
  hlen = get4();
  fseek (ifp, 0, SEEK_SET);
  fread (head, 1, 32, ifp);
  fseek (ifp, 0, SEEK_END);
  fsize = ftell(ifp);
  if ((cp = (char *) memmem (head, 32, "MMMM", 4)) ||
	  (cp = (char *) memmem (head, 32, "IIII", 4))) {
	parse_phase_one (cp-head);
	if (cp-head) parse_tiff(0);
  } else if (order == 0x4949 || order == 0x4d4d) {
	if (!memcmp (head+6,"HEAPCCDR",8)) {
	  data_offset = hlen;
	  parse_ciff (hlen, fsize - hlen);
	} else {
	  parse_tiff(0);
	}
  } else if (!memcmp (head,"\xff\xd8\xff\xe1",4) &&
		 !memcmp (head+6,"Exif",4)) {
	fseek (ifp, 4, SEEK_SET);
	data_offset = 4 + get2();
	fseek (ifp, data_offset, SEEK_SET);
	if (fgetc(ifp) != 0xff)
	  parse_tiff(12);
	thumb_offset = 0;
  } else if (!memcmp (head+25,"ARECOYK",7)) {
	strcpy (make, "Contax");
	strcpy (model,"N Digital");
	fseek (ifp, 33, SEEK_SET);
	get_timestamp(1);
	fseek (ifp, 60, SEEK_SET);
	FORC4 cam_mul[c ^ (c >> 1)] = get4();
  } else if (!strcmp (head, "PXN")) {
	strcpy (make, "Logitech");
	strcpy (model,"Fotoman Pixtura");
  } else if (!strcmp (head, "qktk")) {
	strcpy (make, "Apple");
	strcpy (model,"QuickTake 100");
  } else if (!strcmp (head, "qktn")) {
	strcpy (make, "Apple");
	strcpy (model,"QuickTake 150");
  } else if (!memcmp (head,"FUJIFILM",8)) {
	fseek (ifp, 84, SEEK_SET);
	thumb_offset = get4();
	thumb_length = get4();
	fseek (ifp, 92, SEEK_SET);
	parse_fuji (get4());
	if (thumb_offset > 120) {
	  fseek (ifp, 120, SEEK_SET);
	  is_raw += (i = get4()) && 1;
	  if (is_raw == 2 && shot_select)
	parse_fuji (i);
	}
	fseek (ifp, 100, SEEK_SET);
	data_offset = get4();
	parse_tiff (thumb_offset+12);
  } else if (!memcmp (head,"RIFF",4)) {
	fseek (ifp, 0, SEEK_SET);
	parse_riff();
  } else if (!memcmp (head,"\0\001\0\001\0@",6)) {
	fseek (ifp, 6, SEEK_SET);
	fread (make, 1, 8, ifp);
	fread (model, 1, 8, ifp);
	fread (model2, 1, 16, ifp);
	data_offset = get2();
	get2();
	raw_width = get2();
	raw_height = get2();
	load_raw = nokia_load_raw;
	filters = 0x61616161;
  } else if (!memcmp (head,"DSC-Image",9))
	parse_rollei();
  else if (!memcmp (head,"PWAD",4))
	parse_sinar_ia();
  else if (!memcmp (head,"\0MRM",4))
	parse_minolta(0);
  else if (!memcmp (head,"FOVb",4))
	parse_foveon();
  else if (!memcmp (head,"CI",2))
	parse_cine();
  else
	for (i=0; i < sizeof table / sizeof *table; i++)
	  if (fsize == table[i].fsize) {
	strcpy (make,  table[i].make );
	strcpy (model, table[i].model);
	  }
  if (make[0] == 0) parse_smal (0, fsize);
  if (make[0] == 0) parse_jpeg (is_raw = 0);

  for (i=0; i < sizeof corp / sizeof *corp; i++)
	if (strstr (make, corp[i]))		/* Simplify company names */
	strcpy (make, corp[i]);
  if (!strncmp (make,"KODAK",5))
	make[16] = model[16] = 0;
  cp = make + strlen(make);		/* Remove trailing spaces */
  while (*--cp == ' ') *cp = 0;
  cp = model + strlen(model);
  while (*--cp == ' ') *cp = 0;
  i = strlen(make);			/* Remove make from model */
  if (!strncasecmp (model, make, i) && model[i++] == ' ')
	memmove (model, model+i, 64-i);
  if (!strncmp (model,"Digital Camera ",15))
	strcpy (model, model+15);
  desc[511] = artist[63] = make[63] = model[63] = model2[63] = 0;
  if (!is_raw) goto notraw;

  if (!maximum) maximum = (1 << tiff_bps) - 1;
  if (!height) height = raw_height;
  if (!width)  width  = raw_width;
  if (fuji_width) {
	width = height + fuji_width;
	height = width - 1;
	pixel_aspect = 1;
  }
  if (height == 2624 && width == 3936)	/* Pentax K10D and Samsung GX10 */
	{ height  = 2616;   width  = 3896; }
  if (height == 3136 && width == 4864)	/* Pentax K20D */
	{ height  = 3124;   width  = 4688; }
  if (dng_version) {
	if (filters == UINT_MAX) filters = 0;
	if (filters) is_raw = tiff_samples;
	else	 colors = tiff_samples;
	if (tiff_compress == 1)
	  load_raw = &CLASS adobe_dng_load_raw_nc;
	if (tiff_compress == 7)
	  load_raw = &CLASS adobe_dng_load_raw_lj;
	goto dng_skip;
  }
  if ((is_canon = !strcmp(make,"Canon")))
	load_raw = memcmp (head+6,"HEAPCCDR",8) ?
	&CLASS lossless_jpeg_load_raw : &CLASS canon_compressed_load_raw;
  if (!strcmp(make,"NIKON") && !load_raw)
	load_raw = &CLASS nikon_load_raw;
  if (!strcmp(make,"CASIO")) {
	load_raw = &CLASS packed_12_load_raw;
	maximum = 0xf7f;
  }

/* Set parameters based on camera name (for non-DNG files). */

  if (is_foveon) {
	if (height*2 < width) pixel_aspect = 0.5;
	if (height   > width) pixel_aspect = 2;
	filters = 0;
	load_raw = &CLASS foveon_load_raw;
	simple_coeff(0);
  } else if (is_canon && tiff_samples == 4) {
	filters = 0;
	load_raw = &CLASS canon_sraw_load_raw;
  } else if (!strcmp(model,"PowerShot 600")) {
	height = 613;
	width  = 854;
	raw_width = 896;
	pixel_aspect = 607/628.0;
	colors = 4;
	filters = 0xe1e4e1e4;
	load_raw = &CLASS canon_600_load_raw;
  } else if (!strcmp(model,"PowerShot A5") ||
		 !strcmp(model,"PowerShot A5 Zoom")) {
	height = 773;
	width  = 960;
	raw_width = 992;
	pixel_aspect = 256/235.0;
	colors = 4;
	filters = 0x1e4e1e4e;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A50")) {
	height =  968;
	width  = 1290;
	raw_width = 1320;
	colors = 4;
	filters = 0x1b4e4b1e;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot Pro70")) {
	height = 1024;
	width  = 1552;
	colors = 4;
	filters = 0x1e4b4e1b;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A460")) {
	height = 1960;
	width  = 2616;
	raw_height = 1968;
	raw_width  = 2664;
	top_margin  = 4;
	left_margin = 4;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A530")) {
	height = 1984;
	width  = 2620;
	raw_height = 1992;
	raw_width  = 2672;
	top_margin  = 6;
	left_margin = 10;
	load_raw = &CLASS canon_a5_load_raw;
	raw_color = 0;
  } else if (!strcmp(model,"PowerShot A610")) {
	if (canon_s2is()) strcpy (model+10, "S2 IS");
	height = 1960;
	width  = 2616;
	raw_height = 1968;
	raw_width  = 2672;
	top_margin  = 8;
	left_margin = 12;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A620")) {
	height = 2328;
	width  = 3112;
	raw_height = 2340;
	raw_width  = 3152;
	top_margin  = 12;
	left_margin = 36;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A720")) {
	height = 2472;
	width  = 3298;
	raw_height = 2480;
	raw_width  = 3336;
	top_margin  = 5;
	left_margin = 6;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A630")) {
	height = 2472;
	width  = 3288;
	raw_height = 2484;
	raw_width  = 3344;
	top_margin  = 6;
	left_margin = 12;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A640")) {
	height = 2760;
	width  = 3672;
	raw_height = 2772;
	raw_width  = 3736;
	top_margin  = 6;
	left_margin = 12;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot A650")) {
	height = 3024;
	width  = 4032;
	raw_height = 3048;
	raw_width  = 4104;
	top_margin  = 12;
	left_margin = 48;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot S3 IS")) {
	height = 2128;
	width  = 2840;
	raw_height = 2136;
	raw_width  = 2888;
	top_margin  = 8;
	left_margin = 44;
	load_raw = &CLASS canon_a5_load_raw;
  } else if (!strcmp(model,"PowerShot Pro90 IS")) {
	width  = 1896;
	colors = 4;
	filters = 0xb4b4b4b4;
  } else if (is_canon && raw_width == 2144) {
	height = 1550;
	width  = 2088;
	top_margin  = 8;
	left_margin = 4;
	if (!strcmp(model,"PowerShot G1")) {
	  colors = 4;
	  filters = 0xb4b4b4b4;
	}
  } else if (is_canon && raw_width == 2224) {
	height = 1448;
	width  = 2176;
	top_margin  = 6;
	left_margin = 48;
  } else if (is_canon && raw_width == 2376) {
	height = 1720;
	width  = 2312;
	top_margin  = 6;
	left_margin = 12;
  } else if (is_canon && raw_width == 2672) {
	height = 1960;
	width  = 2616;
	top_margin  = 6;
	left_margin = 12;
  } else if (is_canon && raw_width == 3152) {
	height = 2056;
	width  = 3088;
	top_margin  = 12;
	left_margin = 64;
	if (unique_id == 0x80000170)
	  adobe_coeff ("Canon","EOS 300D");
  } else if (is_canon && raw_width == 3160) {
	height = 2328;
	width  = 3112;
	top_margin  = 12;
	left_margin = 44;
  } else if (is_canon && raw_width == 3344) {
	height = 2472;
	width  = 3288;
	top_margin  = 6;
	left_margin = 4;
  } else if (!strcmp(model,"EOS D2000C")) {
	filters = 0x61616161;
  } else if (is_canon && raw_width == 3516) {
	top_margin  = 14;
	left_margin = 42;
	if (unique_id == 0x80000189)
	  adobe_coeff ("Canon","EOS 350D");
	goto canon_cr2;
  } else if (is_canon && raw_width == 3596) {
	top_margin  = 12;
	left_margin = 74;
	goto canon_cr2;
  } else if (is_canon && raw_width == 3944) {
	height = 2602;
	width  = 3908;
	top_margin  = 18;
	left_margin = 30;
  } else if (is_canon && raw_width == 3948) {
	top_margin  = 18;
	left_margin = 42;
	height -= 2;
	if (unique_id == 0x80000236)
	  adobe_coeff ("Canon","EOS 400D");
	goto canon_cr2;
  } else if (is_canon && raw_width == 3984) {
	top_margin  = 20;
	left_margin = 76;
	height -= 2;
	goto canon_cr2;
  } else if (is_canon && raw_width == 4104) {
	height = 3024;
	width  = 4032;
	top_margin  = 12;
	left_margin = 48;
  } else if (is_canon && raw_width == 4312) {
	top_margin  = 18;
	left_margin = 22;
	height -= 2;
	if (unique_id == 0x80000176)
	  adobe_coeff ("Canon","EOS 450D");
	goto canon_cr2;
  } else if (is_canon && raw_width == 4476) {
	top_margin  = 34;
	left_margin = 90;
	goto canon_cr2;
  } else if (is_canon && raw_width == 5108) {
	top_margin  = 13;
	left_margin = 98;
canon_cr2:
	height -= top_margin;
	width  -= left_margin;
  } else if (is_canon && raw_width == 5712) {
	height = 3752;
	width  = 5640;
	top_margin  = 20;
	left_margin = 62;
  } else if (!strcmp(model,"D1")) {
	cam_mul[0] *= 256/527.0;
	cam_mul[2] *= 256/317.0;
  } else if (!strcmp(model,"D1X")) {
	width -= 4;
	pixel_aspect = 0.5;
  } else if (!strcmp(model,"D40X") ||
		 !strcmp(model,"D60")  ||
		 !strcmp(model,"D80")) {
	height -= 3;
	width  -= 4;
  } else if (!strncmp(model,"D40",3) ||
		 !strncmp(model,"D50",3) ||
		 !strncmp(model,"D70",3)) {
	width--;
  } else if (!strcmp(model,"D100")) {
	if (tiff_compress == 34713 && !nikon_is_compressed()) {
	  load_raw = &CLASS nikon_load_raw;
	  raw_width = (width += 3) + 3;
	}
  } else if (!strcmp(model,"D200")) {
	left_margin = 1;
	width -= 4;
	filters = 0x94949494;
  } else if (!strncmp(model,"D2H",3)) {
	left_margin = 6;
	width -= 14;
  } else if (!strncmp(model,"D2X",3)) {
	if (width == 3264) width -= 32;
	else width -= 8;
  } else if (!strcmp(model,"D3")) {
	width -= 4;
	left_margin = 2;
  } else if (!strcmp(model,"D300")) {
	width -= 32;
  } else if (fsize == 1581060) {
	height = 963;
	width = 1287;
	raw_width = 1632;
	load_raw = &CLASS nikon_e900_load_raw;
	maximum = 0x3f4;
	colors = 4;
	filters = 0x1e1e1e1e;
	simple_coeff(3);
	pre_mul[0] = 1.2085;
	pre_mul[1] = 1.0943;
	pre_mul[3] = 1.1103;
  } else if (fsize == 2465792) {
	height = 1203;
	width  = 1616;
	raw_width = 2048;
	load_raw = &CLASS nikon_e900_load_raw;
	colors = 4;
	filters = 0x4b4b4b4b;
	adobe_coeff ("NIKON","E950");
  } else if (fsize == 4771840) {
	height = 1540;
	width  = 2064;
	colors = 4;
	filters = 0xe1e1e1e1;
	load_raw = &CLASS nikon_load_raw;
	if (!timestamp && nikon_e995())
	  strcpy (model, "E995");
	if (strcmp(model,"E995")) {
	  filters = 0xb4b4b4b4;
	  simple_coeff(3);
	  pre_mul[0] = 1.196;
	  pre_mul[1] = 1.246;
	  pre_mul[2] = 1.018;
	}
  } else if (!strcmp(model,"E2100")) {
	if (!timestamp && !nikon_e2100()) goto cp_e2500;
	height = 1206;
	width  = 1616;
	load_raw = &CLASS nikon_e2100_load_raw;
  } else if (!strcmp(model,"E2500")) {
cp_e2500:
	strcpy (model, "E2500");
	height = 1204;
	width  = 1616;
	colors = 4;
	filters = 0x4b4b4b4b;
  } else if (fsize == 4775936) {
	height = 1542;
	width  = 2064;
	load_raw = &CLASS nikon_e2100_load_raw;
	pre_mul[0] = 1.818;
	pre_mul[2] = 1.618;
	if (!timestamp) nikon_3700();
	if (model[0] == 'E' && atoi(model+1) < 3700)
	  filters = 0x49494949;
	if (!strcmp(model,"Optio 33WR")) {
	  flip = 1;
	  filters = 0x16161616;
	  pre_mul[0] = 1.331;
	  pre_mul[2] = 1.820;
	}
  } else if (fsize == 5869568) {
	height = 1710;
	width  = 2288;
	filters = 0x16161616;
	if (!timestamp && minolta_z2()) {
	  strcpy (make, "Minolta");
	  strcpy (model,"DiMAGE Z2");
	}
	if (make[0] == 'M')
	  load_raw = &CLASS nikon_e2100_load_raw;
  } else if (!strcmp(model,"E4500")) {
	height = 1708;
	width  = 2288;
	colors = 4;
	filters = 0xb4b4b4b4;
  } else if (fsize == 7438336) {
	height = 1924;
	width  = 2576;
	colors = 4;
	filters = 0xb4b4b4b4;
  } else if (fsize == 8998912) {
	height = 2118;
	width  = 2832;
	maximum = 0xf83;
	load_raw = &CLASS nikon_e2100_load_raw;
  } else if (!strcmp(model,"FinePix S5100") ||
		 !strcmp(model,"FinePix S5500")) {
	load_raw = &CLASS unpacked_load_raw;
  } else if (!strcmp(make,"FUJIFILM")) {
	if (!strcmp(model+7,"S2Pro")) {
	  strcpy (model+7," S2Pro");
	  height = 2144;
	  width  = 2880;
	  flip = 6;
	} else
	  maximum = 0x3e00;
	if (is_raw == 2 && shot_select)
	  maximum = 0x2f00;
	top_margin = (raw_height - height)/2;
	left_margin = (raw_width - width )/2;
	if (is_raw == 2)
	  data_offset += (shot_select > 0) * ( fuji_layout ?
		(raw_width *= 2) : raw_height*raw_width*2 );
	fuji_width = width >> !fuji_layout;
	width = (height >> fuji_layout) + fuji_width;
	raw_height = height;
	height = width - 1;
	load_raw = &CLASS fuji_load_raw;
	if (!(fuji_width & 1)) filters = 0x49494949;
  } else if (!strcmp(model,"RD175")) {
	height = 986;
	width = 1534;
	data_offset = 513;
	filters = 0x61616161;
	load_raw = &CLASS minolta_rd175_load_raw;
  } else if (!strcmp(model,"KD-400Z")) {
	height = 1712;
	width  = 2312;
	raw_width = 2336;
	goto konica_400z;
  } else if (!strcmp(model,"KD-510Z")) {
	goto konica_510z;
  } else if (!strcasecmp(make,"MINOLTA")) {
	load_raw = &CLASS unpacked_load_raw;
	maximum = 0xfff;
	if (!strncmp(model,"DiMAGE A",8)) {
	  if (!strcmp(model,"DiMAGE A200"))
	filters = 0x49494949;
	  load_raw = &CLASS packed_12_load_raw;
	} else if (!strncmp(model,"ALPHA",5) ||
		   !strncmp(model,"DYNAX",5) ||
		   !strncmp(model,"MAXXUM",6)) {
	  sprintf (model+20, "DYNAX %-10s", model+6+(model[0]=='M'));
	  adobe_coeff (make, model+20);
	  load_raw = &CLASS packed_12_load_raw;
	} else if (!strncmp(model,"DiMAGE G",8)) {
	  if (model[8] == '4') {
	height = 1716;
	width  = 2304;
	  } else if (model[8] == '5') {
konica_510z:
	height = 1956;
	width  = 2607;
	raw_width = 2624;
	  } else if (model[8] == '6') {
	height = 2136;
	width  = 2848;
	  }
	  data_offset += 14;
	  filters = 0x61616161;
konica_400z:
	  load_raw = &CLASS unpacked_load_raw;
	  maximum = 0x3df;
	  order = 0x4d4d;
	}
  } else if (!strcmp(model,"*ist DS")) {
	height -= 2;
  } else if (!strcmp(model,"K20D")) {
	filters = 0x16161616;
  } else if (!strcmp(model,"Optio S")) {
	if (fsize == 3178560) {
	  height = 1540;
	  width  = 2064;
	  load_raw = &CLASS eight_bit_load_raw;
	  cam_mul[0] *= 4;
	  cam_mul[2] *= 4;
	  pre_mul[0] = 1.391;
	  pre_mul[2] = 1.188;
	} else {
	  height = 1544;
	  width  = 2068;
	  raw_width = 3136;
	  load_raw = &CLASS packed_12_load_raw;
	  maximum = 0xf7c;
	  pre_mul[0] = 1.137;
	  pre_mul[2] = 1.453;
	}
  } else if (fsize == 6114240) {
	height = 1737;
	width  = 2324;
	raw_width = 3520;
	load_raw = &CLASS packed_12_load_raw;
	maximum = 0xf7a;
	pre_mul[0] = 1.980;
	pre_mul[2] = 1.570;
  } else if (!strcmp(model,"Optio 750Z")) {
	height = 2302;
	width  = 3072;
	load_raw = &CLASS nikon_e2100_load_raw;
  } else if (!strcmp(model,"STV680 VGA")) {
	height = 484;
	width  = 644;
	load_raw = &CLASS eight_bit_load_raw;
	flip = 2;
	filters = 0x16161616;
	black = 16;
	pre_mul[0] = 1.097;
	pre_mul[2] = 1.128;
  } else if (!strcmp(model,"KAI-0340")) {
	height = 477;
	width  = 640;
	order = 0x4949;
	data_offset = 3840;
	load_raw = &CLASS unpacked_load_raw;
	pre_mul[0] = 1.561;
	pre_mul[2] = 2.454;
  } else if (!strcmp(model,"N95")) {
	height = raw_height - (top_margin = 2);
  } else if (!strcmp(model,"531C")) {
	height = 1200;
	width  = 1600;
	load_raw = &CLASS unpacked_load_raw;
	filters = 0x49494949;
	pre_mul[1] = 1.218;
  } else if (!strcmp(model,"F-080C")) {
	height = 768;
	width  = 1024;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (!strcmp(model,"F-145C")) {
	height = 1040;
	width  = 1392;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (!strcmp(model,"F-201C")) {
	height = 1200;
	width  = 1600;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (!strcmp(model,"F-510C")) {
	height = 1958;
	width  = 2588;
	load_raw = fsize < 7500000 ?
	&CLASS eight_bit_load_raw : &CLASS unpacked_load_raw;
	maximum = 0xfff0;
  } else if (!strcmp(model,"F-810C")) {
	height = 2469;
	width  = 3272;
	load_raw = &CLASS unpacked_load_raw;
	maximum = 0xfff0;
  } else if (!strcmp(model,"XCD-SX910CR")) {
	height = 1024;
	width  = 1375;
	raw_width = 1376;
	filters = 0x49494949;
	maximum = 0x3ff;
	load_raw = fsize < 2000000 ?
	&CLASS eight_bit_load_raw : &CLASS unpacked_load_raw;
  } else if (!strcmp(model,"2010")) {
	height = 1207;
	width  = 1608;
	order = 0x4949;
	filters = 0x16161616;
	data_offset = 3212;
	maximum = 0x3ff;
	load_raw = &CLASS unpacked_load_raw;
  } else if (!strcmp(model,"A782")) {
	height = 3000;
	width  = 2208;
	filters = 0x61616161;
	load_raw = fsize < 10000000 ?
	&CLASS eight_bit_load_raw : &CLASS unpacked_load_raw;
	maximum = 0xffc0;
  } else if (!strcmp(model,"3320AF")) {
	height = 1536;
	raw_width = width = 2048;
	filters = 0x61616161;
	load_raw = &CLASS unpacked_load_raw;
	maximum = 0x3ff;
	pre_mul[0] = 1.717;
	pre_mul[2] = 1.138;
	fseek (ifp, 0x300000, SEEK_SET);
	if ((order = guess_byte_order(0x10000)) == 0x4d4d) {
	  height -= (top_margin = 16);
	  width -= (left_margin = 28);
	  maximum = 0xf5c0;
	  strcpy (make, "ISG");
	  model[0] = 0;
	}
  } else if (!strcmp(make,"Hasselblad")) {
	if (load_raw == &CLASS lossless_jpeg_load_raw)
	  load_raw = &CLASS hasselblad_load_raw;
	if (raw_width == 7262) {
	  height = 5444;
	  width  = 7248;
	  top_margin  = 4;
	  left_margin = 7;
	  filters = 0x61616161;
	}
  } else if (!strcmp(make,"Sinar")) {
	if (!memcmp(head,"8BPS",4)) {
	  fseek (ifp, 14, SEEK_SET);
	  height = get4();
	  width  = get4();
	  filters = 0x61616161;
	  data_offset = 68;
	}
	if (!load_raw) load_raw = &CLASS unpacked_load_raw;
	maximum = 0x3fff;
  } else if (!strcmp(make,"Leaf")) {
	maximum = 0x3fff;
	if (tiff_samples > 1) filters = 0;
	if (tiff_samples > 1 || tile_length < raw_height)
	  load_raw = &CLASS leaf_hdr_load_raw;
	if ((width | height) == 2048) {
	  if (tiff_samples == 1) {
	filters = 1;
	strcpy (cdesc, "RBTG");
	strcpy (model, "CatchLight");
	top_margin =  8; left_margin = 18; height = 2032; width = 2016;
	  } else {
	strcpy (model, "DCB2");
	top_margin = 10; left_margin = 16; height = 2028; width = 2022;
	  }
	} else if (width+height == 3144+2060) {
	  if (!model[0]) strcpy (model, "Cantare");
	  if (width > height) {
	 top_margin = 6; left_margin = 32; height = 2048;  width = 3072;
	filters = 0x61616161;
	  } else {
	left_margin = 6;  top_margin = 32;  width = 2048; height = 3072;
	filters = 0x16161616;
	  }
	  if (!cam_mul[0] || model[0] == 'V') filters = 0;
	  else is_raw = tiff_samples;
	} else if (width == 2116) {
	  strcpy (model, "Valeo 6");
	  height -= 2 * (top_margin = 30);
	  width -= 2 * (left_margin = 55);
	  filters = 0x49494949;
	} else if (width == 3171) {
	  strcpy (model, "Valeo 6");
	  height -= 2 * (top_margin = 24);
	  width -= 2 * (left_margin = 24);
	  filters = 0x16161616;
	}
  } else if (!strcmp(make,"LEICA") || !strcmp(make,"Panasonic")) {
	maximum = 0xfff0;
	if ((fsize-data_offset) / (width*8/7) == height)
	  load_raw = &CLASS panasonic_load_raw;
	if (!load_raw) load_raw = &CLASS unpacked_load_raw;
	switch (width) {
	  case 2568:
	adobe_coeff ("Panasonic","DMC-LC1");  break;
	  case 3130:
	left_margin = -14;
	  case 3170:
	left_margin += 18;
	width = 3096;
	if (height > 2326) {
	  height = 2326;
	  top_margin = 13;
	  filters = 0x49494949;
	}
	zero_is_bad = 1;
	adobe_coeff ("Panasonic","DMC-FZ8");  break;
	  case 3213:
	width -= 27;
	  case 3177:
	width -= 10;
	filters = 0x49494949;
	zero_is_bad = 1;
	adobe_coeff ("Panasonic","DMC-L1");  break;
	  case 3304:
	width -= 16;
	zero_is_bad = 1;
	adobe_coeff ("Panasonic","DMC-FZ30");  break;
	  case 3330:
	width = 3291;
	left_margin = 9;
	maximum = 0xf7f0;
	goto fz18;
	  case 3370:
	width = 3288;
	left_margin = 15;
fz18:	if (height > 2480)
		height = 2480 - (top_margin = 10);
	filters = 0x49494949;
	zero_is_bad = 1;
	break;
	  case 3690:
	height += 36;
	left_margin = -14;
	filters = 0x49494949;
	maximum = 0xf7f0;
	  case 3770:
	width = 3672;
	if ((height -= 39) == 2760)
	  top_margin = 15;
	left_margin += 17;
	zero_is_bad = 1;
	adobe_coeff ("Panasonic","DMC-FZ50");  break;
	  case 3710:
	width = 3682;
	filters = 0x49494949;
	break;
	  case 3880:
	width -= 22;
	left_margin = 6;
	zero_is_bad = 1;
	adobe_coeff ("Panasonic","DMC-LX1");  break;
	  case 4290:
	height += 38;
	left_margin = -14;
	filters = 0x49494949;
	  case 4330:
	width = 4248;
	if ((height -= 39) == 2400)
	  top_margin = 15;
	left_margin += 17;
	adobe_coeff ("Panasonic","DMC-LX2");  break;
	}
  } else if (!strcmp(model,"C770UZ")) {
	height = 1718;
	width  = 2304;
	filters = 0x16161616;
	load_raw = &CLASS nikon_e2100_load_raw;
  } else if (!strcmp(make,"OLYMPUS")) {
	height += height & 1;
	filters = exif_cfa;
	if (load_raw == &CLASS olympus_e410_load_raw) {
	  black >>= 4;
	} else if (!strcmp(model,"E-10") ||
		  !strncmp(model,"E-20",4)) {
	  black <<= 2;
	} else if (!strcmp(model,"E-300") ||
		   !strcmp(model,"E-500")) {
	  width -= 20;
	  if (load_raw == &CLASS unpacked_load_raw) {
	maximum = 0xfc30;
	black = 0;
	  }
	} else if (!strcmp(model,"E-330")) {
	  width -= 30;
	  if (load_raw == &CLASS unpacked_load_raw)
	maximum = 0xf790;
	} else if (!strcmp(model,"SP550UZ")) {
	  thumb_length = fsize - (thumb_offset = 0xa39800);
	  thumb_height = 480;
	  thumb_width  = 640;
	}
  } else if (!strcmp(model,"N Digital")) {
	height = 2047;
	width  = 3072;
	filters = 0x61616161;
	data_offset = 0x1a00;
	load_raw = &CLASS packed_12_load_raw;
  } else if (!strcmp(model,"DSC-F828")) {
	width = 3288;
	left_margin = 5;
	data_offset = 862144;
	load_raw = &CLASS sony_load_raw;
	filters = 0x9c9c9c9c;
	colors = 4;
	strcpy (cdesc, "RGBE");
  } else if (!strcmp(model,"DSC-V3")) {
	width = 3109;
	left_margin = 59;
	data_offset = 787392;
	load_raw = &CLASS sony_load_raw;
  } else if (!strcmp(make,"SONY") && raw_width == 3984) {
	adobe_coeff ("SONY","DSC-R1");
	width = 3925;
	order = 0x4d4d;
  } else if (!strcmp(model,"DSLR-A100")) {
	height--;
  } else if (!strcmp(model,"DSLR-A350")) {
	height -= 4;
  } else if (!strcmp(model,"C330")) {
	height = 1744;
	width  = 2336;
	raw_height = 1779;
	raw_width  = 2338;
	top_margin = 33;
	left_margin = 1;
	order = 0x4949;
	if ((data_offset = fsize - raw_height*raw_width)) {
	  fseek (ifp, 168, SEEK_SET);
	} else use_gamma = 0;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (!strcasecmp(make,"KODAK")) {
	if (filters == UINT_MAX) filters = 0x61616161;
	if (!strncmp(model,"NC2000",6)) {
	  width -= 4;
	  left_margin = 2;
	} else if (!strcmp(model,"EOSDCS3B")) {
	  width -= 4;
	  left_margin = 2;
	} else if (!strcmp(model,"EOSDCS1")) {
	  width -= 4;
	  left_margin = 2;
	} else if (!strcmp(model,"DCS420")) {
	  width -= 4;
	  left_margin = 2;
	} else if (!strcmp(model,"DCS460")) {
	  width -= 4;
	  left_margin = 2;
	} else if (!strcmp(model,"DCS460A")) {
	  width -= 4;
	  left_margin = 2;
	  colors = 1;
	  filters = 0;
	} else if (!strcmp(model,"DCS660M")) {
	  black = 214;
	  colors = 1;
	  filters = 0;
	} else if (!strcmp(model,"DCS760M")) {
	  colors = 1;
	  filters = 0;
	}
	if (strstr(model,"DC25")) {
	  strcpy (model, "DC25");
	  data_offset = 15424;
	}
	if (!strncmp(model,"DC2",3)) {
	  height = 242;
	  if (fsize < 100000) {
	raw_width = 256; width = 249;
	pixel_aspect = (4.0*height) / (3.0*width);
	  } else {
	raw_width = 512; width = 501;
	pixel_aspect = (493.0*height) / (373.0*width);
	  }
	  data_offset += raw_width + 1;
	  colors = 4;
	  filters = 0x8d8d8d8d;
	  simple_coeff(1);
	  pre_mul[1] = 1.179;
	  pre_mul[2] = 1.209;
	  pre_mul[3] = 1.036;
	  load_raw = &CLASS eight_bit_load_raw;
	} else if (!strcmp(model,"40")) {
	  strcpy (model, "DC40");
	  height = 512;
	  width  = 768;
	  data_offset = 1152;
	  load_raw = &CLASS kodak_radc_load_raw;
	} else if (strstr(model,"DC50")) {
	  strcpy (model, "DC50");
	  height = 512;
	  width  = 768;
	  data_offset = 19712;
	  load_raw = &CLASS kodak_radc_load_raw;
	} else if (strstr(model,"DC120")) {
	  strcpy (model, "DC120");
	  height = 976;
	  width  = 848;
	  pixel_aspect = height/0.75/width;
	  load_raw = tiff_compress == 7 ?
	&CLASS kodak_jpeg_load_raw : &CLASS kodak_dc120_load_raw;
	} else if (!strcmp(model,"DCS200")) {
	  thumb_height = 128;
	  thumb_width  = 192;
	  thumb_offset = 6144;
	  thumb_misc   = 360;
	  write_thumb = &CLASS layer_thumb;
	  height = 1024;
	  width  = 1536;
	  data_offset = 79872;
	  load_raw = &CLASS eight_bit_load_raw;
	  black = 17;
	}
  } else if (!strcmp(model,"Fotoman Pixtura")) {
	height = 512;
	width  = 768;
	data_offset = 3632;
	load_raw = &CLASS kodak_radc_load_raw;
	filters = 0x61616161;
	simple_coeff(2);
  } else if (!strcmp(model,"QuickTake 100")) {
	data_offset = 736;
	load_raw = &CLASS quicktake_100_load_raw;
	goto qt_common;
  } else if (!strcmp(model,"QuickTake 150")) {
	data_offset = 738 - head[5];
	if (head[5]) strcpy (model+10, "200");
	load_raw = &CLASS kodak_radc_load_raw;
qt_common:
	height = 480;
	width  = 640;
	filters = 0x61616161;
  } else if (!strcmp(make,"Rollei") && !load_raw) {
	switch (raw_width) {
	  case 1316:
	height = 1030;
	width  = 1300;
	top_margin  = 1;
	left_margin = 6;
	break;
	  case 2568:
	height = 1960;
	width  = 2560;
	top_margin  = 2;
	left_margin = 8;
	}
	filters = 0x16161616;
	load_raw = &CLASS rollei_load_raw;
	pre_mul[0] = 1.8;
	pre_mul[2] = 1.3;
  } else if (!strcmp(model,"PC-CAM 600")) {
	height = 768;
	data_offset = width = 1024;
	filters = 0x49494949;
	load_raw = &CLASS eight_bit_load_raw;
	pre_mul[0] = 1.14;
	pre_mul[2] = 2.73;
  } else if (!strcmp(model,"QV-2000UX")) {
	height = 1208;
	width  = 1632;
	data_offset = width * 2;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (fsize == 3217760) {
	height = 1546;
	width  = 2070;
	raw_width = 2080;
	load_raw = &CLASS eight_bit_load_raw;
  } else if (!strcmp(model,"QV-4000")) {
	height = 1700;
	width  = 2260;
	load_raw = &CLASS unpacked_load_raw;
	maximum = 0xffff;
  } else if (!strcmp(model,"QV-5700")) {
	height = 1924;
	width  = 2576;
	load_raw = &CLASS casio_qv5700_load_raw;
  } else if (!strcmp(model,"QV-R41")) {
	height = 1720;
	width  = 2312;
	raw_width = 3520;
	left_margin = 2;
  } else if (!strcmp(model,"QV-R51")) {
	height = 1926;
	width  = 2580;
	raw_width = 3904;
	pre_mul[0] = 1.340;
	pre_mul[2] = 1.672;
  } else if (!strcmp(model,"EX-S100")) {
	height = 1544;
	width  = 2058;
	raw_width = 3136;
	pre_mul[0] = 1.631;
	pre_mul[2] = 1.106;
  } else if (!strcmp(model,"EX-Z50")) {
	height = 1931;
	width  = 2570;
	raw_width = 3904;
	pre_mul[0] = 2.529;
	pre_mul[2] = 1.185;
  } else if (!strcmp(model,"EX-Z55")) {
	height = 1960;
	width  = 2570;
	raw_width = 3904;
	pre_mul[0] = 1.520;
	pre_mul[2] = 1.316;
  } else if (!strcmp(model,"EX-P505")) {
	height = 1928;
	width  = 2568;
	raw_width = 3852;
	maximum = 0xfff;
	pre_mul[0] = 2.07;
	pre_mul[2] = 1.88;
  } else if (fsize == 9313536) {	/* EX-P600 or QV-R61 */
	height = 2142;
	width  = 2844;
	raw_width = 4288;
	pre_mul[0] = 1.797;
	pre_mul[2] = 1.219;
  } else if (!strcmp(model,"EX-P700")) {
	height = 2318;
	width  = 3082;
	raw_width = 4672;
	pre_mul[0] = 1.758;
	pre_mul[2] = 1.504;
  }
  if (!model[0])
	sprintf (model, "%dx%d", width, height);
  if (filters == UINT_MAX) filters = 0x94949494;
  if (raw_color) adobe_coeff (make, model);
  if (thumb_offset && !thumb_height) {
	fseek (ifp, thumb_offset, SEEK_SET);
	if (ljpeg_start (&jh, 1)) {
	  thumb_width  = jh.wide;
	  thumb_height = jh.high;
	}
  }
dng_skip:
  if (!load_raw || height < 22) is_raw = 0;
#ifdef NO_JPEG
  if (load_raw == kodak_jpeg_load_raw) {
	fprintf (stderr,_("%s: You must link dcraw with libjpeg!!\n"), ifname);
	is_raw = 0;
  }
#endif
  if (!cdesc[0])
	strcpy (cdesc, colors == 3 ? "RGB":"GMCY");
  if (!raw_height) raw_height = height;
  if (!raw_width ) raw_width  = width;
  if (filters && colors == 3)
	for (i=0; i < 32; i+=4) {
	  if ((filters >> i & 15) == 9)
	filters |= 2 << i;
	  if ((filters >> i & 15) == 6)
	filters |= 8 << i;
	}
notraw:
  if (flip == -1) flip = tiff_flip;
  if (flip == -1) flip = 0;
}

void CLASS jpeg_thumb(CJPEGImage** Image, bool& bOutOfMemory)
{
	int kkk = 0;
	char *thumb;
	
	thumb = (char *) malloc (thumb_length);
	if (thumb == NULL) {
		bOutOfMemory = true;
		*Image = NULL;
		return;
	}

	fread(thumb, 1, thumb_length, ifp);

	int nWidth, nHeight, nBPP;
	void* pPixelData = TurboJpeg::ReadImage(nWidth, nHeight, nBPP, bOutOfMemory, thumb, thumb_length);


	if (pPixelData != NULL && (nBPP == 3 || nBPP == 1))
	{
		*Image = new CJPEGImage(nWidth, nHeight, pPixelData, Helpers::FindEXIFBlock(thumb, thumb_length), nBPP, Helpers::CalculateJPEGFileHash(thumb, thumb_length), IF_JPEG);
		(*Image)->SetJPEGComment(Helpers::GetJPEGComment(thumb, thumb_length));
	}
	else if (bOutOfMemory)
	{
		bOutOfMemory = true;
	}
		else
	{
		// failed, try GDI+
		delete[] pPixelData;
	}
	
	free(thumb);
}

int dcraw_main (LPCTSTR filename, CJPEGImage** Image, bool& bOutOfMemory)
{
	int status = 1;
	image = 0;
	oprof = 0;
	meta_data = 0;

#ifndef _UNICODE
	CHAR *fn = (CHAR *)filename;
#else
	int lenFileName = wcslen(filename) * 2;
	CHAR *fn = new CHAR[lenFileName];
	WideCharToMultiByte(CP_ACP, 0, filename, wcslen(filename), fn, lenFileName, NULL, NULL);
#endif

	ifname = fn;
	if (!(ifp = _tfopen(filename, _T("rb")))) {
	  goto cleanup;
	}
	status = (identify(),!is_raw);

	write_fun = write_thumb;
	if ((status = !thumb_offset)) {
		goto cleanup;
	} else if (thumb_load_raw) {
		goto cleanup;
	} else {
		fseek (ifp, thumb_offset, SEEK_SET);
	}

	(*write_fun)(Image, bOutOfMemory);
	fclose(ifp);

cleanup:
	if (meta_data) free (meta_data);
	if (oprof) free (oprof);
	if (image) free (image);

#ifdef _UNICODE
	delete fn;
#endif

	return status;
}


CJPEGImage* CReaderRAW::ReadRawImage(LPCTSTR strFileName, bool& bOutOfMemory)
{
	bOutOfMemory = false;
	CJPEGImage *Image = NULL;
	dcraw_main(strFileName, &Image, bOutOfMemory);
	return Image;
}
