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


#include <gtest/gtest_VW.h>
#include <vw/Core/CompoundTypes.h>
#include <boost/numeric/conversion/cast.hpp>

using namespace vw;

// A simple compound type.  We only test with this one type here, and thus
// we do not exercise all of the specialized code paths.  However, those
// do largely get exercized by the pixel math tests in the Image module.
template <class ChannelT>
class TestCompound {
  ChannelT values[2];
public:
  TestCompound( ChannelT a, ChannelT b ) { values[0]=a; values[1]=b; }
  ChannelT& operator[]( size_t i ) { return values[i]; }
  ChannelT const& operator[]( size_t i ) const { return values[i]; }
};

// Simple functions to test the compound_apply logic.
template <class T> T add( T const& a, T const& b ) { return boost::numeric_cast<T>(a + b); }
template <class T> void add_in_place( T& a, T const& b ) { a += b; }
template <class T> T add_one( T const& val ) { return val + 1; }
template <class T> void add_one_in_place( T& val ) { val += 1; }

// A dummy type that is neither a scalar nor a compound type.
class DummyType {};

namespace vw {
  template<class ChannelT> struct CompoundChannelType<TestCompound<ChannelT> > { typedef ChannelT type; };
  template<class ChannelT> struct CompoundNumChannels<TestCompound<ChannelT> > { static const size_t value = 2; };
  template<class InT, class OutT> struct CompoundChannelCast<TestCompound<InT>, OutT> { typedef TestCompound<OutT> type; };
}


template <class T1, class T2>
static bool is_of_type( T2 ) {
  return boost::is_same<T1,T2>::value;
}

TEST(CompoundTypes, Basic) {
  EXPECT_TRUE(( boost::is_same<CompoundChannelType<TestCompound<float> >::type, float>::value ));
  // TODO: why is this an error when the next one isn't? EXPECT_EQ( CompoundNumChannels<TestCompound<float> >::value, 2 );
  EXPECT_EQ( 2u, size_t(CompoundNumChannels<TestCompound<float> >::value) );
  EXPECT_TRUE(( boost::is_same<CompoundChannelCast<TestCompound<float>,int>::type, TestCompound<int> >::value ));
}

TEST(CompoundTypes, Traits) {
  EXPECT_TRUE(( !IsCompound<uint8>::value ));
  EXPECT_TRUE(( !IsCompound<double>::value ));
  EXPECT_TRUE(( IsCompound<TestCompound<uint8> >::value ));
  EXPECT_TRUE(( IsCompound<TestCompound<double> >::value ));
  EXPECT_TRUE(( !IsCompound<DummyType>::value ));
  EXPECT_TRUE(( !IsCompound<const uint8>::value ));
  EXPECT_TRUE(( !IsCompound<const double>::value ));
  EXPECT_TRUE(( IsCompound<TestCompound<const uint8> >::value ));
  EXPECT_TRUE(( IsCompound<TestCompound<const double> >::value ));
  EXPECT_TRUE(( !IsCompound<const DummyType>::value ));

  EXPECT_TRUE(( IsScalarOrCompound<uint8>::value ));
  EXPECT_TRUE(( IsScalarOrCompound<double>::value ));
  EXPECT_TRUE(( IsScalarOrCompound<TestCompound<uint8> >::value ));
  EXPECT_TRUE(( IsScalarOrCompound<TestCompound<double> >::value ));
  EXPECT_TRUE(( !IsScalarOrCompound<DummyType>::value ));
  EXPECT_TRUE(( IsScalarOrCompound<const uint8>::value ));
  EXPECT_TRUE(( IsScalarOrCompound<const double>::value ));
  EXPECT_TRUE(( IsScalarOrCompound<const TestCompound<uint8> >::value ));
  EXPECT_TRUE(( IsScalarOrCompound<const TestCompound<double> >::value ));
  EXPECT_TRUE(( !IsScalarOrCompound<const DummyType>::value ));

  EXPECT_TRUE(( CompoundIsCompatible<double,double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<uint8,double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<TestCompound<double>,TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<TestCompound<uint8>,TestCompound<double> >::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<TestCompound<double>,double>::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<double,TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const double,double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const uint8,double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const TestCompound<double>,TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const TestCompound<uint8>,TestCompound<double> >::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<const TestCompound<double>,double>::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<const double,TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<double,const double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<uint8,const double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<TestCompound<double>,const TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<TestCompound<uint8>,const TestCompound<double> >::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<TestCompound<double>,const double>::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<double,const TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const double,const double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const uint8,const double>::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const TestCompound<double>,const TestCompound<double> >::value ));
  EXPECT_TRUE(( CompoundIsCompatible<const TestCompound<uint8>,const TestCompound<double> >::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<const TestCompound<double>,const double>::value ));
  EXPECT_TRUE(( !CompoundIsCompatible<const double,const TestCompound<double> >::value ));

  EXPECT_TRUE(( boost::is_same<CompoundAccumulatorType<uint8>::type,AccumulatorType<uint8>::type>::value ));
  EXPECT_TRUE(( boost::is_same<CompoundAccumulatorType<TestCompound<uint8> >::type,TestCompound<AccumulatorType<uint8>::type> >::value ));
  EXPECT_TRUE(( boost::is_same<CompoundAccumulatorType<const uint8>::type,AccumulatorType<uint8>::type>::value ));
  EXPECT_TRUE(( boost::is_same<CompoundAccumulatorType<const TestCompound<uint8> >::type,TestCompound<AccumulatorType<uint8>::type> >::value ));
}

TEST(CompoundTypes, CompoundSelectChannel) {
  uint32 vali = 3;
  EXPECT_EQ( 3u, compound_select_channel<const uint32&>(vali,0) );
  compound_select_channel<uint32&>(vali,0) = 5;
  EXPECT_EQ( 5u, compound_select_channel<const uint32&>(vali,0) );
  float valf = 4.0;
  EXPECT_EQ( 4.0, compound_select_channel<const float&>(valf,0) );
  compound_select_channel<float&>(valf,0) = 6;
  EXPECT_EQ( 6.0, compound_select_channel<const float&>(valf,0) );
  TestCompound<uint32> valci( 1, 2 );
  EXPECT_EQ( 1u, compound_select_channel<const uint32&>(valci,0) );
  EXPECT_EQ( 2u, compound_select_channel<const uint32&>(valci,1) );
  compound_select_channel<uint32&>(valci,0) = 3;
  compound_select_channel<uint32&>(valci,1) = 4;
  EXPECT_EQ( 3u, compound_select_channel<const uint32&>(valci,0) );
  EXPECT_EQ( 4u, compound_select_channel<const uint32&>(valci,1) );
  TestCompound<float> valcf( 2.0, 3.0 );
  EXPECT_EQ( 2.0, compound_select_channel<const float&>(valcf,0) );
  EXPECT_EQ( 3.0, compound_select_channel<const float&>(valcf,1) );
  compound_select_channel<float&>(valcf,0) = 3.0;
  compound_select_channel<float&>(valcf,1) = 4.0;
  EXPECT_EQ( 3.0, compound_select_channel<const float&>(valcf,0) );
  EXPECT_EQ( 4.0, compound_select_channel<const float&>(valcf,1) );
}

TEST(CompoundTypes, BinaryCompoundApply) {
  uint32 ai=1, bi=2, ci=compound_apply(&add<uint32>, ai, bi);
  EXPECT_EQ( 3u, ci );
  float af=1.0, bf=2.0, cf=compound_apply(&add<float>, af, bf);
  EXPECT_EQ( 3.0, cf );
  TestCompound<uint32> aci(1,2), bci(3,4), cci=compound_apply(&add<uint32>, aci, bci);
  EXPECT_EQ( 4u, cci[0] );
  EXPECT_EQ( 6u, cci[1] );
  TestCompound<float> acf(1,2), bcf(3,4), ccf=compound_apply(&add<float>, acf, bcf);
  EXPECT_EQ( 4, ccf[0] );
  EXPECT_EQ( 6, ccf[1] );
}

TEST(CompoundTypes, BinaryCompoundApplyInPlace) {
  uint32 ai=1, bi=2;
  compound_apply_in_place(&add_in_place<uint32>, ai, bi);
  EXPECT_EQ( 3u, ai );
  float af=1.0, bf=2.0;
  compound_apply_in_place(&add_in_place<float>, af, bf);
  EXPECT_EQ( 3.0, af );
  TestCompound<uint32> aci(1,2), bci(3,4);
  compound_apply_in_place(&add_in_place<uint32>, aci, bci);
  EXPECT_EQ( 4u, aci[0] );
  EXPECT_EQ( 6u, aci[1] );
  TestCompound<float> acf(1,2), bcf(3,4);
  compound_apply_in_place(&add_in_place<float>, acf, bcf);
  EXPECT_EQ( 4, acf[0] );
  EXPECT_EQ( 6, acf[1] );
}

TEST(CompoundTypes, UnaryCompoundApply) {
  uint32 ai=1, bi=compound_apply(&add_one<uint32>, ai);
  EXPECT_EQ( 2u, bi );
  float af=1.0, bf=compound_apply(&add_one<float>, af);
  EXPECT_EQ( 2.0, bf );
  TestCompound<uint32> aci(1,2), bci=compound_apply(&add_one<uint32>, aci);
  EXPECT_EQ( 2u, bci[0] );
  EXPECT_EQ( 3u, bci[1] );
  TestCompound<float> acf(1,2), bcf=compound_apply(&add_one<float>, acf);
  EXPECT_EQ( 2, bcf[0] );
  EXPECT_EQ( 3, bcf[1] );
}

TEST(CompoundTypes, UnaryCompoundApplyInPlace) {
  uint32 vali = 0;
  compound_apply_in_place( &add_one_in_place<uint32>, vali );
  EXPECT_EQ( 1.0, vali );
  double valf = 0.0;
  compound_apply_in_place( &add_one_in_place<double>, valf );
  EXPECT_EQ( 1.0, valf );
  TestCompound<int> valc( 1, 2 );
  compound_apply_in_place( &add_one_in_place<int>, valc );
  EXPECT_EQ( 2, valc[0] );
  EXPECT_EQ( 3, valc[1] );
}
