// __BEGIN_LICENSE__
//  Copyright (c) 2009-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NGT platform is licensed under the Apache License, Version 2.0 (the
//  "License"); you may not use this file except in compliance with the
//  License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


#include <test/Helpers.h>

#include <vw/Image/ImageViewRef.h>
#include <vw/Image/InpaintView.h>

using namespace vw;

// For now this test is only to make sure that the class compiles!

TEST(InpaintView, compile) {
  /* Set up this simple image:
     .  .  .  .  .
     .  .  X  .  .
     .  .  .  .  .
     .  X  .  X  X
     .  .  .  .  .
   */
  ImageView<PixelMask<uint8> > blobs(5,5);
  blobs(1,2) = PixelMask<uint8>(100);
  blobs(3,1) = PixelMask<uint8>(100);
  blobs(3,3) = PixelMask<uint8>(100);
  blobs(3,4) = PixelMask<uint8>(100);

  // Process blobs
  BlobIndexThreaded bindex( blobs, 100, 100 );
  EXPECT_EQ( 3u, bindex.num_blobs() );

  ImageView<PixelMask<uint8> > dummy(10, 10);
  InpaintView< ImageView<PixelMask<uint8> > > ipView( dummy, bindex, true, PixelMask<uint8>());
  
}


