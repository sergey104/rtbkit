/*
 * polygon.h
 *
 *  Created on: 03.11.2016
 *      Author: Alexander Poznyak
 */

#ifndef POLYGON_H_
#define POLYGON_H_

#include "jml/arch/exception.h"
#include "soa/jsoncpp/value.h"
#include <vector>

#include "soa/types/string.h"
#include <iostream>


namespace RTBKIT {

struct Point {
	inline Point(double px, double py) : x(px), y(py) {}
	
	double x;
	double y;
};
	
struct Polygon : public std::vector<Point> {

    Polygon ();
	Polygon(std::initializer_list<Point> il);

    static Polygon createFromJson(const Json::Value & val);

    void fromJson(const Json::Value & val);

    Json::Value toJson() const ;
};

struct PolygonList : public std::vector<Polygon> {

    static PolygonList createFromJson(const Json::Value & val);

    void fromJson(const Json::Value & val);

    Json::Value toJson() const ;
};

struct PolygonsInfo {
	PolygonList include;
	PolygonList exclude;
	
	bool empty() const;

    static PolygonsInfo createFromJson(const Json::Value & val);

    void fromJson(const Json::Value & val);

    Json::Value toJson() const ;
};

}  // namespace RTBKIT


#endif /* LATLONRAD_H_ */
