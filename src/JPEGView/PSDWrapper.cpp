/*  This file uses code adapted from SAIL (https://github.com/HappySeaFox/sail/blob/master/src/sail-codecs/psd/psd.c)
	See the original copyright notice below:

	Copyright (c) 2022 Dmitry Baryshev

	The MIT License

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

/* Documentation of the PSD file format can be found here: https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
	Tags can also be found here: https://exiftool.org/TagNames/Photoshop.html

	Useful image resources:
	0x0409 1033 (Photoshop 4.0) Thumbnail resource for Photoshop 4.0 only.See See Thumbnail resource format.
	0x040C 1036 (Photoshop 5.0) Thumbnail resource(supersedes resource 1033).See See Thumbnail resource format.
	0x040F 1039 (Photoshop 5.0) ICC Profile.The raw bytes of an ICC(International Color Consortium) format profile.See ICC1v42_2006 - 05.pdf in the Documentation folder and icProfileHeader.h in Sample Code\Common\Includes .
	0x0411 1041 (Photoshop 5.0) ICC Untagged Profile. 1 byte that disables any assumed profile handling when opening the file. 1 = intentionally untagged.
	0x0417 1047 (Photoshop 6.0) Transparency Index. 2 bytes for the index of transparent color, if any.
	0x0419 1049 (Photoshop 6.0) Global Altitude. 4 byte entry for altitude
	0x041D 1053 (Photoshop 6.0) Alpha Identifiers. 4 bytes of length, followed by 4 bytes each for every alpha identifier.
	Get alpha identifier and look at its index number, if not 0 abort
	0x0421 1057 (Photoshop 6.0) Version Info. 4 bytes version, 1 byte hasRealMergedData, Unicode string : writer name, Unicode string : reader name, 4 bytes file version.
	0x0422 1058 (Photoshop 7.0) EXIF data 1. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
	0x0423 1059 (Photoshop 7.0) EXIF data 3. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
	Not sure what 0x0423 is.
*/

#include "stdafx.h"
#include "PSDWrapper.h"
#include "MaxImageDef.h"
#include "Helpers.h"
#include "TJPEGWrapper.h"
#include "ICCProfileTransform.h"
#include "SettingsProvider.h"


// Throw exception if bShouldThrow is true. Setting a breakpoint in here is useful for debugging
static inline void ThrowIf(bool bShouldThrow) {
	if (bShouldThrow) {
		throw 1;
	}
}

// Read exactly sz bytes of the file into p
static inline void ReadFromFile(void* dst, HANDLE file, DWORD sz) {
	unsigned int nNumBytesRead;
	ThrowIf(!(::ReadFile(file, dst, sz, (LPDWORD)&nNumBytesRead, NULL) && nNumBytesRead == sz));
}

// Read and return an unsigned int from file
static inline unsigned int ReadUIntFromFile(HANDLE file) {
	unsigned int val;
	ReadFromFile(&val, file, 4);
	return _byteswap_ulong(val);
}

// Read and return an unsigned short from file
static inline unsigned short ReadUShortFromFile(HANDLE file) {
	unsigned short val;
	ReadFromFile(&val, file, 2);
	return _byteswap_ushort(val);
}

// Read and return an unsigned char from file
static inline unsigned short ReadUCharFromFile(HANDLE file) {
	unsigned char val;
	ReadFromFile(&val, file, 1);
	return val;
}

// Move file pointer by offset from current position
static inline void SeekFile(HANDLE file, LONG offset) {
	ThrowIf(::SetFilePointer(file, offset, NULL, 1) == INVALID_SET_FILE_POINTER);
}

// Move file pointer to offset from beginning of file
static inline void SeekFileFromStart(HANDLE file, LONG offset) {
	ThrowIf(::SetFilePointer(file, offset, NULL, 0) == INVALID_SET_FILE_POINTER);
}

CJPEGImage* PsdReader::ReadImage(LPCTSTR strFileName, bool& bOutOfMemory)
{
	HANDLE hFile;
	hFile = ::CreateFile(strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	char* pBuffer = NULL;
	void* pPixelData = NULL;
	void* pEXIFData = NULL;
	char* pICCProfile = NULL;
	unsigned int nICCProfileSize = 0;
	void* transform = NULL;
	CJPEGImage* Image = NULL;
	try {
		unsigned int nFileSize = 0;
		nFileSize = ::GetFileSize(hFile, NULL);
		ThrowIf(nFileSize > MAX_PSD_FILE_SIZE);

		// Skip file signature
		SeekFile(hFile, 4);

		// Read version: 1 for PSD, 2 for PSB
		unsigned short nVersion = ReadUShortFromFile(hFile);
		ThrowIf(nVersion != 1);

		// Check reserved bytes
		char pReserved[6];
		ReadFromFile(pReserved, hFile, 6);
		ThrowIf(memcmp(pReserved, "\0\0\0\0\0\0", 6));

		// Read number of channels
		unsigned short nRealChannels = ReadUShortFromFile(hFile);

		// Read width and height
		unsigned int nHeight = ReadUIntFromFile(hFile);
		unsigned int nWidth = ReadUIntFromFile(hFile);
		if ((double)nHeight * nWidth > MAX_IMAGE_PIXELS) {
			bOutOfMemory = true;
		}
		ThrowIf(bOutOfMemory || nHeight > MAX_IMAGE_DIMENSION || nWidth > MAX_IMAGE_DIMENSION);

		// PSD can have bit depths of 1, 2, 4, 8, 16, 32
		unsigned short nBitDepth = ReadUShortFromFile(hFile);
		// Only 8-bit is supported for now
		ThrowIf(nBitDepth != 8);

		
		// Read color mode
		// Bitmap = 0; Grayscale = 1; Indexed = 2; RGB = 3; CMYK = 4; Multichannel = 7; Duotone = 8; Lab = 9.
		// TODO: NegateCMYK
		unsigned short nChannels = 0;
		unsigned short nColorMode = ReadUShortFromFile(hFile);
		switch (nColorMode) {
			case MODE_Grayscale:
			case MODE_Duotone:
				nChannels = min(nRealChannels, 1);
				break;
			case MODE_Multichannel:
				nChannels = min(nRealChannels, 3);
				break;
			case MODE_Lab:
			case MODE_RGB:
			case MODE_CMYK:
				nChannels = min(nRealChannels, 4);
				break;
		}
		if (nChannels == 2) {
			nChannels = 1;
		}
		ThrowIf(nChannels != 1 && nChannels != 3 && nChannels != 4);

		// Skip color mode data
		unsigned int nColorDataSize = ReadUIntFromFile(hFile);
		SeekFile(hFile, nColorDataSize);

		// Read resource section size
		unsigned int nResourceSectionSize = ReadUIntFromFile(hFile);

		// This default value should detect alpha channels for PSDs created by programs which don't save alpha identifiers (e.g. Krita, GIMP)
		bool bUseAlpha = nChannels == 4;

		for (;;) {
			// Resource block signature
			try {
				if (ReadUIntFromFile(hFile) != 0x3842494D) { // "8BIM"
					break;
				}
			} catch (...) {
				break;
			}

			// Resource ID
			unsigned short nResourceID = ReadUShortFromFile(hFile);

			// Skip Pascal string (padded to be even length)
			while (ReadUShortFromFile(hFile));

			// Resource size
			unsigned int nResourceSize = ReadUIntFromFile(hFile);

			// Parse image resources
			switch (nResourceID) {
				case 0x040F: // ICC Profile
					if (nColorMode == MODE_RGB) {
						pICCProfile = new(std::nothrow) char[nResourceSize];
					}
					if (pICCProfile != NULL) {
						ReadFromFile(pICCProfile, hFile, nResourceSize);
						SeekFile(hFile, -nResourceSize);
						nICCProfileSize = nResourceSize;
					}
					break;
				case 0x041D: // 0x041D 1053 (Photoshop 6.0) Alpha Identifiers. 4 bytes of length, followed by 4 bytes each for every alpha identifier.
					if (bUseAlpha) {
						bUseAlpha = false;
						int i = 0;
						while (i < nResourceSize / 4) {
							i++;
							if (ReadUIntFromFile(hFile) == 0) {
								bUseAlpha = true;
								break;
							}
						}
						SeekFile(hFile, -i * 4);
					}
					break;
				case 0x0421: // 0x0421 1057 (Photoshop 6.0) Version Info. 4 bytes version, 1 byte hasRealMergedData, Unicode string : writer name, Unicode string : reader name, 4 bytes file version.
					if (nResourceSize >= 5) {
						ReadUIntFromFile(hFile);
						// See https://exiftool.org/forum/index.php?topic=12897.0
						ThrowIf(!ReadUCharFromFile(hFile));
						SeekFile(hFile, -5);
					}
					break;
				case 0x0422: // 0x0422 1058 (Photoshop 7.0) EXIF data 1. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
				case 0x0423: // 0x0423 1059 (Photoshop 7.0) EXIF data 3. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
					if (pEXIFData == NULL && nResourceSize < 65526) {
						pEXIFData = new(std::nothrow) char[nResourceSize + 10];
						if (pEXIFData != NULL) {
							memcpy(pEXIFData, "\xFF\xE1\0\0Exif\0\0", 10);
							*((unsigned short*)pEXIFData + 1) = _byteswap_ushort(nResourceSize + 8);
							ReadFromFile((char*)pEXIFData + 10, hFile, nResourceSize);
							SeekFile(hFile, -nResourceSize);
						}
					}
					break;
			}

			// Skip resource data (padded to be even length)
			SeekFile(hFile, (nResourceSize + 1) & -2);
		}
		
		// Go back to start of file
		SeekFileFromStart(hFile, 26 + 4 + nColorDataSize + 4 + nResourceSectionSize);

		// Skip Layer and Mask Info section
		unsigned int nLayerSize = ReadUIntFromFile(hFile);
		ReadUIntFromFile(hFile);
		short nLayerCount = ReadUShortFromFile(hFile);
		bUseAlpha = bUseAlpha && (nLayerCount <= 0);
		SeekFile(hFile, nLayerSize - 6);

		// Compression. 0 = Raw Data, 1 = RLE compressed, 2 = ZIP without prediction, 3 = ZIP with prediction.
		unsigned short nCompressionMethod = ReadUShortFromFile(hFile);
		ThrowIf(nCompressionMethod != COMPRESSION_RLE && nCompressionMethod != COMPRESSION_None);

		unsigned int nImageDataSize = nFileSize - (26 + 4 + nColorDataSize + 4 + nResourceSectionSize + 4 + nLayerSize + 2);
		pBuffer = new(std::nothrow) char[nImageDataSize];
		if (pBuffer == NULL) {
			bOutOfMemory = true;
			ThrowIf(true);
		}
		ReadFromFile(pBuffer, hFile, nImageDataSize);

		if (!bUseAlpha && nColorMode != MODE_CMYK) {
			nChannels = min(nChannels, 3);
		}

		// Apply ICC Profile
		if (nChannels == 3 || nChannels == 4) {
			if (nColorMode == MODE_Lab) {
				transform = ICCProfileTransform::CreateLabTransform(nChannels == 4 ? ICCProfileTransform::FORMAT_LabA : ICCProfileTransform::FORMAT_Lab);
				if (transform == NULL) {
					// If we can't convert Lab to sRGB then just use the Lightness channel as grayscale
					nChannels = min(nChannels, 1);
				}
			} else if (nColorMode == MODE_RGB) {
				transform = ICCProfileTransform::CreateTransform(pICCProfile, nICCProfileSize, nChannels == 4 ? ICCProfileTransform::FORMAT_BGRA : ICCProfileTransform::FORMAT_BGR);
			}
		}

		int nRowSize = Helpers::DoPadding(nWidth * nChannels, 4);
		pPixelData = new(std::nothrow) char[nRowSize * nHeight];
		if (pPixelData == NULL) {
			bOutOfMemory = true;
			ThrowIf(true);
		}
		// TODO: non-8bit, better non-RGB support
		// non-8bit must first be decompressed as arbitrary data
		// TODO: continue next row at end of row bytes (to support corrupt images)
		unsigned char* p = (unsigned char*)pBuffer;
		if (nCompressionMethod == COMPRESSION_RLE) {
			// Skip byte counts for scanlines
			p += nHeight * nRealChannels * 2;
			for (unsigned channel = 0; channel < nChannels; channel++) {
				unsigned rchannel;
				if (nColorMode == MODE_Lab) {
					rchannel = channel;
				} else {
					rchannel = (-channel - 2) % nChannels;
				}
				for (unsigned row = 0; row < nHeight; row++) {
					for (unsigned count = 0; count < nWidth; ) {
						unsigned char c;
						ThrowIf(p >= (unsigned char*)pBuffer + nImageDataSize);
						c = *p;
						p += 1;

						if (c > 128) {
							c = ~c + 2;

							ThrowIf(p >= (unsigned char*)pBuffer + nImageDataSize);
							unsigned char value = *p;
							p += 1;

							for (unsigned i = count; i < count + c; i++) {
								unsigned char* scan = (unsigned char*)pPixelData + row * nRowSize + i * nChannels;
								unsigned char* pixel = scan + rchannel;
								ThrowIf(pixel >= (unsigned char*)pPixelData + nRowSize * nHeight);
								*pixel = value;
							}
						} else if (c < 128) {
							c++;

							for (unsigned i = count; i < count + c; i++) {
								ThrowIf(p >= (unsigned char*)pBuffer + nImageDataSize);
								unsigned char value = *p;
								p += 1;

								unsigned char* scan = (unsigned char*)pPixelData + row * nRowSize + i * nChannels;
								unsigned char* pixel = scan + rchannel;
								ThrowIf(pixel >= (unsigned char*)pPixelData + nRowSize * nHeight);
								*pixel = value;
							}
						}

						count += c;
					}
				}
			}
		} else { // No compression
			for (unsigned channel = 0; channel < nChannels; channel++) {
				unsigned rchannel;
				if (nColorMode == MODE_Lab) {
					rchannel = channel;
				} else {
					rchannel = (-channel - 2) % nChannels;
				}
				for (unsigned row = 0; row < nHeight; row++) {
					for (unsigned count = 0; count < nWidth; count++) {
						ThrowIf(p >= (unsigned char*)pBuffer + nImageDataSize);
						unsigned char value = *p;
						p += 1;

						unsigned char* scan = (unsigned char*)pPixelData + row * nRowSize + count * nChannels;
						unsigned char* pixel = scan + rchannel;
						ThrowIf(pixel >= (unsigned char*)pPixelData + nRowSize * nHeight);
						*pixel = value;
					}
				}
			}
		}

		ICCProfileTransform::DoTransform(transform, pPixelData, pPixelData, nWidth, nHeight, nRowSize);

		if (nChannels == 4) {
			// Multiply alpha value into each AABBGGRR pixel
			uint32* pImage32 = (uint32*)pPixelData;
			// Blend K channel for CMYK images, alpha channel for RGBA images
			COLORREF backgroundColor = nColorMode == MODE_CMYK ? 0 : CSettingsProvider::This().ColorTransparency();
			for (int i = 0; i < nWidth * nHeight; i++)
				*pImage32++ = Helpers::AlphaBlendBackground(*pImage32, backgroundColor);
		}

		Image = new CJPEGImage(nWidth, nHeight, pPixelData, pEXIFData, nChannels, 0, IF_PSD, false, 0, 1, 0);
	} catch (...) {
		delete Image;
		Image = NULL;
	}
	::CloseHandle(hFile);
	if (Image == NULL) {
		delete[] pPixelData;
	}
	delete[] pBuffer;
	delete[] pEXIFData;
	delete[] pICCProfile;
	ICCProfileTransform::DeleteTransform(transform);
	return Image;
};


CJPEGImage* PsdReader::ReadThumb(LPCTSTR strFileName, bool& bOutOfMemory)
{
	HANDLE hFile;
	hFile = ::CreateFile(strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return NULL;
	}
	char* pBuffer = NULL;
	void* pPixelData = NULL;
	void* pEXIFData = NULL;
	CJPEGImage* Image = NULL;
	int nWidth, nHeight, nChannels;
	int nJpegSize;
	TJSAMP eChromoSubSampling;

	try {
		// Skip file signature
		SeekFile(hFile, 26);

		// Skip color mode data
		unsigned int nColorDataSize = ReadUIntFromFile(hFile);
		SeekFile(hFile, nColorDataSize);

		// Skip resource section size
		ReadUIntFromFile(hFile);

		for (;;) {
			// Resource block signature
			try {
				if (ReadUIntFromFile(hFile) != 0x3842494D) { // "8BIM"
					break;
				}
			} catch (...) {
				break;
			}


			unsigned short nResourceID = ReadUShortFromFile(hFile);

			// Skip Pascal string (padded to be even length)
			while (ReadUShortFromFile(hFile));

			// Resource size
			unsigned int nResourceSize = ReadUIntFromFile(hFile);

			// Parse image resources
			switch (nResourceID) {
				case 0x0409: // 0x0409 1033 (Photoshop 4.0) Thumbnail resource for Photoshop 4.0 only. See See Thumbnail resource format.
				case 0x040C: // 0x040C 1036 (Photoshop 5.0) Thumbnail resource (supersedes resource 1033). See See Thumbnail resource format.
					// Skip thumbnail resource header
					SeekFile(hFile, 28);

					// Read embedded JPEG thumbnail
					nJpegSize = nResourceSize - 28;
					if (nJpegSize > MAX_JPEG_FILE_SIZE) {
						bOutOfMemory = true;
						ThrowIf(true);
					}

					pBuffer = new(std::nothrow) char[nJpegSize];
					if (pBuffer == NULL) {
						bOutOfMemory = true;
						ThrowIf(true);
					}

					ReadFromFile(pBuffer, hFile, nJpegSize);
					SeekFile(hFile, -nResourceSize);


					pPixelData = TurboJpeg::ReadImage(nWidth, nHeight, nChannels, eChromoSubSampling, bOutOfMemory, pBuffer, nJpegSize);
					break;

				case 0x0422: // 0x0422 1058 (Photoshop 7.0) EXIF data 1. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
				case 0x0423: // 0x0423 1059 (Photoshop 7.0) EXIF data 3. See http://www.kodak.com/global/plugins/acrobat/en/service/digCam/exifStandard2.pdf
					if (pEXIFData == NULL && nResourceSize < 65526) {
						pEXIFData = new(std::nothrow) char[nResourceSize + 10];
						if (pEXIFData != NULL) {
							memcpy(pEXIFData, "\xFF\xE1\0\0Exif\0\0", 10);
							*((unsigned short*)pEXIFData + 1) = _byteswap_ushort(nResourceSize + 8);
							ReadFromFile((char*)pEXIFData + 10, hFile, nResourceSize);
							SeekFile(hFile, -nResourceSize);
						}
					}
					break;
			}

			// Skip resource data (padded to be even length)
			SeekFile(hFile, (nResourceSize + 1) & -2);
		}

		if (pPixelData != NULL) {
			Image = new CJPEGImage(nWidth, nHeight, pPixelData, pEXIFData,
				nChannels, Helpers::CalculateJPEGFileHash(pBuffer, nJpegSize), IF_JPEG_Embedded, false, 0, 1, 0);
			Image->SetJPEGComment(Helpers::GetJPEGComment(pBuffer, nJpegSize));
			Image->SetJPEGChromoSampling(eChromoSubSampling);
		}

	} catch(...) {
		delete Image;
		Image = NULL;
	}
	::CloseHandle(hFile);
	if (Image == NULL) {
		delete[] pPixelData;
	}
	delete[] pEXIFData;
	delete[] pBuffer;
	return Image;
}
