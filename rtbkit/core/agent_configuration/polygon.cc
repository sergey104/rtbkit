/*
 * polygon.cc
 *
 *  Created on: 03.11.2016
 *      Author: Alexander Poznyak
 */
#include <boost/lexical_cast.hpp>
#include <rtbkit/core/agent_configuration/polygon.h>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm/remove_if.hpp>
#include <stdlib.h>
#include "jml/arch/exception.h"

using namespace RTBKIT;


Polygon::Polygon() : std::vector<Point>() { }
Polygon::Polygon(std::initializer_list<Point> il) : std::vector<Point>(il) {}

Polygon
Polygon::createFromJson(const Json::Value & val) {

	Polygon pg;
	auto jt = val.begin();
	for (auto jend = val.end();  jt != jend;  ++jt) {
		if (jt->isMember("lat") && jt->isMember("long")) {
			Point p ((*jt)["long"].asDouble(), (*jt)["lat"].asDouble());
			pg.push_back(p);
		} else {
			throw ML::Exception("Error parsing LatLonPair. It should have lat and long."
                "There was given: ", jt->asCString());
		}
	}

    return pg;
}

void Polygon::fromJson(const Json::Value & val) {
    *this = createFromJson(val);
}

Json::Value Polygon::toJson() const {
    Json::Value result;
	
	for (unsigned idx = 0;  idx < this->size();  ++idx) {
		Json::Value item;
	
		item["long"] = (*this)[idx].x;
		item["lat"] = (*this)[idx].y;
	
		result[idx] = item;
	}
    return result;
}

PolygonList
PolygonList::createFromJson(const Json::Value & val) {

    PolygonList pgl;

    for (auto jt = val.begin(), jend = val.end();  jt != jend;  ++jt) {
			Polygon pg = Polygon::createFromJson(*jt);
			pgl.push_back(pg);
    }

    return pgl;
}

void PolygonList::fromJson(const Json::Value & val) {
    *this = createFromJson(val);
}

Json::Value PolygonList::toJson() const {
    Json::Value result;
    for (unsigned i = 0;  i < this->size();  ++i)
        result[i] = (*this)[i].toJson();
    return result;
}

bool 
PolygonsInfo::empty() const { 
	return include.empty() && exclude.empty(); 
}

PolygonsInfo 
PolygonsInfo::createFromJson(const Json::Value & val) {
	PolygonsInfo pgi;
	
	if(val.isMember("include")) {
		pgi.include.fromJson(val["include"]);
	}
	if(val.isMember("exclude")) {
		pgi.exclude.fromJson(val["exclude"]);
	}
	
	return pgi;
}

void 
PolygonsInfo::fromJson(const Json::Value & val) {
	*this = createFromJson(val);
}

Json::Value 
PolygonsInfo::toJson() const {
	Json::Value result;
	
	if(!include.empty())
		result["include"] = include.toJson();
	if(!exclude.empty())
		result["exclude"] = exclude.toJson();
	
	return result;
}

