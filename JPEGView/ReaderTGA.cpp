#include "StdAfx.h"
#include "ReaderTGA.h"
#include "JPEGImage.h"
#include "Helpers.h"
#include "BasicProcessing.h"

// The TGA reader has been adapted and extended from the TGA reader used in an example of the BOINC project
// http://www.filewatcher.com/p/boinc-server-maker_7.0.27+dfsg-5_armhf.deb.5191030/usr/share/doc/boinc-server-maker/examples/tgalib.h.html
// The reader supports 8, 16, 24 and 32 bit TGAs

// The following types are supported
#define TGA_INDEXED	 1		// This tells us it's a indexed 8 bit file
#define TGA_RGB		 2		// This tells us it's a normal RGB (really BGR) file
#define TGA_MONO	 3		// This tells us it's a monochrome file
#define TGA_RLE_INDEXED		9   // This tells us it's a indexed 8 bit Run-Length Encoded (RLE) file
#define TGA_RLE_RGB		10		// This tells us that the targa is Run-Length Encoded (RLE) RGB file
#define TGA_RLE_MONO	11		// This tells us that the targa is Run-Length Encoded (RLE) monochrome file

#define ALPHA_OPAQUE 0xFF000000 // Used for masking out the alpha channel from a 32 bit RGBA pixel

// Checks if all alpha values in a 32 bit image are zero. If yes the alpha channel should be ignored as the image
// would just be black.
static bool IsAlphaChannelValid(int width, int height, uint32* pImageData)
{
    uint32 pixel = 0;
    for (int i = 0; i < width*height; i++)
    {
        pixel = pixel | (*pImageData & ALPHA_OPAQUE);
        pImageData++;
    }
    return pixel != 0;
}

static inline uint32 AlphaBlendBackground(uint32 pixel, uint32 backgroundColor)
{
    uint32 alpha = pixel & ALPHA_OPAQUE;
    if (alpha == 0) {
        return backgroundColor;
    } else if (alpha == ALPHA_OPAQUE)
    {
        return pixel;
    } else {
        uint8 b = GetBValue(pixel);
        uint8 g = GetGValue(pixel);
        uint8 r = GetRValue(pixel);
        uint8 bg_r = GetRValue(backgroundColor);
        uint8 bg_g = GetGValue(backgroundColor);
        uint8 bg_b = GetBValue(backgroundColor);
        uint8 a = alpha >> 24;
        uint8 one_minus_a = 255 - a;
        return
            ((r * a + bg_r * one_minus_a) >> 8) + 
            (((g * a + bg_g * one_minus_a) >> 8) << 8) + 
            (((b * a + bg_b * one_minus_a) >> 8) << 16) + ALPHA_OPAQUE;
    }
}


CJPEGImage* CReaderTGA::ReadTgaImage(LPCTSTR strFileName, COLORREF backgroundColor, bool& bOutOfMemory) {

    const int MAX_IMAGE_SIZE_BYTES = 256 * 1024 * 1024; // 256 MB
    const int MAX_IMAGE_DIMENSION = 65535;

    bOutOfMemory = false;

	WORD width = 0, height = 0;			// The dimensions of the image
    WORD colormapStart = 0;             // Start of color map after header (in bytes)
    WORD colormapLen = 0;               // Length of color map in bytes
    byte colormapBits = 0;              // Bits per color map entry
	byte length = 0;					// The length in bytes to the pixels
	byte imageType = 0;					// The image type (RLE, RGB, Alpha...)
	byte bits = 0;						// The bits per pixel for the image (16, 24, 32)
    byte attributes = 0;                // Image attributes
	FILE *pFile = NULL;					// The file pointer
	int channels = 0;					// The channels of the image (3 = RGA : 4 = RGBA)
	int stride = 0;						// The stride (channels * width)
	int i = 0;							// A counter

	// Open a file pointer to the targa file and check if it was found and opened 
	if((pFile = _tfopen(strFileName, _T("rb"))) == NULL) 
	{
		return NULL;
	}

	// Read in the length in bytes from the header to the pixel data
	fread(&length, sizeof(byte), 1, pFile);
	
	// Jump over one byte
	fseek(pFile,1,SEEK_CUR); 

	// Read in the imageType (RLE, RGB, etc...)
	fread(&imageType, sizeof(byte), 1, pFile);

    bool isIndexed = imageType == TGA_INDEXED || imageType == TGA_RLE_INDEXED;

    // Read in palette info
    fread(&colormapStart, sizeof(WORD), 1, pFile);
    fread(&colormapLen, sizeof(WORD), 1, pFile);
    fread(&colormapBits, sizeof(byte), 1, pFile);
	
	// Skip past general information we don't care about
	fseek(pFile, 4, SEEK_CUR); 

	// Read the width, height and bits per pixel (16, 24 or 32)
	fread(&width,  sizeof(WORD), 1, pFile);
	fread(&height, sizeof(WORD), 1, pFile);
	fread(&bits,   sizeof(byte), 1, pFile);
    fread(&attributes, sizeof(byte), 1, pFile);

    bool flipVertically = ((attributes >> 5) & 1) == 0;

    // check preconditions: 
    // - image size valid
    // - supported bits per pixel - only 8, 15, 16, 24 and 32 bits supported
    // - no strange image attributes
    // - supported image types - only RGB, mono and indexed
    // - color map format must be 256 RGB entries
    if (width <= 0 || height <= 0 || width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION ||
        (bits != 8 && bits != 16 && bits != 15 && bits != 24 && bits != 32) ||
        (imageType != TGA_INDEXED && imageType != TGA_RGB && imageType != TGA_MONO && imageType != TGA_RLE_INDEXED && imageType != TGA_RLE_RGB && imageType != TGA_RLE_MONO) ||
        ((imageType == TGA_MONO || imageType == TGA_RLE_MONO) && bits != 8) ||
        (isIndexed && (colormapStart != 0 || colormapLen != 256 || colormapBits != 24)))
    {
        fclose(pFile);
        return NULL;
    }

    // check memory footprint
    int targetChannels = (bits == 32) ? 4 : 3;
    int targetStride = Helpers::DoPadding(width * targetChannels, 4);
    double numberOfBytesRequired = (double)targetStride * height;
    if (numberOfBytesRequired > MAX_IMAGE_SIZE_BYTES)
    {
        bOutOfMemory = true;
        fclose(pFile);
        return NULL;
    }
	
    // Allocate memory which will hold our image data
	byte* pImageData = new(std::nothrow) byte[(uint32)numberOfBytesRequired];
    if (pImageData == NULL)
    {
        bOutOfMemory = true;
        fclose(pFile);
        return NULL;
    }

	// Now we move the file pointer to the pixel data
	if (fseek(pFile, length, SEEK_CUR) != 0)
    {
        fclose(pFile);
        return NULL;
    }

    // read the color map
    byte palette[768];
    if (isIndexed)
    {  
        fread(&palette, 768, 1, pFile);
    }

    byte* pImage = pImageData;

    if(imageType == TGA_MONO)
	{
        // this is always 8 bpp (checked above)
        for(int y = 0; y < height; y++)
		{
            uint8* pLine = pImage;
            for(int x = 0; x < width; x++)
            {
                byte grey;
                fread(&grey, sizeof(byte), 1, pFile);
                *pLine++ = grey;
                *pLine++ = grey;
                *pLine++ = grey;
            }
            pImage += targetStride;
        }
    }
    else if(imageType == TGA_INDEXED)
	{
        for(int y = 0; y < height; y++)
		{
            uint8* pLine = pImage;
            for(int x = 0; x < width; x++)
            {
                byte index;
                fread(&index, sizeof(byte), 1, pFile);
                *pLine++ = palette[index*3];
                *pLine++ = palette[index*3 + 1];
                *pLine++ = palette[index*3 + 2];
            }
            pImage += targetStride;
        }
    }
    else if(imageType == TGA_RGB)
	{
		// Check if the image is a 24 or 32-bit image
		if(bits == 24 || bits == 32)
		{
			// Calculate the channels (3 or 4)
			// Next, we calculate the stride and allocate enough memory for on pixel line.
			channels = bits / 8;
			stride = channels * width;

			// Load in all the pixel data line by line
			for(int y = 0; y < height; y++)
			{
                fread(pImage, stride, 1, pFile);
                pImage += targetStride;
            }
		}
		// Check if the image is a 16 bit image (RGB stored in 1 unsigned short)
		else if(bits == 16 || bits == 15)
		{
			unsigned short pixel = 0;
			int r=0, g=0, b=0;

			// Since we convert 16-bit images to 24 bit, we hardcode the channels to 3.
			// We then calculate the stride and allocate memory for the pixels.
			channels = 3;
			stride = channels * width;

			// Load in all the pixel data pixel by pixel
            for(int y = 0; y < height; y++)
            {
                uint8* pLine = pImage;
			    for(int i = 0; i < width; i++)
			    {
				    // Read in the current pixel
				    fread(&pixel, sizeof(unsigned short), 1, pFile);
				
				    // To convert a 16-bit pixel into an R, G, B, we need to
				    // do some masking and such to isolate each color value.
				    // 0x1f = 11111 in binary, so since 5 bits are reserved in
				    // each unsigned short for the R, G and B, we bit shift and mask
				    // to find each value.  We then bit shift up by 3 to get the full color.
				    b = (pixel & 0x1f) << 3;
				    g = ((pixel >> 5) & 0x1f) << 3;
				    r = ((pixel >> 10) & 0x1f) << 3;
				
				    *pLine++ = b;
				    *pLine++ = g;
				    *pLine++ = r;
			    }
                pImage += targetStride;
            }
		}
	}
	// Else, it must be Run-Length Encoded (RLE)
	else
	{
		// First, let me explain real quickly what RLE is.  
		// For further information, check out Paul Bourke's intro article at: 
		// http://astronomy.swin.edu.au/~pbourke/dataformats/rle/
		// 
		// Anyway, we know that RLE is a basic type compression.  It takes
		// colors that are next to each other and then shrinks that info down
		// into the color and a integer that tells how much of that color is used.
		// For instance:
		// aaaaabbcccccccc would turn into a5b2c8
		// Well, that's fine and dandy and all, but how is it down with RGB colors?
		// Simple, you read in an color count (rleID), and if that number is less than 128,
		// it does NOT have any optimization for those colors, so we just read the next
		// pixels normally.  Say, the color count was 28, we read in 28 colors like normal.
		// If the color count is over 128, that means that the next color is optimized and
		// we want to read in the same pixel color for a count of (colorCount - 127).
		// It's 127 because we add 1 to the color count, as you'll notice in the code.

		// Create some variables to hold the rleID, current colors read, channels, & stride.
		byte rleID = 0;
		int colorsRead = 0;
		channels = bits / 8;
        int x = 0;
        int padding = targetStride - targetChannels * width;

        uint8 colors[4];
		byte* pColors = (byte*)colors;
        byte* pImage = pImageData;

		// Load in all the pixel data
        int numPixels = width*height;
		while(i < numPixels)
		{
			// Read in the current color count + 1
			fread(&rleID, sizeof(byte), 1, pFile);
			
			// Check if we don't have an encoded string of colors
            bool useSameColor;
			if(rleID < 128)
			{
				// Increase the count by 1
				rleID++;
                useSameColor = false;
            }
            // Else, let's read in a string of the same character
			else
			{
				// Minus the 128 ID + 1 (127) to get the color count that needs to be read
				rleID -= 127;
                useSameColor = true;

				// Read in the current color, which is the same for a while
				fread(pColors, sizeof(byte) * channels, 1, pFile);
            }

			// Go through and read all the unique colors found
			while(rleID && i < numPixels)
			{
                if (!useSameColor)
                {
				    // Read in the current color
				    fread(pColors, sizeof(byte) * channels, 1, pFile);
                }

                if(bits == 32)
                {
                    *((uint32*)(pImage + colorsRead)) = *((uint32*)pColors);
                }
                else
                {
                    if (bits == 8)
                    {
                        if (isIndexed)
                        {
                            int index = pColors[0] * 3;
                            pImage[colorsRead + 0] = palette[index];
					        pImage[colorsRead + 1] = palette[index + 1];
					        pImage[colorsRead + 2] = palette[index + 2];
                        } else {
                            pImage[colorsRead + 0] = pColors[0];
					        pImage[colorsRead + 1] = pColors[0];
					        pImage[colorsRead + 2] = pColors[0];
                        }
                    } else {
					    pImage[colorsRead + 0] = pColors[0];
					    pImage[colorsRead + 1] = pColors[1];
					    pImage[colorsRead + 2] = pColors[2];
                    }

                    // do padding target lines when 24 bpp
                    x++;
                    if (x == width)
                    {
                        x = 0;
                        colorsRead += padding;
                    }
                }

				// Increase the current pixels read, decrease the amount
				// of pixels left, and increase the starting index for the next pixel.
				i++;
				rleID--;
				colorsRead += targetChannels;
			}
				
		} // end of RLE pixel loop
	}

	// Close the file pointer that opened the file
	fclose(pFile);

    // If image needs to be flipped, do this inplace
    if (flipVertically)
    {
        CBasicProcessing::MirrorVInplace(width, height, targetStride, pImageData);
    }

    // 32 bit image - check alpha channel for validity and multiply RGB with alpha if valid
    if (targetChannels == 4)
    {
        uint32* pImage32 = (uint32*)pImageData;
        if (IsAlphaChannelValid(width, height, (uint32*)pImageData))
        {
            for (int i = 0; i < width*height; i++)
            {
                *pImage32++ = AlphaBlendBackground(*pImage32, backgroundColor | ALPHA_OPAQUE);
            }
        }
        else
        {
            // no valid alpha channel - set all A to 255
            for (int i = 0; i < width*height; i++)
            {
                *pImage32++ = *pImage32 | ALPHA_OPAQUE;
            }
        }
    }

    CJPEGImage* pTargetImage = new CJPEGImage(width, height, pImageData, NULL, targetChannels, 
		0, IF_TGA, false, 0, 1, 0);

    return pTargetImage;
}