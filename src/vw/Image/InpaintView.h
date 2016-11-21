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


/// \file InpaintView.h
///

#ifndef __VW_IMAGE_INPAINTVIEW_H__
#define __VW_IMAGE_INPAINTVIEW_H__

// Standard
#include <vector>

// VW
#include <vw/Core/Thread.h>
#include <vw/Core/ThreadPool.h>
#include <vw/Core/Stopwatch.h>
#include <vw/Image/Algorithms.h>
#include <vw/Image/ImageViewBase.h>
#include <vw/Image/MaskViews.h>
#include <vw/Image/BlobIndex.h>
#include <vw/Image/SparseView.h>

#include <boost/foreach.hpp>

namespace vw {
  namespace inpaint_p {

    // Semi-private tasks that I wouldn't like the user to know about
    //
    // This is used for threaded rendering
    template <class ViewT, class SViewT>
    class InpaintTask : public Task, boost::noncopyable {
      ViewT const& m_view;
      blob::BlobCompressed m_c_blob;
      bool m_use_grassfire;
      typename ViewT::pixel_type m_default_inpaint_val;
      SparseCompositeView<SViewT> & m_patches; // Store our output

    public:
      InpaintTask( ImageViewBase<ViewT> const& view,
                   blob::BlobCompressed const& c_blob,
                   bool use_grassfire,
                   typename ViewT::pixel_type default_inpaint_val,
                   SparseCompositeView<SViewT> & sparse ) :
        m_view(view.impl()), m_c_blob(c_blob),
        m_use_grassfire(use_grassfire), m_default_inpaint_val(default_inpaint_val),
        m_patches(sparse) {}

      void operator()() {
        using namespace vw;

        typedef typename ViewT::pixel_type pixel_type;

        // Gathering information about blob
        BBox2i bbox = m_c_blob.bounding_box();
        bbox.expand(1);

        // How do we want to handle spots on the edges?
        if ( bbox.min().x() < 0 || bbox.min().y() < 0 ||
             bbox.max().x() >= m_view.impl().cols() ||
             bbox.max().y() >= m_view.impl().rows() ) {
          return;
        }

        std::list<Vector2i> blob;
        m_c_blob.decompress( blob );
        for ( std::list<Vector2i>::iterator iter = blob.begin();
              iter != blob.end(); iter++ )
          *iter -= bbox.min();

        // Building a cropped copy for my patch
        ImageView<pixel_type> cropped_copy =
          crop( m_view, bbox );

        // Creating binary image to highlight hole
        ImageView<uint8> mask( bbox.width(), bbox.height() );
        fill( mask, 0 );
        for ( std::list<Vector2i>::const_iterator iter = blob.begin();
              iter != blob.end(); iter++ )
          mask( iter->x(), iter->y() ) = 255;

        if (m_use_grassfire){
          ImageView<int32> distance = grassfire(mask);
          int max_distance = max_pixel_value( distance );

          // Working out order of convolution
          std::list<Vector2i> processing_order;
          for ( int d = 1; d < max_distance+1; d++ )
            for ( int i = 0; i < bbox.width(); i++ )
              for ( int j = 0; j < bbox.height(); j++ )
                if ( distance(i,j) == d ) {
                  processing_order.push_back( Vector2i(i,j) );
                }

          // Iterate and apply convolution seperately to each channel
          typedef typename CompoundChannelCast<pixel_type,float>::type AccumulatorType;
          for ( int d = 0; d < 10*max_distance*max_distance; d++ )
            BOOST_FOREACH( Vector2i const& l, processing_order ) {
              typename ImageView<pixel_type>::pixel_accessor pit =
                cropped_copy.origin();
              pit.advance( l.x() - 1, l.y() - 1 );

              AccumulatorType sum;
              sum += .176765 * (*pit);
              pit.next_col();
              sum += .073235 * (*pit);
              pit.next_col();
              sum += .176765 * (*pit);
              pit.advance( -2, 1 );
              sum += .073235 * (*pit);
              pit.advance( 2, 0 );
              sum += .073235 * (*pit);
              pit.advance( -2, 1 );
              sum += .176765 * (*pit);
              pit.next_col();
              sum += .073235 * (*pit);
              pit.next_col();
              sum += .176765 * (*pit);

              sum.validate();
              cropped_copy(l.x(),l.y()) = sum;
            }
        }else{
          for ( std::list<Vector2i>::const_iterator iter = blob.begin();
                iter != blob.end(); iter++ )
            cropped_copy( iter->x(), iter->y() ) = m_default_inpaint_val;
        }

        // Insert results into sparse view
        m_patches.absorb(bbox.min(),copy_mask(cropped_copy,create_mask( mask, 0 )));
      }

    };

  } // end namespace inpaint_p

  /// InpaintView (feed all blobs before hand)
  //////////////////////////////////////////////
  template <class ViewT>
  class InpaintView : public ImageViewBase<InpaintView<ViewT> > {

    ViewT m_child;
    BlobIndexThreaded const& m_bindex;
    bool m_use_grassfire;
    typename ViewT::pixel_type m_default_inpaint_val;

  public:
    typedef typename UnmaskedPixelType<typename ViewT::pixel_type>::type sparse_type;
    typedef typename ViewT::pixel_type pixel_type;
    typedef pixel_type result_type; // We can't return references
    typedef ProceduralPixelAccessor<InpaintView<ViewT> > pixel_accessor;

    InpaintView( ImageViewBase<ViewT> const& image,
                 BlobIndexThreaded const& bindex,
                 bool use_grassfire,
                 pixel_type default_inpaint_val):
      m_child(image.impl()), m_bindex(bindex),
      m_use_grassfire(use_grassfire), m_default_inpaint_val(default_inpaint_val) {}

    inline int32 cols  () const { return m_child.cols(); }
    inline int32 rows  () const { return m_child.rows(); }
    inline int32 planes() const { return 1; } // Not allowed.

    inline pixel_accessor origin() const { return pixel_accessor(*this,0,0); }

    inline result_type operator()( int32 i, int32 j, int32 /*p*/=0 ) const {
      vw_throw( NoImplErr() << "Per pixel access is not provided for InpaintView" );
    }

    typedef CropView<ImageView<pixel_type> >    inner_pre_type;
    typedef SparseCompositeView<inner_pre_type> prerasterize_type;
    inline prerasterize_type prerasterize( BBox2i const& bbox ) const {
      using namespace vw;

      // Expand the preraster size to include all the area that our patches use
      // - This makes sure all contained blobs are identified and fully contained
      std::vector<size_t> intersections;
      intersections.reserve(20);
      BBox2i bbox_expanded = bbox;
      for ( size_t i = 0; i < m_bindex.num_blobs(); i++ ) {
        if ( m_bindex.blob_bbox(i).intersects( bbox ) && // Early exit option
             m_bindex.compressed_blob(i).intersects( bbox ) ) {
          bbox_expanded.grow( m_bindex.blob_bbox(i) );
          intersections.push_back(i);
        }
      }
      // Expand by one more pixel, then make sure the BB has not exceeded the image bounds.
      bbox_expanded.expand(1);
      bbox_expanded.crop( BBox2i(0,0,cols(),rows()) );

      // Generate sparse view that will hold background data and all the patches.
      inner_pre_type preraster =
        crop(ImageView<pixel_type>(crop(m_child,bbox_expanded)),
             -bbox_expanded.min().x(), -bbox_expanded.min().y(), cols(), rows());
      SparseCompositeView<inner_pre_type> patched_view( preraster );

      // Build up the patches that intersect our tile
      // - For each intersecting blob, use InpaintTask to fill in that blob
      typedef inpaint_p::InpaintTask<inner_pre_type, inner_pre_type> task_type;
      for ( std::vector<size_t>::const_iterator it = intersections.begin();
            it != intersections.end(); it++ ) {
        task_type task( preraster, m_bindex.compressed_blob(*it), m_use_grassfire,
                        m_default_inpaint_val, patched_view );
        task();
      }

      return patched_view;
    }
    template <class DestT>
    inline void rasterize( DestT const& dest, BBox2i const& bbox ) const {
      vw::rasterize( prerasterize(bbox), dest, bbox );
    }
  };

  template <class SourceT>
  inline InpaintView<SourceT> inpaint( ImageViewBase<SourceT> const& src,
                                       BlobIndexThreaded const& bindex,
                                       bool use_grassfire,
                                       typename SourceT::pixel_type default_inpaint_val) {
    return InpaintView<SourceT>(src, bindex, use_grassfire, default_inpaint_val);
  }

  // Fill holes using grassfire. The input image is expected to be a PixelMask,
  // with the pixels in the holes being invalid.
  template <class ImageT>
  class FillHolesGrass: public ImageViewBase< FillHolesGrass<ImageT> >{
    ImageT m_img;
    int    m_hole_fill_len;
  
  public:
    FillHolesGrass(ImageT const& img, int hole_fill_len):
      m_img(img), m_hole_fill_len(hole_fill_len){}
    
    typedef typename ImageT::pixel_type pixel_type;
    typedef pixel_type                  result_type;
    typedef ProceduralPixelAccessor< FillHolesGrass<ImageT> > pixel_accessor;
    
    inline int cols  () const { return m_img.cols(); }
    inline int rows  () const { return m_img.rows(); }
    inline int planes() const { return 1; }
  
    inline pixel_accessor origin() const { return pixel_accessor( *this, 0, 0 ); }

    inline pixel_type operator()( double/*i*/, double/*j*/, int/*p*/ = 0 ) const {
      vw_throw(NoImplErr() << "FillHolesGrass::operator() is not implemented");
      return pixel_type();
    }
  
    typedef CropView<ImageView<pixel_type> > prerasterize_type;
    inline prerasterize_type prerasterize(BBox2i const& bbox) const {

      // Must see m_hole_fill_len beyond current tile if we are to fill
      // holes of this size in the tile.
      BBox2i biased_box = bbox;
      biased_box.expand(m_hole_fill_len);
      biased_box.crop(bounding_box(m_img));

      // Pull the relevant chunk into memory
      ImageView<pixel_type> tile = crop(m_img, biased_box);
      
      int area = m_hole_fill_len * m_hole_fill_len;
      
      // Pick a large number here, to avoid the tile being broken up into
      // sub-tiles for processing.
      int tile_size = std::max(tile.cols(), tile.rows()); 
  
      // Use just one thread for this operation, since there are many such
      // operations running simultaneously already.
      int num_threads = 1;
      
      // Find the holes, wipe holes bigger than the spec
      BlobIndexThreaded blob_index( invert_mask(tile),
                                    area, tile_size, num_threads);
      blob_index.wipe_big_blobs(m_hole_fill_len);

      // Fill the holes
      bool use_grassfire = true;
      pixel_type default_inpaint_val;
      return prerasterize_type(inpaint(tile, blob_index, use_grassfire,
                                       default_inpaint_val),
                               -biased_box.min().x(), -biased_box.min().y(),
                               cols(), rows() );
    }
    
    template <class DestT>
    inline void rasterize(DestT const& dest, BBox2i bbox) const {
      vw::rasterize(prerasterize(bbox), dest, bbox);
    }
  };

  template <class ImageT>
  inline FillHolesGrass<ImageT>
  fill_holes_grass(ImageT const& img, int hole_fill_len) {
    return FillHolesGrass<ImageT>(img, hole_fill_len);
  }

// If a pixel has invalid data, fill its value with the average of
// valid pixel values within a given window around the pixel.  This is
// a simple way of inpainting. This is useful if a Gaussian blur is
// applied afterward with the same kernel size.
template <class ImageT>
class FillNoDataWithAvg:
  public ImageViewBase< FillNoDataWithAvg<ImageT> > {
  ImageT m_img;
  int m_kernel_size;
  typedef typename ImageT::pixel_type PixelT;

public:

  typedef PixelT pixel_type;
  typedef PixelT result_type;
  typedef ProceduralPixelAccessor<FillNoDataWithAvg> pixel_accessor;

  FillNoDataWithAvg( ImageViewBase<ImageT> const& img,
		     int kernel_size) :
    m_img(img.impl()), m_kernel_size(kernel_size) {
    VW_ASSERT((m_kernel_size%2 == 1) && (m_kernel_size > 0),
	      ArgumentErr() << "Expecting odd and positive kernel size.");
  }

  inline int32 cols  () const { return m_img.cols(); }
  inline int32 rows  () const { return m_img.rows(); }
  inline int32 planes() const { return 1; }

  inline pixel_accessor origin() const { return pixel_accessor(*this); }

  inline result_type operator()( size_t i, size_t j, size_t p=0 ) const {
    if (is_valid(m_img(i, j)))
      return m_img(i, j);

    pixel_type val; val.validate();
    int nvalid = 0;
    int c0 = i, r0 = j; // convert to int from size_t
    int k2 = m_kernel_size/2, nc = m_img.cols(), nr = m_img.rows();
    for (int c = std::max(0, c0-k2); c <= std::min(nc-1, c0+k2); c++){
      for (int r = std::max(0, r0-k2); r <= std::min(nr-1, r0+k2); r++){
	if (!is_valid(m_img(c, r)))
	  continue;
	val += m_img(c, r);
	nvalid++;
      }
    }

    if (nvalid == 0) return m_img(i, j); // could not find valid points
    return val/nvalid; // average of valid values within window
  }

  typedef FillNoDataWithAvg<CropView<ImageView<PixelT> > > prerasterize_type;
  inline prerasterize_type prerasterize( BBox2i const& bbox ) const {

    // Crop into an expanded box as to have enough pixels to do
    // averaging with given window at every pixel in the current box.
    BBox2i biased_box = bbox;
    biased_box.expand(m_kernel_size/2);
    biased_box.crop(bounding_box(m_img));
    ImageView<PixelT> dest( crop( m_img, biased_box ) );

    return prerasterize_type( crop( dest, -biased_box.min().x(), -biased_box.min().y(), cols(), rows() ), m_kernel_size );

}
  template <class DestT> inline void rasterize( DestT const& dest, BBox2i const& bbox ) const { vw::rasterize( prerasterize(bbox), dest, bbox ); }

};
template <class ImgT>
FillNoDataWithAvg<ImgT>
fill_nodata_with_avg( ImageViewBase<ImgT> const& img,
		      int kernel_size) {
  typedef FillNoDataWithAvg<ImgT> result_type;
  return result_type( img.impl(), kernel_size);
}
  
  
} //end namespace vw

#endif//__VW_IMAGE_INPAINTVIEW_H__
