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


/// \file AdjustSparse.h
///
/// Sparse implementation of bundle adjustment. Fast yo!

#ifndef __VW_BUNDLEADJUSTMENT_ADJUST_SPARSE_H__
#define __VW_BUNDLEADJUSTMENT_ADJUST_SPARSE_H__

// Vision Workbench
#include <vw/Math/MatrixSparseSkyline.h>
#include <vw/Core/Debugging.h>
#include <vw/BundleAdjustment/AdjustBase.h>
#include <vw/BundleAdjustment/CameraRelation.h>

// Boost
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/vector_sparse.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/version.hpp>
#if BOOST_VERSION<=103200
// Mapped matrix doesn't exist in 1.32, but Sparse Matrix does
//
// Unfortunately some other tests say this doesn't work
#define boost_sparse_matrix boost::numeric::ublas::sparse_matrix
#define boost_sparse_vector boost::numeric::ublas::sparse_vector
#else
// Sparse Matrix was renamed Mapped Matrix in later editions
#define boost_sparse_matrix boost::numeric::ublas::mapped_matrix
#define boost_sparse_vector boost::numeric::ublas::mapped_vector
#endif

namespace vw {
namespace ba {

  template <class BundleAdjustModelT, class RobustCostT>
  class AdjustSparse : public AdjustBase<BundleAdjustModelT, RobustCostT> {

    // Common Types
    typedef Matrix<double, 2, BundleAdjustModelT::camera_params_n> matrix_2_camera;
    typedef Matrix<double, 2, BundleAdjustModelT::point_params_n> matrix_2_point;
    typedef Matrix<double,BundleAdjustModelT::camera_params_n,BundleAdjustModelT::camera_params_n> matrix_camera_camera;
    typedef Matrix<double,BundleAdjustModelT::point_params_n,BundleAdjustModelT::point_params_n> matrix_point_point;
    typedef Matrix<double,BundleAdjustModelT::camera_params_n,BundleAdjustModelT::point_params_n> matrix_camera_point;
    typedef Vector<double,BundleAdjustModelT::camera_params_n> vector_camera;
    typedef Vector<double,BundleAdjustModelT::point_params_n> vector_point;

    math::MatrixSparseSkyline<double> m_S;
    std::vector<size_t> m_ideal_ordering;
    Vector<size_t> m_ideal_skyline;
    bool m_found_ideal_ordering;
    CameraRelationNetwork<JFeature> m_crn;
    typedef CameraNode<JFeature>::iterator crn_iter;

    // Reused structures
    std::vector< matrix_camera_camera > U;
    std::vector< matrix_point_point > V, V_inverse;
    std::vector< vector_camera > epsilon_a;
    std::vector< vector_point > epsilon_b;

  public:

    AdjustSparse( BundleAdjustModelT & model,
                  RobustCostT const& robust_cost_func,
                  bool use_camera_constraint=true,
                  bool use_gcp_constraint=true) :
    AdjustBase<BundleAdjustModelT,RobustCostT>( model, robust_cost_func,
                                                use_camera_constraint,
                                                use_gcp_constraint ),
      U( this->m_model.num_cameras() ), V( this->m_model.num_points() ),
      V_inverse( this->m_model.num_points() ),
      epsilon_a( this->m_model.num_cameras() ), epsilon_b( this->m_model.num_points() ) {
      vw_out(DebugMessage,"ba") << "Constructed Sparse Bundle Adjuster.\n";
      m_crn.read_controlnetwork( *(this->m_control_net).get() );
      m_found_ideal_ordering = false;
    }

    math::MatrixSparseSkyline<double> S() const { return m_S; }

    // Covariance Calculator
    // ___________________________________________________________
    // This routine inverts a sparse matrix S, and prints the individual
    // covariance matrices for each camera
    void covCalc(){
      // camera params
      size_t num_cam_params = BundleAdjustModelT::camera_params_n;
      size_t num_cameras = this->m_model.num_cameras();

      size_t inverse_size = num_cam_params * num_cameras;

      typedef Matrix<double, BundleAdjustModelT::camera_params_n, BundleAdjustModelT::camera_params_n> matrix_camera_camera;

      // final vector of camera covariance matrices
      vw::Vector< matrix_camera_camera > sparse_cov(num_cameras);

      // Get the S matrix from the model
      math::MatrixSparseSkyline<double> S = this->S();  // Make copy as solve is destructive
      Matrix<double> Id(inverse_size, inverse_size);
      Id.set_identity();
      Matrix<double> Cov = multi_sparse_solve(S, Id);

      //pick out covariances of individual cameras
      for ( size_t i = 0; i < num_cameras; i++ )
        sparse_cov(i) = submatrix(Cov, i*num_cam_params, i*num_cam_params, num_cam_params, num_cam_params);

      std::cout << "Covariance matrices for cameras are:"
                << sparse_cov << "\n\n";
    }

    // UPDATE IMPLEMENTATION
    //-------------------------------------------------------------
    // This is the sparse levenberg marquardt update step.  Returns
    // the average improvement in the cost function.
    double update(double &abs_tol, double &rel_tol) {
      ++this->m_iterations;
      boost::scoped_ptr<Timer> time;

      VW_DEBUG_ASSERT(this->m_control_net->size() == this->m_model.num_points(), LogicErr() << "BundleAdjustment::update() : Number of bundles does not match the number of points in the bundle adjustment model.");

      // Reseting the values for U, V, epsilon
      BOOST_FOREACH( matrix_camera_camera& element, U )
        element = matrix_camera_camera();
      BOOST_FOREACH( vector_camera& element, epsilon_a )
        element = vector_camera();
      BOOST_FOREACH( matrix_point_point& element, V )
        element = matrix_point_point();
      BOOST_FOREACH( vector_point& element, epsilon_b )
        element = vector_point();

      size_t num_cam_params = BundleAdjustModelT::camera_params_n;
      size_t num_pt_params = BundleAdjustModelT::point_params_n;

      // Populate the Jacobian, which is broken into two sparse
      // matrices A & B, as well as the error matrix and the W
      // matrix.
      time.reset(new Timer("Solve for Image Error, Jacobian, U, V, and W:", DebugMessage, "ba"));
      double error_total = 0; // assume this is r^T\Sigma^{-1}r
      for ( size_t j = 0; j < m_crn.size(); j++ ) {
        BOOST_FOREACH( boost::shared_ptr<JFeature> measure, m_crn[j] ) {
          size_t i = measure->m_point_id;

          matrix_2_camera A =
            this->m_model.cam_jacobian( i, j,
                                      this->m_model.cam_params(j),
                                      this->m_model.point_params(i) );
          matrix_2_point B =
            this->m_model.point_jacobian( i, j,
                                      this->m_model.cam_params(j),
                                      this->m_model.point_params(i) );

          // Apply robust cost function weighting
          Vector2 error;
          try {
            error = measure->m_location -
              this->m_model.cam_pixel(i,j,this->m_model.cam_params(j),
                            this->m_model.point_params(i) );
          } catch (const camera::PointToPixelErr& e) {}

          if ( error != Vector2() ) {
            double mag = norm_2(error);
            double weight = sqrt(this->m_robust_cost_func(mag)) / mag;
            error *= weight;
          }

          Matrix2x2 inverse_cov;
          Vector2 pixel_sigma = measure->m_scale;
          inverse_cov(0,0) = 1/(pixel_sigma(0)*pixel_sigma(0));
          inverse_cov(1,1) = 1/(pixel_sigma(1)*pixel_sigma(1));
          error_total += .5 * transpose(error) *
            inverse_cov * error;

          // Storing intermediate values
          U[j] += transpose(A) * inverse_cov * A;
          V[i] += transpose(B) * inverse_cov * B;
          epsilon_a[j] += transpose(A) * inverse_cov * error;
          epsilon_b[i] += transpose(B) * inverse_cov * error;
          measure->m_w = transpose(A) * inverse_cov * B;
        }
      }
      time.reset();

      // Add in the camera position and pose constraint terms and covariances.
      time.reset(new Timer("Solving for Camera and GCP error:",DebugMessage,"ba"));
      if ( this->m_use_camera_constraint )
        for ( size_t j = 0; j < U.size(); ++j ) {
          matrix_camera_camera inverse_cov =
            this->m_model.cam_inverse_covariance(j);
          U[j] += inverse_cov;
          vector_camera eps_a = this->m_model.cam_target(j)-this->m_model.cam_params(j);
          error_total += .5  * transpose(eps_a) * inverse_cov * eps_a;
          epsilon_a[j] += inverse_cov * eps_a;
        }

      // Add in the 3D point position constraint terms and
      // covariances. We only add constraints for Ground Control
      // Points (GCPs), not for 3D tie points.
      if (this->m_use_gcp_constraint)
        for ( size_t i = 0; i < V.size(); ++i )
          if ((*this->m_control_net)[i].type() == ControlPoint::GroundControlPoint) {
            matrix_point_point inverse_cov;
            inverse_cov = this->m_model.point_inverse_covariance(i);
            V[i] += inverse_cov;
            vector_point eps_b = this->m_model.point_target(i)-this->m_model.point_params(i);
            error_total += .5 * transpose(eps_b) * inverse_cov * eps_b;
            epsilon_b[i] += inverse_cov * eps_b;
          }
      time.reset();

      // set initial lambda, and ignore if the user has touched it
      if ( this->m_iterations == 1 && this->m_lambda == 1e-3 ) {
        time.reset(new Timer("Solving for Lambda:", DebugMessage, "ba"));
        double max = 0.0;
        BOOST_FOREACH( matrix_camera_camera& element, U )
          for (size_t j = 0; j < BundleAdjustModelT::camera_params_n; ++j){
            if (fabs(element(j,j)) > max)
              max = fabs(element(j,j));
          }
        BOOST_FOREACH( matrix_point_point& element, V )
          for (size_t j = 0; j < BundleAdjustModelT::point_params_n; ++j) {
            if ( fabs(element(j,j)) > max)
              max = fabs(element(j,j));
          }
        this->m_lambda = max * 1e-10;
        time.reset();
      }

      time.reset(new Timer("Augmenting with lambda",DebugMessage,"ba"));
      //e at this point should be -g_a

      // "Augment" the diagonal entries of the U and V matrices with
      // the parameter lambda.
      {
        matrix_camera_camera u_lambda;
        u_lambda.set_identity();
        u_lambda *= this->m_lambda;
        BOOST_FOREACH( matrix_camera_camera& element, U )
          element += u_lambda;
      }

      {
        matrix_point_point v_lambda;
        v_lambda.set_identity();
        v_lambda *= this->m_lambda;
        BOOST_FOREACH( matrix_point_point& element, V )
          element += v_lambda;
      }
      time.reset();

      // Create the 'e' vector in S * delta_a = e.  The first step is
      // to "flatten" our block structure to a vector that contains
      // scalar entries.
      time.reset(new Timer("Create special e vector", DebugMessage, "ba"));
      Vector<double> e(this->m_model.num_cameras() * BundleAdjustModelT::camera_params_n);
      for (size_t j = 0; j < epsilon_a.size(); ++j) {
        subvector(e, j*BundleAdjustModelT::camera_params_n, BundleAdjustModelT::camera_params_n) =
          epsilon_a[j];
      }

      // Compute V inverse
      for ( size_t i = 0; i < this->m_model.num_points(); i++ ) {
        Matrix<double> V_temp = V[i];
        chol_inverse( V_temp );
        V_inverse[i] = transpose(V_temp)*V_temp;
      }

      // Compute Y and finish constructing e.
      for ( size_t j = 0; j < m_crn.size(); j++ ) {
        for ( crn_iter fiter = m_crn[j].begin();
              fiter != m_crn[j].end(); fiter++ ) {
          // Compute the blocks of Y
          (**fiter).m_y = (**fiter).m_w * V_inverse[(**fiter).m_point_id];
          // Flatten the block structure to compute 'e'
          subvector(e, j*num_cam_params, num_cam_params) -= (**fiter).m_y
            * epsilon_b[ (**fiter).m_point_id ];
        }
      }

      time.reset();

      // --- BUILD SPARSE, SOLVE A'S UPDATE STEP -------------------------
      time.reset(new Timer("Build Sparse", DebugMessage, "ba"));

      // The S matrix is a m x m block matrix with blocks that are
      // camera_params_n x camera_params_n in size.  It has a sparse
      // skyline structure, which makes it more efficient to solve
      // through L*D*L^T decomposition and forward/back substitution
      // below.
      math::MatrixSparseSkyline<double> S(this->m_model.num_cameras()*num_cam_params,
                                          this->m_model.num_cameras()*num_cam_params);
      for ( size_t j = 0; j < m_crn.size(); j++ ) {
        { // Filling in diagonal
          matrix_camera_camera S_jj;

          // Iterate across all features seen by the camera
          for ( crn_iter fiter = m_crn[j].begin();
                fiter != m_crn[j].end(); fiter++ ) {
            S_jj -= (**fiter).m_y*transpose((**fiter).m_w);
          }

          // Augmenting Diagonal
          S_jj += U[j];

          // Loading into sparse matrix
          size_t offset = j * num_cam_params;
          for ( size_t aa = 0; aa < num_cam_params; aa++ ) {
            for ( size_t bb = aa; bb < num_cam_params; bb++ ) {
              S( offset+bb, offset+aa ) = S_jj(aa,bb);  // Transposing
            }
          }
        }

        // Filling in off diagonal
        for ( size_t k = j+1; k < m_crn.size(); k++ ) {
          typedef boost::weak_ptr<JFeature> w_ptr;
          typedef boost::shared_ptr<JFeature> f_ptr;
          typedef std::multimap< size_t, f_ptr >::iterator mm_iterator;
          std::pair< mm_iterator, mm_iterator > feature_range;
          feature_range = m_crn[j].map.equal_range( k );

          // Iterating through all features in camera j that have
          // connections to camera k.
          matrix_camera_camera S_jk;
          bool found = false;
          for ( mm_iterator f_j_iter = feature_range.first;
                f_j_iter != feature_range.second; f_j_iter++ ) {
            w_ptr f_k = (*f_j_iter).second->m_map[k];
            found = true;
            S_jk -= (*f_j_iter).second->m_y *
              transpose( f_k.lock()->m_w );
          }

          // Loading into sparse matrix
          // - if it seems we are loading in oddly, it's because the sparse
          //   matrix is row major.
          if ( found ) {
            submatrix( S, k*num_cam_params, j*num_cam_params,
                       num_cam_params, num_cam_params ) = transpose(S_jk);
          }
        }
      }

      m_S = S; // S is modified in sparse solve. Keeping a copy.
      time.reset();

      // Computing ideal ordering
      if (!m_found_ideal_ordering) {
        time.reset(new Timer("Solving Cuthill-Mckee", DebugMessage, "ba"));
        m_ideal_ordering = cuthill_mckee_ordering(S,num_cam_params);
        math::MatrixReorganize<math::MatrixSparseSkyline<double> > mod_S( S, m_ideal_ordering );
        m_ideal_skyline = solve_for_skyline(mod_S);

        m_found_ideal_ordering = true;
        time.reset();
      }

      time.reset(new Timer("Solve Delta A", DebugMessage, "ba"));

      // Compute the LDL^T decomposition and solve using sparse methods.
      math::MatrixReorganize<math::MatrixSparseSkyline<double> > modified_S( S, m_ideal_ordering );
      Vector<double> delta_a = sparse_solve( modified_S,
                                             reorganize(e, m_ideal_ordering),
                                             m_ideal_skyline );
      delta_a = reorganize(delta_a, modified_S.inverse());
      BOOST_FOREACH( double& e, delta_a )
        if ( std::isnan( e ) ) e = 0;
      time.reset();

      // --- SOLVE B'S UPDATE STEP ---------------------------------

      // Back Solving for Delta B
      time.reset(new Timer("Solve Delta B", DebugMessage, "ba"));
      Vector<double> delta_b( this->m_model.num_points() * num_pt_params );
      {
        // delta_b = inverse(V)*( epsilon_b - sum_across_cam( WijT * delta_aj ) )

        // Building right half, sum( WijT * delta_aj )
        std::vector< vector_point > right_delta_b( this->m_model.num_points() );
        for ( size_t j = 0; j < m_crn.size(); j++ ) {
          for ( crn_iter fiter = m_crn[j].begin();
                fiter != m_crn[j].end(); fiter++ ) {
            right_delta_b[ (**fiter).m_point_id ] += transpose( (**fiter).m_w ) *
              subvector( delta_a, j*num_cam_params, num_cam_params );
          }
        }

        // Solving for delta b
        for ( size_t i = 0; i < this->m_model.num_points(); i++ ) {
          Vector<double> delta_temp = epsilon_b[i] - right_delta_b[i];
          Matrix<double> hessian = V[i];
          solve( delta_temp, hessian );
          subvector( delta_b, i*num_pt_params, num_pt_params ) = delta_temp;
        }
      }
      time.reset();

      //Predicted improvement for Fletcher modification
      double dS = 0;
      for ( size_t j = 0; j < this->m_model.num_cameras(); j++ )
        dS += transpose(subvector(delta_a,j*num_cam_params,num_cam_params))
          * ( this->m_lambda * subvector(delta_a,j*num_cam_params,num_cam_params) +
              epsilon_a[j] );
      for ( size_t i = 0; i < this->m_model.num_points(); i++ )
        dS += transpose(subvector(delta_b,i*num_pt_params,num_pt_params))
          * ( this->m_lambda * subvector(delta_b,i*num_pt_params,num_pt_params) +
              epsilon_b[i] );
      dS *= 0.5;

      // -------------------------------
      // Compute the update error vector and predicted change
      // -------------------------------
      time.reset(new Timer("Solve for Updated Error", DebugMessage, "ba"));
      double new_error_total = 0;
      for ( size_t j = 0; j < m_crn.size(); j++ ) {
        for ( crn_iter fiter = m_crn[j].begin();
              fiter != m_crn[j].end(); fiter++ ) {
          // Compute error vector
          vector_camera new_a = this->m_model.cam_params(j) +
            subvector( delta_a, num_cam_params*j, num_cam_params );
          vector_point new_b = this->m_model.point_params((**fiter).m_point_id) +
            subvector( delta_b, num_pt_params*(**fiter).m_point_id, num_pt_params );

          // Apply robust cost function weighting
          Vector2 error;
          try {
            error = (**fiter).m_location -
              this->m_model.cam_pixel((**fiter).m_point_id,j,new_a,new_b);
          } catch (const camera::PointToPixelErr& e) {}
          double mag = norm_2( error );
          double weight = sqrt( this->m_robust_cost_func(mag)) / mag;
          error *= weight;

          Matrix2x2 inverse_cov;
          Vector2 pixel_sigma = (**fiter).m_scale;
          inverse_cov(0,0) = 1/(pixel_sigma(0)*pixel_sigma(0));
          inverse_cov(1,1) = 1/(pixel_sigma(1)*pixel_sigma(1));

          new_error_total += .5 * transpose(error) *
            inverse_cov * error;
        }
      }

      // Camera Constraints
      if ( this->m_use_camera_constraint )
        for (size_t j = 0; j < U.size(); ++j) {

          vector_camera new_a = this->m_model.cam_params(j) +
            subvector(delta_a, num_cam_params*j, num_cam_params);
          vector_camera eps_a = this->m_model.cam_target(j)-new_a;

          matrix_camera_camera inverse_cov;
          inverse_cov = this->m_model.cam_inverse_covariance(j);
          new_error_total += .5 * transpose(eps_a) * inverse_cov * eps_a;
        }

      // GCP Error
      if ( this->m_use_gcp_constraint )
        for ( size_t i = 0; i < V.size(); ++i )
          if ( (*this->m_control_net)[i].type() ==
               ControlPoint::GroundControlPoint) {

            vector_point new_b = this->m_model.point_params(i) +
              subvector( delta_b, num_pt_params*i, num_pt_params );
            vector_point eps_b = this->m_model.point_target(i)-new_b;
            matrix_point_point inverse_cov;
            inverse_cov = this->m_model.point_inverse_covariance(i);
            new_error_total += .5 * transpose(eps_b) * inverse_cov * eps_b;
          }
      time.reset();

      //Fletcher modification
      double Splus = new_error_total;     //Compute new objective
      double SS = error_total;            //Compute old objective
      double R = (SS - Splus)/dS;         // Compute ratio
      vw_out(DebugMessage,"ba") << "New Error: " << new_error_total << std::endl;
      vw_out(DebugMessage,"ba") << "Old Error: " << error_total << std::endl;

      rel_tol = -1e30;
      BOOST_FOREACH( vector_camera const& a, epsilon_a )
        rel_tol = std::max( rel_tol, math::max( abs( a ) ) );
      BOOST_FOREACH( vector_point const& b, epsilon_b )
        rel_tol = std::max( rel_tol, math::max( abs( b ) ) );
      abs_tol = Splus;

      if ( R > 0 ) {

        time.reset(new Timer("Setting Parameters",DebugMessage,"ba"));
        for (size_t j = 0; j < this->m_model.num_cameras(); ++j)
          this->m_model.set_cam_params(j, this->m_model.cam_params(j) +
                                         subvector(delta_a, num_cam_params*j,num_cam_params));
        for (size_t i = 0; i < this->m_model.num_points(); ++i)
          this->m_model.set_point_params(i, this->m_model.point_params(i) +
                                         subvector(delta_b, num_pt_params*i,num_pt_params));
        time.reset();

        if ( this->m_control == 0 ) {
          double temp = 1 - pow((2*R - 1),3);
          if (temp < 1.0/3.0)
            temp = 1.0/3.0;

          this->m_lambda *= temp;
          this->m_nu = 2;
        } else if (this->m_control == 1)
          this->m_lambda /= 10;

        return SS-Splus;
      }

      // Didn't make progress ...
      if ( this->m_control == 0 ) {
        this->m_lambda *= this->m_nu;
        this->m_nu*=2;
      } else if ( this->m_control == 1 )
        this->m_lambda *= 10;

      return 0;
    }

  };

}} // namespace vw::ba

#endif//__VW_BUNDLEADJUSTMENT_ADJUST_SPARSE_H__
