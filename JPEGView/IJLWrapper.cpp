//** JPEG.CPP, the code used to wrap IJL

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//############################################################################
//##                                                                        ##
//##  JPEG.CPP                                                              ##
//##                                                                        ##
//##  Wrapper class to load a jpeg from a block of memory.                  ##
//##                                                                        ##
//##  OpenSourced 2/4/2000 by John W. Ratcliff                              ##
//##                                                                        ##
//##  No warranty expressed or implied.  Released as part of the triangle   ##
//##  throughput testbed project.                                           ##
//############################################################################
//##                                                                        ##
//##  Contact John W. Ratcliff at jratcliff@verant.com                      ##
//############################################################################

#include "IJLWrapper.h"
#include "ijl.h" // intel jpeg library header file.

// read image into this buffer.
void * Jpeg::ReadImage(int &width,
                       int &height,
                       int &nchannels,
                       const void *buffer,
                       int sizebytes)
{
  const int MAX_PIXELS = 1024*1024*100; // 100 MPixel

  JPEG_CORE_PROPERTIES jcprops;
  if ( ijlInit(&jcprops) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }  
  jcprops.JPGBytes = (unsigned char *) buffer;
  jcprops.JPGSizeBytes = sizebytes;
  jcprops.jquality = 100;
  if ( ijlRead(&jcprops,IJL_JBUFF_READPARAMS) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }  
  width  = jcprops.JPGWidth;
  height = jcprops.JPGHeight;
  if (abs(width*height) > MAX_PIXELS) {
    ijlFree(&jcprops);
    return 0;
  }
  IJLIOTYPE mode;  
  mode = IJL_JBUFF_READWHOLEIMAGE;
  nchannels = jcprops.JPGChannels;
  int nPad = (4 - (width * nchannels) & 3) & 3;
  unsigned char * pixbuff = new unsigned char[(width + nPad)*height*nchannels];
  if ( !pixbuff )
  {
    ijlFree(&jcprops);
    return 0;
  }  
  jcprops.DIBWidth  = width;
  jcprops.DIBHeight = height;
  jcprops.DIBChannels = nchannels;
  jcprops.DIBPadBytes = nPad;
  jcprops.DIBBytes = (unsigned char *)pixbuff; 
  if ( jcprops.JPGChannels == 3 )
  {
    jcprops.DIBColor = IJL_BGR;
    jcprops.JPGColor = IJL_YCBCR;
    jcprops.JPGSubsampling = IJL_411;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }
  else
  {
	  /*
    jcprops.DIBColor = IJL_G;
    jcprops.JPGColor = IJL_G;
    jcprops.JPGSubsampling = (IJL_JPGSUBSAMPLING) 0;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
	*/
	// IJL seems to crash sometimes when reading 1 channel JPEGs, better user GDI+ in this case
	ijlFree(&jcprops);
    return 0;
  } 
  if ( ijlRead(&jcprops, mode) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  }  
  if ( ijlFree(&jcprops) != IJL_OK ) return 0; 
  return (void *)pixbuff;
}

void * Jpeg::Compress(const void *source,
                      int width,
                      int height,
                      int nchannels,
                      int &len,
                      int quality)
{
  JPEG_CORE_PROPERTIES jcprops;  
  if ( ijlInit(&jcprops) != IJL_OK )
  {
    ijlFree(&jcprops);
    return 0;
  } 
  int nPad = (4 - (width * nchannels) & 3) & 3;
  jcprops.DIBWidth    = width;
  jcprops.DIBHeight   = height;
  jcprops.JPGWidth    = width;
  jcprops.JPGHeight   = height;
  jcprops.DIBBytes    = (unsigned char *) source;
  jcprops.DIBPadBytes = nPad;
  jcprops.DIBChannels = nchannels;
  jcprops.JPGChannels = nchannels;  
  if ( nchannels == 3 )
  {
    jcprops.DIBColor = IJL_BGR;
    jcprops.JPGColor = IJL_YCBCR;
    jcprops.JPGSubsampling = IJL_411;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }
  else
  {
    jcprops.DIBColor = IJL_G;
    jcprops.JPGColor = IJL_G;
    jcprops.JPGSubsampling = (IJL_JPGSUBSAMPLING) 0;
    jcprops.DIBSubsampling = (IJL_DIBSUBSAMPLING) 0;
  }  
  int size = (width*nchannels+nPad)*height;  
  unsigned char * buffer = new unsigned char[size];  
  jcprops.JPGSizeBytes = size;
  jcprops.JPGBytes     = buffer;  
  jcprops.jquality = quality;
  if ( ijlWrite(&jcprops,IJL_JBUFF_WRITEWHOLEIMAGE) != IJL_OK )
  {
    ijlFree(&jcprops);
    delete buffer;
    return 0;
  }
  if ( ijlFree(&jcprops) != IJL_OK )
  {
    delete buffer;
    return 0;
  }  
  len = jcprops.JPGSizeBytes;
  return buffer;
}
