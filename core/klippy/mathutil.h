#ifndef MATHUTIL_H
#define MATHUTIL_H
#include <vector>
#include <string>
#include <map>
#include <math.h>

//Perform coordinate descent
double adjusted_height(std::vector<double> pos, std::map<std::string, double> params);
double errorfunc(std::map<std::string, double> params, std::vector<std::vector<double>> positions);
std::map<std::string, double> coordinate_descent(std::map<std::string, double> params, std::vector<std::vector<double>> positions);
std::vector<double> trilateration(std::vector<std::vector<double>> sphere_coords, std::vector<double> radius2);
std::vector<double> matrix_cross(std::vector<double>m1, std::vector<double>m2);
double matrix_dot(std::vector<double> m1, std::vector<double> m2);
double matrix_magsq(std::vector<double> m1);
std::vector<double> matrix_add(std::vector<double> m1, std::vector<double> m2);
std::vector<double> matrix_sub(std::vector<double> m1, std::vector<double> m2);
std::vector<double> matrix_mul(std::vector<double> m1, double s);
#endif