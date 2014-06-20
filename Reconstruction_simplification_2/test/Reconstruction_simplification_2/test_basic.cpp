// test_basic.cpp

//----------------------------------------------------------
// Test the cgal environment for Reconstruction_simplification_2
//----------------------------------------------------------

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include "Reconstruction_simplification_2.h"

#include <fstream>

#include<iostream>
#include <iterator>
#include <utility>      // std::pair

#include <CGAL/property_map.h>
#include <CGAL/value_type_traits.h>

#include <QtOpenGL>

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef K::Point_2                                          Point;
typedef K::FT                                         		FT;

typedef std::pair<Point, FT> PointMassPair;
typedef std::list<PointMassPair> PointMassList;
typedef PointMassList::const_iterator InputIterator;
typedef CGAL::value_type_traits<InputIterator>::type MassPoint;
typedef CGAL::First_of_pair_property_map <PointMassPair> PointPMap;
typedef CGAL::Second_of_pair_property_map <PointMassPair> MassPMap;


PointMassList* load_xy_file(const QString& fileName);
PointMassList* simple_point_set();


int main ()
{

	//use the stair example for testing
	PointMassList points = *(load_xy_file("data/stair-noise00.xy"));

    PointPMap point_pmap;
    MassPMap  mass_pmap;

    MassPoint mp;

    CGAL::Reconstruction_simplification_2<K, InputIterator, PointPMap, MassPMap>
    	rs2(points.begin(), points.end(), point_pmap, mass_pmap);

    rs2.initialize();

    rs2.reconstruct(100); //100 steps

    rs2.print_stats_debug();

    rs2.extract_solid_eges();
}



PointMassList* simple_point_set() {

	PointMassList *points = new PointMassList();

    points->push_back(std::make_pair(Point(0.1,0.1), 1));
    points->push_back(std::make_pair(Point(0.4,0.1), 1));
    points->push_back(std::make_pair(Point(0.6,0.1), 1));
    points->push_back(std::make_pair(Point(0.9,0.1), 1));
    points->push_back(std::make_pair(Point(0.9,0.4), 1));
    points->push_back(std::make_pair(Point(0.9,0.6), 1));
    points->push_back(std::make_pair(Point(0.9,0.9), 1));
    points->push_back(std::make_pair(Point(0.6,0.9), 1));
    points->push_back(std::make_pair(Point(0.4,0.9), 1));
    points->push_back(std::make_pair(Point(0.1,0.9), 1));
    points->push_back(std::make_pair(Point(0.1,0.6), 1));
    points->push_back(std::make_pair(Point(0.1,0.4), 1));

    return points;

}


PointMassList* load_xy_file(const QString& fileName)
{
	PointMassList *points = new PointMassList();
       std::ifstream ifs(qPrintable(fileName));
       std::cerr << "read xy...";
       Point point;
       unsigned int nb = 0;
       while (ifs >> point)
       {
    	   points->push_back(std::make_pair(point, 1));
       }
       std::cerr << "done (" << nb << " points)" << std::endl;
       ifs.close();

       return points;

}
