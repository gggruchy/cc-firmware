#include "mathutil.h"

//######################################################################
//# Coordinate descent
//######################################################################

//Perform coordinate descent
double adjusted_height(std::vector<double> pos, std::map<std::string, double> params)
{
    double ret = pos[2] - pos[0] * params["x_adjust"] - pos[1] * params["y_adjust"] - params["z_adjust"];
    return ret;
}

double errorfunc(std::map<std::string, double> params, std::vector<std::vector<double>> positions)
{
    double total_error = 0.;
    for (auto pos : positions)
    {
        total_error += pow(adjusted_height(pos, params), 2);
    }
    return total_error;
}

// Helper code that implements coordinate descent
std::map<std::string, double> coordinate_descent(std::map<std::string, double> params, std::vector<std::vector<double>> positions)
{
    // Define potential changes
    std::map<std::string, double> dp = params;
    std::map<std::string, double>::iterator it = dp.begin();
    while (it != dp.end())
    {
        it->second = 1.;
        it++;
    }
    // Calculate the error
    double best_err = errorfunc(params, positions);
    // logging.info("Coordinate descent initial error: %s", best_err)

    double threshold = 0.00001;
    int rounds = 0;
    
    while (rounds < 10000)
    {
        double dp_sum = 0;
        for (auto d : dp)
        {
            dp_sum += d.second;
        }
        if (dp_sum <= threshold)
        {
            break;
        }
        rounds += 1;
        for (auto param : params)
        {
            double orig = param.second;
            params[param.first] = orig + dp[param.first];
            double err = errorfunc(params, positions);
            if (err < best_err)
            {
                // There was some improvement
                best_err = err;
                dp[param.first] *= 1.1;
                continue;
            }
            params[param.first] = orig - dp[param.first];
            err = errorfunc(params, positions);
            if (err < best_err)
            {
                // There was some improvement
                best_err = err;
                dp[param.first] *= 1.1;
                continue;
            }
            params[param.first] = orig;
            dp[param.first] *= 0.9;
        }
    }
    // logging.info("Coordinate descent best_err: %s  rounds: %d", best_err, rounds)
    return params;
}


//#####################################################################
// Trilateration
//#####################################################################

// Trilateration finds the intersection of three spheres.  See the
// wikipedia article for the details of the algorithm.
std::vector<double> trilateration(std::vector<std::vector<double>> sphere_coords, std::vector<double> radius2)
{
    std::vector<double> sphere_coord1 = sphere_coords[0];
    std::vector<double> sphere_coord2 = sphere_coords[1];
    std::vector<double> sphere_coord3 = sphere_coords[2];
    std::vector<double>s21 = matrix_sub(sphere_coord2, sphere_coord1);
    std::vector<double>s31 = matrix_sub(sphere_coord3, sphere_coord1);

    double d = sqrt(matrix_magsq(s21));
    std::vector<double> ex = matrix_mul(s21, 1. / d);
    double i = matrix_dot(ex, s31);
    std::vector<double> vect_ey = matrix_sub(s31, matrix_mul(ex, i));
    std::vector<double> ey = matrix_mul(vect_ey, 1. / sqrt(matrix_magsq(vect_ey)));
    std::vector<double> ez = matrix_cross(ex, ey);
    double j = matrix_dot(ey, s31);

    double x = (radius2[0] - radius2[1] + (d, 2)) / (2. * d);
    double y = (radius2[0] - radius2[2] - pow(x, 2) + pow((x-i), 2) + pow(j, 2)) / (2. * j);
    double z = 0 - sqrt(radius2[0] - pow(x, 2) - pow(y, 2));

    std::vector<double> ex_x = matrix_mul(ex, x);
    std::vector<double> ey_y = matrix_mul(ey, y);
    std::vector<double> ez_z = matrix_mul(ez, z);
    return matrix_add(sphere_coord1, matrix_add(ex_x, matrix_add(ey_y, ez_z)));
}
    


//#####################################################################
// Matrix helper functions for 3x1 matrices
//#####################################################################

std::vector<double> matrix_cross(std::vector<double>m1, std::vector<double>m2)
{
    std::vector<double> ret = {m1[1] * m2[2] - m1[2] * m2[1], 
                                m1[2] * m2[0] - m1[0] * m2[2], 
                                m1[0] * m2[1] - m1[1] * m2[0]};
    return ret;
}
    

double matrix_dot(std::vector<double> m1, std::vector<double> m2)
{
    return m1[0] * m2[0] + m1[1] * m2[1] + m1[2] * m2[2];

}
    
double matrix_magsq(std::vector<double> m1)
{
    return pow(m1[0], 2) + pow(m1[1], 2) + pow(m1[2], 2);
}
    

std::vector<double> matrix_add(std::vector<double> m1, std::vector<double> m2)
{
    std::vector<double> ret = {m1[0] + m2[0], m1[1] + m2[1], m1[2] + m2[2]};
    return ret;
}
    
std::vector<double> matrix_sub(std::vector<double> m1, std::vector<double> m2)
{
    std::vector<double> ret = {m1[0] - m2[0], m1[1] - m2[1], m1[2] - m2[2]};
    return ret;
}
    
std::vector<double> matrix_mul(std::vector<double> m1, double s)
{
    std::vector<double> ret = {m1[0]*s, m1[1]*s, m1[2]*s};
    return ret;
}
    
