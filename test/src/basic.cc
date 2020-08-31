#include <ivanp/hist/histograms.hh>
#include <climits>
#include <array>
#include <list>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

TEST_CASE( "1d histogram with cont_axis", "[hist]" ) {
  using hist_t = ivanp::hist::histogram<>;
  hist_t h({{1,2,3,5,7,11,13}});

  REQUIRE( h.nbins() == 8 );
  REQUIRE( h.size() == 8 );
  REQUIRE( h.bins().size() == 8 );
  REQUIRE( h.ndim() == 1 );

  const auto& ax = h.axis(0);
  REQUIRE( ax.nbins() == 8 );
  REQUIRE( ax.ndiv() == 6 );
  REQUIRE( ax.nedges() == 7 );

  REQUIRE( ax.min() == 1 );
  REQUIRE( ax.max() == 13 );

  REQUIRE( ax.lower(0) == -std::numeric_limits<double>::infinity() );
  REQUIRE( ax.upper(0) == 1 );
  REQUIRE( ax.lower(4) == 5 );
  REQUIRE( ax.upper(4) == 7 );
  REQUIRE( ax.lower(6) == 11 );
  REQUIRE( ax.upper(6) == 13 );
  REQUIRE( ax.lower(7) == 13 );
  REQUIRE( ax.upper(7) == std::numeric_limits<double>::infinity() );
  REQUIRE( ax.lower(8) == std::numeric_limits<double>::infinity() );
  REQUIRE( ax.upper(8) == std::numeric_limits<double>::infinity() );
}

TEST_CASE( "1d histogram with uniform_axis", "[hist]" ) {
  using hist_t = ivanp::hist::histogram<
    double,
    ivanp::hist::axes_spec<
      std::array<ivanp::hist::uniform_axis<double>,1> >
  >;
  hist_t h({{{10,0,1}}});

  REQUIRE( h.nbins() == 12 );
  REQUIRE( h.size() == 12 );
  REQUIRE( h.bins().size() == 12 );
  REQUIRE( h.ndim() == 1 );

  const auto& ax = h.axis<0>();
  REQUIRE( ax.nbins() == 12 );
  REQUIRE( ax.ndiv() == 10 );
  REQUIRE( ax.nedges() == 11 );

  REQUIRE( ax.min() == 0 );
  REQUIRE( ax.max() == 1 );

  REQUIRE( ax.lower(0) == -std::numeric_limits<double>::infinity() );
  REQUIRE( ax.upper(0) == 0 );
  REQUIRE( std::abs(ax.lower(4) - 0.3) < 1e-6 );
  REQUIRE( std::abs(ax.upper(4) - 0.4) < 1e-6 );
  REQUIRE( std::abs(ax.lower(10) - 0.9) < 1e-6 );
  REQUIRE( ax.upper(10) == 1 );
  REQUIRE( ax.lower(11) == 1 );
  REQUIRE( ax.upper(11) == std::numeric_limits<double>::infinity() );
  REQUIRE( ax.lower(12) == std::numeric_limits<double>::infinity() );
  REQUIRE( ax.upper(12) == std::numeric_limits<double>::infinity() );

  REQUIRE( ax.find_bin_index(0.25) == 3 );
  REQUIRE( h.find_bin_index(0.25) == 3 );
}

TEST_CASE( "int axis", "[axis]" ) {
  ivanp::hist::uniform_axis<int> ax(4,10,-10);

  REQUIRE( ax.lower(0) == INT_MIN );
  REQUIRE( ax.upper(0) == -10 );
  REQUIRE( ax.lower(2) == -5 );
  REQUIRE( ax.upper(2) == 0 );
  REQUIRE( ax.lower(3) == 0 );
  REQUIRE( ax.upper(3) == 5 );
  REQUIRE( ax.lower(5) == 10 );
  REQUIRE( ax.upper(5) == INT_MAX );
  REQUIRE( ax.lower(6) == INT_MAX );
  REQUIRE( ax.upper(6) == INT_MAX );
}

TEST_CASE( "Nd histogram with variant axes", "[hist]" ) {
  using hist_t = ivanp::hist::histogram<
    int,
    ivanp::hist::axes_spec< std::vector<ivanp::hist::variant_axis<
      ivanp::hist::uniform_axis< double >,
      ivanp::hist::cont_axis< std::vector<double> >
    >> >
  >;
  hist_t h({
    { std::in_place_index_t<0>{}, 9, 10, 100 },
    { std::in_place_index_t<1>{}, std::array<float,4>{1e0,1e1,1e2,1e3} }
  });

  REQUIRE( h.nbins() == 11*5 );

  REQUIRE( h.axis(0).find_bin_index(25) == 2 );
  REQUIRE( h.axis(1).find_bin_index(156) == 3 );
  REQUIRE( h.find_bin_index(25,156) == 2*5+3 );

  REQUIRE( h(56,17) == 1 );
  REQUIRE( h({50,50},10) == 11 );
  REQUIRE( h.fill_at(5*5+2,4) == 15 );
  REQUIRE( h.bin_at(5*5+2) == 15 );
}

TEST_CASE( "generic container constructor", "[hist]" ) {
  using hist_t = ivanp::hist::histogram<>;
  hist_t h(std::make_tuple(
    std::array<float,3>{1,2,3},
    std::list{1,2,3}
  ));

  REQUIRE( h.nbins() == 16 );
}
