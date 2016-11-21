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


#ifdef _MSC_VER
#pragma warning(disable:4244)
#pragma warning(disable:4267)
#pragma warning(disable:4996)
#endif

#include <vw/Core/Log.h>
#include <vw/Math/BBox.h>
#include <vw/Math/Vector.h>
#include <vw/Image/AlgorithmFunctions.h>
#include <vw/Image/Manipulation.h>
#include <vw/Image/ImageIO.h>
#include <vw/Image/ImageViewRef.h>
#include <vw/FileIO/DiskImageView.h>
#include <vw/Mosaic/QuadTreeGenerator.h>
#include <vw/Mosaic/ImageComposite.h>

#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#include <boost/operators.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path_traits.hpp>
#include <boost/filesystem/fstream.hpp>
namespace fs = boost::filesystem;


using namespace vw;

std::string mosaic_name;
std::string file_type;
int tile_size;
bool draft;
bool qtree;

template <class PixelT>
void do_blend() {
  mosaic::ImageComposite<PixelT> composite;
  if( draft ) composite.set_draft_mode( true );

  std::map<std::string,fs::path> image_files;
  std::map<std::string,fs::path> offset_files;
  fs::path source_dir_path( mosaic_name );
  fs::directory_iterator pi( source_dir_path ), pend;
  for( ; pi != pend; ++pi ) {
    fs::path path = pi->path();
    if ( path.extension() == ".offset" )
      offset_files[path.stem().string()] = path;
    else image_files[path.stem().string()] = path;
  }
  std::map<std::string,fs::path>::iterator ofi=offset_files.begin(), ofend=offset_files.end();
  for( ; ofi != ofend; ++ofi ) {
    std::map<std::string,fs::path>::iterator ifi = image_files.find( ofi->first );
    if( ifi != image_files.end() ) {
      fs::ifstream offset( ofi->second );
      int x, y;
      offset >> x >> y;
      std::cout << "Importing image file " << ifi->second.string()
                << " at offet (" << x << "," << y << ")" << std::endl;
      composite.insert( DiskImageView<PixelT>( ifi->second.string() ), x, y );
    }
  }

  vw_out(InfoMessage) << "Preparing the composite..." << std::endl;
  composite.prepare();
  if( qtree ) {
    vw_out(InfoMessage) << "Preparing the quadtree..." << std::endl;
    mosaic::QuadTreeGenerator quadtree( composite, mosaic_name );
    quadtree.set_file_type( file_type );
    quadtree.set_tile_size( tile_size );
    vw_out(InfoMessage) << "Generating..." << std::endl;
    quadtree.generate();
    vw_out(InfoMessage) << "Done!" << std::endl;
  }
  else {
    vw_out(InfoMessage) << "Blending..." << std::endl;
    write_image( mosaic_name+".blend."+file_type, composite );
    vw_out(InfoMessage) << "Done!" << std::endl;
  }
}

int main( int argc, char *argv[] ) {
  try {
    po::options_description desc("Options");
    desc.add_options()
      ("input-dir", po::value<std::string>(&mosaic_name),
       "Explicitly specify the input directory")
      ("file-type", po::value<std::string>(&file_type)->default_value("png"),
       "Output file type")
      ("tile-size", po::value<int>(&tile_size)->default_value(256),
       "Tile size, in pixels")
      ("draft", "Draft mode (no blending)")
      ("qtree", "Output in quadtree format")
      ("grayscale", "Process in grayscale only")
      ("help,h", "Display this help message");
    po::positional_options_description p;
    p.add("input-dir", 1);

    po::variables_map vm;
    try {
      po::store( po::command_line_parser( argc, argv ).options(desc).positional(p).run(), vm );
      po::notify( vm );
    } catch (const po::error& e) {
      std::cout << "An error occured while parsing command line arguments.\n";
      std::cout << "\t" << e.what() << "\n\n";
      std::cout << desc << std::endl;
      return 1;
    }

    if( vm.count("help") ) {
      std::cout << desc << std::endl;
      return 1;
    }

    if( vm.count("input-dir") != 1 ) {
      std::cout << "Error: Must specify one (and only one) input directory!" << std::endl;
      std::cout << desc << std::endl;
      return 1;
    }

    if( vm.count("draft") ) draft = true; else draft = false;
    if( vm.count("qtree") ) qtree = true; else qtree = false;

    if( tile_size <= 0 ) {
      std::cerr << "Error: The tile size must be a positive number!  (You specified "
                << tile_size << ".)" << std::endl;
      std::cout << desc << std::endl;
      return 1;
    }

    if( vm.count("grayscale") ) {
      do_blend<PixelGrayA<float> >();
    }
    else {
      do_blend<PixelRGBA<float> >();
    }

  }
  catch (const std::exception& err) {
    vw_out(ErrorMessage) << "Error: " << err.what() << std::endl << "Aborting!" << std::endl;
    return 1;
  }
  return 0;
}
