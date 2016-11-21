// __BEGIN_LICENSE__
//  Copyright (c) 2006-2013, United States Government as represented by the
//  Administrator of the National Aeronautics and Space Administration. All
//  rights reserved.
//
//  The NASA Vision Workbench is licensed under the Apache License,
//  Version 2.0 (the "License"); you may not use this file except in
//  compliance with the License. You may obtain a copy of the License at
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// __END_LICENSE__


#include <vw/Mosaic/TMSQuadTreeConfig.h>

#include <boost/filesystem/path.hpp>
namespace fs = boost::filesystem;

namespace vw {
namespace mosaic {

  std::string TMSQuadTreeConfig::image_path( QuadTreeGenerator const& qtree, std::string const& name ) {
    fs::path path( qtree.get_name() );

    Vector2i pos(0,0);
    for ( int i=0; i<(int)name.length(); ++i ) {
      pos *= 2;
      if( name[i]=='0' ) pos += Vector2i(0,1);
      else if( name[i]=='1' ) pos += Vector2i(1,1);
      else if( name[i]=='3' ) pos += Vector2i(1,0);
      else if( name[i]!='2' ) {
        vw_throw(LogicErr() << "TMS output format incompatible with non-standard quadtree structure");
      }
    }
    std::ostringstream oss;
    oss << name.length() << "/" << pos.x() << "/" << pos.y();
    path /= oss.str();

    return path.string();
  }

  void TMSQuadTreeConfig::configure( QuadTreeGenerator& qtree ) const {
    qtree.set_image_path_func( &image_path );
    qtree.set_cull_images( true );
  }

  cartography::GeoReference TMSQuadTreeConfig::output_georef(uint32 xresolution, uint32 yresolution) {
    if (yresolution == 0)
      yresolution = xresolution;

    VW_ASSERT(xresolution == yresolution, LogicErr() << "TMS requires square pixels");

    cartography::GeoReference r;
    r.set_pixel_interpretation(cartography::GeoReference::PixelAsArea);

    // Note: the global TMS pixel space extends from +270 to -90
    // latitude, so that the lower-left hand corner is tile-
    // aligned, since TMS uses an origin in the lower left.
    Matrix3x3 transform;
    transform(0,0) = 360.0 / xresolution;
    transform(0,2) = -180;
    transform(1,1) = -360.0 / yresolution;
    transform(1,2) = 270;
    transform(2,2) = 1;
    r.set_transform(transform);

    return r;
  }

} // namespace mosaic
} // namespace vw
