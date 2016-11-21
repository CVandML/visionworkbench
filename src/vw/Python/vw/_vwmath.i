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


%module vwmath

%{
#define SWIG_FILE_WITH_INIT
#include <vw/Math.h>
%}

%include "std_string.i"
%include "carrays.i"
%include "numpy.i"

%init %{
  import_array();
%}

%array_class(double, doublea);

namespace vw {

  class BBox2i {
  public:
    BBox2i(int,int,int,int);
    BBox2i(BBox2i);

    %extend {
      int _get_minx() const { return self->min().x(); }
      int _get_miny() const { return self->min().y(); }
      int _get_maxx() const { return self->max().x(); }
      int _get_maxy() const { return self->max().y(); }
      void _set_minx(int v) { self->min().x() = v; }
      void _set_miny(int v) { self->min().y() = v; }
      void _set_maxx(int v) { self->max().x() = v; }
      void _set_maxy(int v) { self->max().y() = v; }
      int _get_width() const { return self->width(); }
      int _get_height() const { return self->height(); }
      std::string __str__() const {
        std::ostringstream os;
        os << "BBox2i(" << self->min().x() << "," << self->min().y() << ","
           << self->width() << "," << self->height() << ")";
        return os.str();
      }
    }

    %pythoncode {
      minx = property(_get_minx,_set_minx)
      miny = property(_get_miny,_set_miny)
      maxx = property(_get_maxx,_set_maxx)
      maxy = property(_get_maxy,_set_maxy)
      width = property(_get_width)
      height = property(_get_height)
    }
  };

} // namespace vw
