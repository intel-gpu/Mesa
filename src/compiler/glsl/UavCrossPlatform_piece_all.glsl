/*
 * Copyright 2020-2022 Matias N. Goldberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#define OGRE_imageLoad2D( inImage, iuv ) imageLoad( inImage, int2( iuv ) )
#define OGRE_imageLoad2DArray( inImage, iuvw ) imageLoad( inImage, int3( iuvw ) )

#define OGRE_imageWrite2D1( outImage, iuv, value ) imageStore( outImage, int2( iuv ), float4( value, 0, 0, 0 ) )
#define OGRE_imageWrite2D2( outImage, iuv, value ) imageStore( outImage, int2( iuv ), float4( value, 0, 0 ) )
#define OGRE_imageWrite2D4( outImage, iuv, value ) imageStore( outImage, int2( iuv ), value )

#define OGRE_imageLoad3D( inImage, iuv ) imageLoad( inImage, int3( iuv ) )

#define OGRE_imageWrite3D1( outImage, iuv, value ) imageStore( outImage, int3( iuv ), value )
#define OGRE_imageWrite3D4( outImage, iuv, value ) imageStore( outImage, int3( iuv ), value )

#define OGRE_imageWrite2DArray1( outImage, iuvw, value ) imageStore( outImage, int3( iuvw ), value )
#define OGRE_imageWrite2DArray4( outImage, iuvw, value ) imageStore( outImage, int3( iuvw ), value )

#define __sharedOnlyBarrier memoryBarrierShared();barrier();
