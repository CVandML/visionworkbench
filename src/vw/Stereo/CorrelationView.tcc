

namespace vw {
namespace stereo {



template <class Image1T, class Image2T, class PreFilterT>
typename CorrelationView<Image1T, Image2T, PreFilterT>::prerasterize_type
CorrelationView<Image1T, Image2T, PreFilterT>::
prerasterize(BBox2i const& bbox) const {

#if VW_DEBUG_LEVEL > 0
    Stopwatch watch;
    watch.start();
#endif

    // 1.) Expand the left raster region by the kernel size.
    Vector2i half_kernel = m_kernel_size/2;
    BBox2i  left_region  = bbox;
    left_region.min() -= half_kernel;
    left_region.max() += half_kernel;

    // 2.) Calculate the region of the right image that we're using.
    BBox2i right_region = left_region + m_search_region.min();
    right_region.max() += m_search_region.size();

    // 3.) Calculate the disparity
    ImageView<pixel_type> result
      = calc_disparity(m_cost_type,
                       crop(m_prefilter.filter(m_left_image),left_region),
                       crop(m_prefilter.filter(m_right_image),right_region),
                       left_region - left_region.min(),
                       m_search_region.size() + Vector2i(1,1),
                       m_kernel_size);

    // 4.0 ) Consistency check
    if ( m_consistency_threshold >= 0 ) {
      // Getting the crops correctly here is not important as we
      // will re-crop later. The important bit is aligning up the origins.
      ImageView<pixel_type> rl_result
        = calc_disparity(m_cost_type,
                         crop(m_prefilter.filter(m_right_image),right_region),
                         crop(m_prefilter.filter(m_left_image),
                              left_region - (m_search_region.size()+Vector2i(1,1))),
                         right_region - right_region.min(),
                         m_search_region.size() + Vector2i(1,1),
                         m_kernel_size) -
        pixel_type(m_search_region.size()+Vector2i(1,1));

      stereo::cross_corr_consistency_check( result, rl_result,
                                            m_consistency_threshold, false );
    }
    VW_ASSERT( bbox.size() == bounding_box(result).size(),
               MathErr() << "CorrelationView::prerasterize got a bad return from best_of_search_convolution." );

    // 5.) Convert back to original coordinates
    result += pixel_type(m_search_region.min());

#if VW_DEBUG_LEVEL > 0
    watch.stop();
    vw_out(DebugMessage,"stereo") << "Tile " << bbox << " processed in " << watch.elapsed_seconds() << " s\n";
#endif

    return prerasterize_type( result, -bbox.min().x(), -bbox.min().y(), cols(), rows() );
  } // End function prerasterize





//=========================================================================


template <class Image1T, class Image2T, class Mask1T, class Mask2T>
bool PyramidCorrelationView<Image1T, Image2T, Mask1T, Mask2T>::
build_image_pyramids(BBox2i const& bbox, int32 const max_pyramid_levels,
                     std::vector<ImageView<typename Image1T::pixel_type> > & left_pyramid,
                     std::vector<ImageView<typename Image2T::pixel_type> > & right_pyramid,
                     std::vector<ImageView<typename Mask1T::pixel_type > > & left_mask_pyramid,
                     std::vector<ImageView<typename Mask2T::pixel_type > > & right_mask_pyramid) const {

  Vector2i half_kernel = m_kernel_size/2;

  // Init the pyramids: Highest resolution image is stored at index zero.
  left_pyramid.resize      (max_pyramid_levels + 1);
  right_pyramid.resize     (max_pyramid_levels + 1);
  left_mask_pyramid.resize (max_pyramid_levels + 1);
  right_mask_pyramid.resize(max_pyramid_levels + 1);
  
  
  int32 max_upscaling = 1 << max_pyramid_levels;
  BBox2i left_global_region, right_global_region;
  // Region in the left image is the input bbox expanded by the kernel,
  //  to make sure that we have full base of support for the stereo correlation.
  // - Make sure that the kernel buffer size is large enough to support the kernel
  //   buffer at the lower resolution levels!
  Vector2i region_offset = half_kernel * max_upscaling;
  vw_out(VerboseDebugMessage, "stereo") << "pyramid region offset = " << region_offset << std::endl;
  left_global_region = bbox;
  left_global_region.expand(region_offset);
  // Region in the right image is the left region plus search range offsets
  // - What is the last upscaling term for?
  right_global_region = left_global_region + m_search_region.min();
  right_global_region.max() += m_search_region.size() + Vector2i(max_upscaling,max_upscaling); 
  // Total increase in right size = 2*region_offset + search_region_size + max_upscaling
  
  vw_out(VerboseDebugMessage, "stereo") << "Left pyramid base bbox:  " << left_global_region  << std::endl;
  vw_out(VerboseDebugMessage, "stereo") << "Right pyramid base bbox: " << right_global_region << std::endl;
  
  // Extract the lowest resolution layer
  // - Constant extension is used here to help the correlator make matches near the image edge.
  left_pyramid      [0] = crop(edge_extend(m_left_image,  ConstantEdgeExtension()), left_global_region );
  right_pyramid     [0] = crop(edge_extend(m_right_image, ConstantEdgeExtension()), right_global_region);
  left_mask_pyramid [0] = crop(edge_extend(m_left_mask,   ConstantEdgeExtension()), left_global_region );
  right_mask_pyramid[0] = crop(edge_extend(m_right_mask,  ConstantEdgeExtension()), right_global_region);

#if VW_DEBUG_LEVEL > 0
  VW_OUT(DebugMessage,"stereo") << " > Left ROI: "    << left_global_region
                                << "\n > Right ROI: " << right_global_region << "\n";
#endif

  // Fill in the nodata of the left and right images with a mean
  // pixel value. This helps with the edge quality of a DEM.
  // - Note that this will not fill in the edge-extended values.
  typename Image1T::pixel_type left_mean;
  typename Image2T::pixel_type right_mean;
  try {
    left_mean  = mean_pixel_value(subsample(copy_mask(left_pyramid [0], create_mask(left_mask_pyramid [0],0)),2));
    right_mean = mean_pixel_value(subsample(copy_mask(right_pyramid[0], create_mask(right_mask_pyramid[0],0)),2));
  } catch ( const ArgumentErr& err ) {
    // Mean pixel value will throw an argument error if there
    // are no valid pixels. If that happens, it means either the
    // left or the right image is full masked.
    return false;
  }
  // Now paste the mean value into the masked pixels
  left_pyramid [0] = apply_mask(copy_mask(left_pyramid [0],create_mask(left_mask_pyramid [0],0)), left_mean  );
  right_pyramid[0] = apply_mask(copy_mask(right_pyramid[0],create_mask(right_mask_pyramid[0],0)), right_mean );

  vw_out(DebugMessage, "stereo") << "Left  pyramid base size = " << bounding_box(left_pyramid[0]) << std::endl;
  vw_out(DebugMessage, "stereo") << "Right pyramid base size = " << bounding_box(right_pyramid[0]) << std::endl;

  // Reduce the mask images from the expanded-size region to the actual sized region.
  // - The mask is not used in the expanded base of support region of the image, only the main region.
  // - The larger mask size before is used to compute the mean color values.
  // - Zero edge extension is used here so we don't treat the edge extended pixels as valid.
  //   Currently this mainly affects the SGM algorithm which needs to know not to solve for those pixels.
  BBox2i right_mask = bbox + m_search_region.min();
  right_mask.max() += m_search_region.size();
  left_mask_pyramid [0] = crop(edge_extend(m_left_mask, ZeroEdgeExtension()), bbox );
  right_mask_pyramid[0] = crop(edge_extend(m_right_mask,ZeroEdgeExtension()), right_mask);

  // Build a smoothing kernel to use before downsampling.
  // Szeliski's book recommended this simple kernel. This
  // operation is quickly becoming a time sink, we might
  // possibly want to write an integer optimized version.
  std::vector<typename DefaultKernelT<typename Image1T::pixel_type>::type > kernel(5);
  kernel[0] = kernel[4] = 1.0/16.0;
  kernel[1] = kernel[3] = 4.0/16.0;
  kernel[2] = 6.0/16.0;
  std::vector<uint8> mask_kern(max(m_kernel_size));
  std::fill(mask_kern.begin(), mask_kern.end(), 1 );

  // Smooth and downsample to build the pyramid (don't smooth the masks)
  for ( int32 i = 1; i <= max_pyramid_levels; ++i ) {
    left_pyramid      [i] = subsample(separable_convolution_filter(left_pyramid [i-1],kernel,kernel),2);
    right_pyramid     [i] = subsample(separable_convolution_filter(right_pyramid[i-1],kernel,kernel),2);
    left_mask_pyramid [i] = subsample_mask_by_two(left_mask_pyramid [i-1]);
    right_mask_pyramid[i] = subsample_mask_by_two(right_mask_pyramid[i-1]);
    
    vw_out(DebugMessage, "stereo") << "--- Created pyramid level " << i << std::endl;    
    vw_out(DebugMessage, "stereo") << "Left  pyramid size = " << bounding_box(left_pyramid[i]) << std::endl;
    vw_out(DebugMessage, "stereo") << "Right pyramid size = " << bounding_box(right_pyramid[i]) << std::endl;
  }

  // Apply the prefilter to each pyramid level
  for ( int32 i = 0; i <= max_pyramid_levels; ++i ) {
    left_pyramid [i] = prefilter_image(left_pyramid [i], m_prefilter_mode, m_prefilter_width);
    right_pyramid[i] = prefilter_image(right_pyramid[i], m_prefilter_mode, m_prefilter_width);
  }
    
  return true;
}


/// Filter out small blobs of valid pixels (they are usually bad)
template <class Image1T, class Image2T, class Mask1T, class Mask2T>
void PyramidCorrelationView<Image1T, Image2T, Mask1T, Mask2T>::
disparity_blob_filter(ImageView<pixel_typeI> &disparity, int level,
                      int max_blob_area) const {

  // Throw out blobs with this many pixels or fewer
  int scaling = 1 << level;
  int area    = max_blob_area / scaling;
  if (area < 1)
    return; // Skip if erode turned off
  //vw_out() << "Removing blobs smaller than: " << area << std::endl;


  if (0) { // DEBUG
    vw_out() << "Writing pre-blob image...\n";
    std::ostringstream ostr;
    ostr << "disparity_preblob_" << level;
    write_image( ostr.str() + ".tif", pixel_cast<PixelMask<Vector2f> >(disparity) );
    vw_out() << "Finished writing DEBUG data...\n";
  } // End DEBUG

  // Do the entire image at once!
  BBox2i tile_size = bounding_box(disparity);
  int big_size = tile_size.width();
  if (tile_size.height() > big_size) 
    big_size = tile_size.height();
    
  BlobIndexThreaded smallBlobIndex(disparity, area, big_size);
  ImageView<pixel_typeI> filtered_image = applyErodeView(disparity, smallBlobIndex);

  disparity = filtered_image;
}



template <class Image1T, class Image2T, class Mask1T, class Mask2T>
typename PyramidCorrelationView<Image1T, Image2T, Mask1T, Mask2T>::prerasterize_type
PyramidCorrelationView<Image1T, Image2T, Mask1T, Mask2T>::
prerasterize(BBox2i const& bbox) const {

    time_t start, end;
    if (m_corr_timeout){
      std::time (&start);
    }

#if VW_DEBUG_LEVEL > 0
    Stopwatch watch;
    watch.start();
#endif

    // 1.0) Determining the number of levels to process
    //      There's a maximum base on kernel size. There's also
    //      maximum defined by the search range. Here we determine
    //      the maximum based on kernel size and current bbox.
    // - max_pyramid_levels is the number of levels not including the original resolution level.
    // - Each pyramid level is shrunk by the (reduced size) kernel size on that level.
    int32 smallest_bbox      = math::min(bbox.size()); // Get smallest/largest of height/width
    int32 largest_kernel     = math::max(m_kernel_size);
    int32 max_pyramid_levels = std::floor(log(smallest_bbox)/log(2.0f) - log(largest_kernel)/log(2.0f));
    if ( m_max_level_by_search < max_pyramid_levels )
      max_pyramid_levels = m_max_level_by_search;
    if ( max_pyramid_levels < 1 )
      max_pyramid_levels = 0;
    Vector2i half_kernel = m_kernel_size/2;
    int32 max_upscaling = 1 << max_pyramid_levels;


    // 2.0) Build the pyramids
    //      - Highest resolution image is stored at index zero.
    //      - Remember that these pyramid images are larger than just the input bbox!
    std::vector<ImageView<typename Image1T::pixel_type> > left_pyramid;
    std::vector<ImageView<typename Image2T::pixel_type> > right_pyramid;
    std::vector<ImageView<typename Mask1T::pixel_type > > left_mask_pyramid;
    std::vector<ImageView<typename Mask2T::pixel_type > > right_mask_pyramid;

    if (!build_image_pyramids(bbox, max_pyramid_levels, left_pyramid, right_pyramid, 
                              left_mask_pyramid, right_mask_pyramid)){
#if VW_DEBUG_LEVEL > 0
      watch.stop();
      double elapsed = watch.elapsed_seconds();
      vw_out(DebugMessage,"stereo") << "Tile " << bbox << " has no data. Processed in " << elapsed << " s\n";
#endif
      return prerasterize_type(ImageView<result_type>(bbox.width(), bbox.height()),
                               -bbox.min().x(), -bbox.min().y(),
                               cols(), rows() );
    }
    
    // TODO: The ROI details are important, document them!
    
    // 3.0) Actually perform correlation now
    ImageView<pixel_typeI > disparity, prev_disparity;
    std::vector<stereo::SearchParam> zones; 
    // Start off the search at the lowest resolution pyramid level.  This zone covers
    // the entire image and uses the disparity range that was loaded into the class.
    BBox2i initial_disparity_range = BBox2i(0,0,m_search_region.width ()/max_upscaling+1,
                                                m_search_region.height()/max_upscaling+1);
    zones.push_back( SearchParam(bounding_box(left_mask_pyramid[max_pyramid_levels]),
                                 initial_disparity_range) );
    vw_out(DebugMessage,"stereo") << "initial_disparity_range = " << initial_disparity_range << std::endl;

    // Perform correlation. Keep track of how much time elapsed
    // since we started and stop if we estimate that doing one more
    // image chunk will bring us over time.

    // To not slow us down with timing, we use some heuristics to
    // estimate how much time elapsed, as time to do an image chunk
    // is proportional with image area times search range area. This
    // is not completely accurate, so every now and then do actual
    // timing, no more often than once in measure_spacing seconds.
    double estim_elapsed   = 0.0;
    int    measure_spacing = 2; // seconds
    double prev_estim      = estim_elapsed;

    boost::shared_ptr<SemiGlobalMatcher> sgm_matcher_ptr;
    const bool use_mgm = (m_algorithm == CORRELATION_MGM);

    // Loop down through all of the pyramid levels, low res to high res.
    for ( int32 level = max_pyramid_levels; level >= 0; --level) {

      const bool on_last_level = (level == 0);

      // In the future we could disable SGM for larger levels.
      bool use_sgm_on_level = (m_algorithm != CORRELATION_WINDOW);

      int32 scaling = 1 << level;
      if (use_sgm_on_level)
        prev_disparity = disparity; // TODO: Not efficient!!!!!!!!!!!!!!!!!!!!!!!

      disparity.set_size( left_mask_pyramid[level] ); // Note, no kernel padding here.
      
      // This is the number of padding pixels added to expand the base of support for the
      //  correlation kernel.  It matches the amount computed in build_image_pyramid().
      Vector2i region_offset = max_upscaling*half_kernel/scaling;
      
      vw_out(DebugMessage,"stereo") << "\nProcessing level: " << level 
                                    << " with size " << disparity.get_size() << std::endl;
      vw_out(DebugMessage,"stereo") << "region_offset = " << region_offset << std::endl;
      vw_out(DebugMessage,"stereo") << "Number of zones = " << zones.size() << std::endl;

      // SGM method
      if (use_sgm_on_level) {

        // Mimic processing in normal case with a single zone
        BBox2i disparity_range = BBox2i(0,0,m_search_region.width()/scaling+1,
                                            m_search_region.height()/scaling+1);
        SearchParam zone(bounding_box(left_mask_pyramid[level]), // Non-padded size
                         disparity_range);

        // TODO: Why are +1's here?        
        // The zone disparity range is not inclusive, so it is increased by one here?
        
        // The input zone is in the normal pixel coordinates for this level.
        // We need to convert it to a bbox in the expanded base of support image at this level.
        BBox2i left_region = zone.image_region() + region_offset;
          left_region.expand(half_kernel);
        BBox2i right_region = left_region;
          right_region.max() += zone.disparity_range().size();
        
        // TODO: Need to get the sizes lined up properly!
        ImageView<pixel_typeI> *prev_disp_ptr=0; // Pass in upper level disparity
        if (level != max_pyramid_levels) {
          prev_disp_ptr = &prev_disparity;
          vw_out(VerboseDebugMessage, "stereo") << "Disparity size      = " << bounding_box(disparity     ) << std::endl;
          vw_out(VerboseDebugMessage, "stereo") << "Prev Disparity size = " << bounding_box(prev_disparity) << std::endl;
        }
        
        crop(disparity, zone.image_region())
          = calc_disparity_sgm(m_cost_type,
                           crop(left_pyramid [level], left_region), 
                           crop(right_pyramid[level], right_region),
                           left_region - left_region.min(), // Specify that the whole cropped region is valid
                           zone.disparity_range().size(), 
                           m_kernel_size, use_mgm, sgm_matcher_ptr,
                           &(left_mask_pyramid[level]), &(right_mask_pyramid[level]),
                           prev_disp_ptr);
                           

        // If at the last level and the user requested a left<->right consistency check,
        //   compute right to left disparity.
        if ( m_consistency_threshold >= 0 && level == 0 ) {

          // To properly perform the reverse correlation, we need to fix the ROIs
          //  to account for the different sizes of the left and right images
          //  and make sure they line up with the previous disparity image and the
          //  input masks.
          BBox2i right_reverse_region = zone.image_region() + region_offset - m_search_region.min();
            right_reverse_region.expand(half_kernel);
          BBox2i left_reverse_region = zone.image_region() + region_offset - m_search_region.max();
            left_reverse_region.expand(half_kernel);
            left_reverse_region.max() += m_search_region.size();

          // Convert the previous image estimates to apply to the RL operation
          *prev_disp_ptr = pixel_typeI((m_search_region.max()-m_search_region.min())/2) - *prev_disp_ptr;

          boost::shared_ptr<SemiGlobalMatcher> sgm_right_matcher_ptr;
          ImageView<pixel_typeI> rl_result;
          rl_result = calc_disparity_sgm(m_cost_type,
                           crop(edge_extend(right_pyramid[level]), right_reverse_region),
                           crop(edge_extend(left_pyramid [level]), left_reverse_region),
                           right_reverse_region - right_reverse_region.min(), // Full RR region
                           zone.disparity_range().size(), 
                           m_kernel_size, use_mgm, sgm_right_matcher_ptr,
                           &(left_mask_pyramid[level]), 
                           &(right_mask_pyramid[level]),
                           prev_disp_ptr); 

          // Convert from RL to negative LR values
          rl_result += pixel_typeI(m_search_region.min()- m_search_region.max());

          // Find pixels where the disparity distance is greater than m_consistency_threshold
          const bool aligned_images = true; // The LR and RL images line up exactly
          const bool verbose        = true;
          stereo::cross_corr_consistency_check(crop(disparity,zone.image_region()),
                                               rl_result, m_consistency_threshold, 
                                               aligned_images, verbose);
                                               
        } // End of last level right to left disparity check


      } else { // Normal block matching method
      
        // 3.1) Process each zone with their refined search estimates
        // - The zones are subregions of the image with similar disparities
        //   that we identified in previous iterations.
        // - Prioritize the zones which take less time so we don't miss
        //   a bunch of tiles because we spent all our time on a slow one.
        std::sort(zones.begin(), zones.end(), SearchParamLessThan()); // Sort the zones, smallest to largest.
        BOOST_FOREACH( SearchParam const& zone, zones ) {

          // The input zone is in the normal pixel coordinates for this  level.
          // We need to convert it to a bbox in the expanded base of support image at this level.
          BBox2i left_region = zone.image_region() + region_offset; // Kernel width offset
          left_region.expand(half_kernel);
          BBox2i right_region = left_region + zone.disparity_range().min(); // Make right region contain all of
          right_region.max() += zone.disparity_range().size();              //  the needed match area.
          // Setting up the ROIs in this way means that the range of disparities calculated is always >=0

          // Check timing estimate to see if we should go ahead with this zone or quit.
          SearchParam params(left_region, zone.disparity_range());
          double next_elapsed = m_seconds_per_op * params.search_volume();
          if (m_corr_timeout > 0.0 && estim_elapsed + next_elapsed > m_corr_timeout){
            vw_out() << "Tile: " << bbox << " reached timeout: "
                     << m_corr_timeout << " s" << std::endl;
            break;
          }else
            estim_elapsed += next_elapsed;

          // See if it is time to actually accurately compute the time
          if (m_corr_timeout > 0.0 && estim_elapsed - prev_estim > measure_spacing){
            std::time (&end);
            double diff = std::difftime(end, start);
            estim_elapsed = diff;
            prev_estim = estim_elapsed;
          }

          // Compute left to right disparity vectors in this zone.
          // - The cropped regions we pass in have padding for the kernel.
          crop(disparity, zone.image_region())
            = calc_disparity(m_cost_type,
                             crop(left_pyramid [level], left_region), 
                             crop(right_pyramid[level], right_region),
                             left_region - left_region.min(), // Specify that the whole cropped region is valid
                             zone.disparity_range().size(), 
                             m_kernel_size);


          // If at the last level and the user requested a left<->right consistency check,
          //   compute right to left disparity.
          if ( m_consistency_threshold >= 0 && level == 0 ) {

            // Check the time again before moving on with this
            SearchParam params2(right_region, zone.disparity_range());
            double next_elapsed = m_seconds_per_op * params2.search_volume();
            if (m_corr_timeout > 0.0 && estim_elapsed + next_elapsed > m_corr_timeout){
              vw_out() << "Tile: " << bbox << " reached timeout: "
                       << m_corr_timeout << " s" << std::endl;
              break;
            }else{
              estim_elapsed += next_elapsed;
            }
            // Compute right to left disparity in this zone       
            
            ImageView<pixel_typeI> rl_result;
            rl_result = calc_disparity(m_cost_type,
                             crop(edge_extend(right_pyramid[level]), right_region),
                             crop(edge_extend(left_pyramid [level]),
                                  left_region - zone.disparity_range().size()),
                             right_region - right_region.min(),
                             zone.disparity_range().size(), m_kernel_size)
            - pixel_typeI(zone.disparity_range().size());


            // Find pixels where the disparity distance is greater than m_consistency_threshold
            const bool aligned_images = false; // The LR and RL images are aligned with an offset
            const bool verbose        = true;
            stereo::cross_corr_consistency_check(crop(disparity,zone.image_region()),
                                                  rl_result,
                                                 m_consistency_threshold, aligned_images, verbose);
          } // End of last level right to left disparity check

          // Fix the offsets to account for cropping.
          crop(disparity, zone.image_region()) += pixel_typeI(zone.disparity_range().min());
        } // End of zone loop
      } // End non-SGM case

      // 3.2a) Filter the disparity so we are not processing more than we need to.
      //       - Inner function filtering is only to catch "speckle" type noise of individual ouliers.
      //       - Outer function just merges the masks over the filtered disparity image.
      const int32 rm_half_kernel = 5;
      const float rm_min_matches_percent = 0.5;
      const float rm_threshold = 3.0;

      if ( !on_last_level ) {
        disparity = disparity_mask(disparity_cleanup_using_thresh
                                     (disparity,
                                      rm_half_kernel, rm_half_kernel,
                                      rm_threshold,
                                      rm_min_matches_percent),
                                     left_mask_pyramid [level],
                                     right_mask_pyramid[level]);
      } else {
      
        // We don't do a single hot pixel check on the final level as it leaves a border.
        disparity = disparity_mask(rm_outliers_using_thresh
                                     (disparity,
                                      rm_half_kernel, rm_half_kernel,
                                      rm_threshold,
                                      rm_min_matches_percent),
                                     left_mask_pyramid [level],
                                     right_mask_pyramid[level]);
      }

      // The kernel based filtering tends to leave isolated blobs behind.
      disparity_blob_filter(disparity, level, m_blob_filter_area);        

      // 3.2b) Refine search estimates but never let them go beyond
      // the search region defined by the user
      // - SGM method does not use zones.
      if ( !on_last_level && !use_sgm_on_level) {
        const size_t next_level = level-1;
        zones.clear();
        
        // On the next resolution level, break up the image area into multiple
        // smaller zones with similar disparities.  This helps minimize
        // the total amount of searching done on the image.
        subdivide_regions( disparity, bounding_box(disparity),
                           zones, m_kernel_size );
      
        scaling >>= 1;
        
        // Scale search range defines the maximum search range that
        // is possible in the next step. This (at lower levels) will
        // actually be larger than the search range that the user
        // specified. We are able to do this because we are taking
        // advantage of the half kernel padding needed at the hight
        // level of the pyramid.
        BBox2i scale_search_region(0,0,
                                   right_pyramid[next_level].cols() - left_pyramid[next_level].cols(),
                                   right_pyramid[next_level].rows() - left_pyramid[next_level].rows() );
        BBox2i next_zone_size = bounding_box( left_mask_pyramid[level-1] );
        
        BBox2i default_disparity_range = BBox2i(0,0,m_search_region.width(),
                                                    m_search_region.height());
        
        BOOST_FOREACH( SearchParam& zone, zones ) {
        
          zone.image_region() *= 2;
          zone.image_region().crop( next_zone_size );
          zone.disparity_range() *= 2;
          zone.disparity_range().expand(2); // This is practically required. Our
          // correlation will fail if the search has only one solution.
          // - Increasing this expansion number improves results slightly but
          //   significantly increases the processing times.
          
          zone.disparity_range().crop( scale_search_region );
          
          if (zone.disparity_range().empty()) {
            zone.disparity_range() = default_disparity_range; // Reset invalid disparity!
          }
        } // End zone update loop

      } // End zone handling
      
      if (m_write_debug_images) { // DEBUG
        vw_out() << "Writing DEBUG data...\n";
        BBox2i scaled = bbox/2;
        std::ostringstream ostr;
        ostr << "disparity_" << scaled.min()[0] << "_"
             << scaled.min()[1] << "_" << scaled.max()[0] << "_"
             << scaled.max()[1] << "_" << level;
        write_image( ostr.str() + ".tif", pixel_cast<PixelMask<Vector2f> >(disparity) );
        std::ofstream f( (ostr.str() + "_zone.txt").c_str() );
        BOOST_FOREACH( SearchParam& zone, zones ) {
          f << zone.image_region() << " " << zone.disparity_range() << "\n";
        }
        write_image( ostr.str() + "left.tif",  left_pyramid [level] );
        write_image( ostr.str() + "right.tif", right_pyramid[level] );
        write_image( ostr.str() + "lmask.tif", left_mask_pyramid [level] );
        write_image( ostr.str() + "rmask.tif", right_mask_pyramid[level] );
        f.close();
        vw_out() << "Finished writing DEBUG data...\n";
      } // End DEBUG
      
      //if (level < 4)
      //  vw_throw( NoImplErr() << "DEBUG" );
      
    } // End of the level loop

    VW_ASSERT( bbox.size() == bounding_box(disparity).size(),
               MathErr() << "PyramidCorrelation: Solved disparity doesn't match requested bbox size." );

#if VW_DEBUG_LEVEL > 0
    watch.stop();
    double elapsed = watch.elapsed_seconds();
    vw_out(DebugMessage,"stereo") << "Tile " << bbox << " processed in "
                                  << elapsed << " s\n";
    if (m_corr_timeout > 0.0){
      vw_out(DebugMessage,"stereo")
        << "Elapsed (actual/estimated/ratio): " << elapsed << ' '
        << estim_elapsed << ' ' << elapsed/estim_elapsed << std::endl;
    }
#endif

    // 5.0) Reposition our result back into the global solution. 
    // Also we need to correct for the offset we applied to the search region.
    // - At this point we either cast to floating point or run a subpixel refinement algorithm.

    if (m_algorithm != CORRELATION_WINDOW) {
    
      // For SGM, subpixel correlation is performed here, not in stereo_rfne.     
      return prerasterize_type(sgm_matcher_ptr->create_disparity_view_subpixel(disparity) 
                                 + result_type(m_search_region.min()),
                               -bbox.min().x(), -bbox.min().y(),
                               cols(), rows() );      
    } else {
      // TODO CLEANUP
      ImageView<pixel_typeI> temp = disparity + pixel_typeI(m_search_region.min());
      ImageView<result_type> float_type = pixel_cast<result_type, ImageView<pixel_typeI> >(temp);
      return prerasterize_type(float_type,
                               -bbox.min().x(), -bbox.min().y(),
                               cols(), rows() );
    }
  } // End function prerasterize



}} // namespace stereo

