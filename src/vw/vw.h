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


/// \file vw.h
///
/// A convenience header that includes all the public Vision Workbench
/// header files.  Careful: this is an awful lot of stuff, and you may
/// want be more selective!
///
#ifndef __VW_VW_H__
#define __VW_VW_H__

#include <vw/config.h>

#if defined(VW_HAVE_PKG_CORE) && VW_HAVE_PKG_CORE==1
#include <vw/Core.h>
#endif

#if defined(VW_HAVE_PKG_MATH) && VW_HAVE_PKG_MATH==1
#include <vw/Math.h>
#endif

#if defined(VW_HAVE_PKG_IMAGE) && VW_HAVE_PKG_IMAGE==1
#include <vw/Image.h>
#endif

#if defined(VW_HAVE_PKG_FILEIO) && VW_HAVE_PKG_FILEIO==1
#include <vw/FileIO.h>
#endif

#if defined(VW_HAVE_PKG_CAMERA) && VW_HAVE_PKG_CAMERA==1
#include <vw/Camera.h>
#endif

#if defined(VW_HAVE_PKG_MOSAIC) && VW_HAVE_PKG_MOSAIC==1
#include <vw/Mosaic.h>
#endif

#if defined(VW_HAVE_PKG_CARTOGRAPHY) && VW_HAVE_PKG_CARTOGRAPHY==1
#include <vw/Cartography.h>
#endif

#if defined(VW_HAVE_PKG_HDR) && VW_HAVE_PKG_HDR==1
#include <vw/HDR.h>
#endif

#if defined(VW_HAVE_PKG_GEOMETRY) && VW_HAVE_PKG_GEOMETRY==1
#include <vw/Geometry.h>
#endif

#if defined(VW_HAVE_PKG_GPU) && VW_HAVE_PKG_GPU==1
#include <vw/GPU.h>
#endif

#if defined(VW_HAVE_PKG_INTERESTPOINT) && VW_HAVE_PKG_INTERESTPOINT==1
#include <vw/InterestPoint.h>
#endif

#if defined(VW_HAVE_PKG_STEREO) && VW_HAVE_PKG_STEREO==1
#include <vw/Stereo.h>
#endif

#endif // __VW_VW_H__
