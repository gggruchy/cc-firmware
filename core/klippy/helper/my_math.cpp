#include "my_math.h"
 
 
std::vector<double> rfftfreq(int n, double d)		//产生1025长的 0-1601的均匀序列
{
    std::vector<double> ret;
    if(n%2 == 0)
    {
        for(int i = 0; i <= (n/2); i++)
        {
            ret.push_back(i / (n * d));
        }
    }
    else
    {
        for(int i = 0; i <= (n-1)/2; i++)
        {
            ret.push_back(i / (n * d));
        }
    }
    return ret;
}


int bit_length(int n)
{
    int ret = 0;
    while(n != 0)
    {
        n /= 2;
        ret += 1;
    }
    return ret;
}
#define Vec2d std::pair<double, double>

std::string move_to(const Vec2d &point)
{
	std::stringstream gcode;
	gcode << "G0 X" << point.first << " Y" << point.second;
	return gcode.str();
}

std::string extrude_to_xy(const Vec2d &point, double dE)
{
	std::stringstream gcode;
	gcode << "G0 X" << point.first << " Y" << point.second << " E" << dE;
	return gcode.str();
}

void draw_digital(std::vector<std::string> &script, double startx, double starty, char c)
{
	std::stringstream gcode;
	const double lw = 0.48;
	// Flow line_flow = Flow(lw, 0.2, mp_gcodegen->config().nozzle_diameter.get_at(0));
	const double len = 2;
	const double gap = lw / 2.0;
	const double e = 0.111416 / 2.40528; // filament_mm/extrusion_mm

	//  0-------1 
	//  |       |
	//  3-------2
	//  |       |
	//  4-------5

	const Vec2d p0(startx, starty);
	const Vec2d p1(startx + len, starty);
	const Vec2d p2(startx + len, starty - len);
	const Vec2d p3(startx, starty - len);
	const Vec2d p4(startx, starty - len * 2);
	const Vec2d p5(startx + len, starty - len * 2);

	switch (c)
	{
	case '0':
		script.push_back(move_to(p0));
		script.push_back(extrude_to_xy(p1, e * len));
		script.push_back(extrude_to_xy(p5, e * len * 2));
		script.push_back(extrude_to_xy(p4, e * len));
		script.push_back(extrude_to_xy(p0, e * len * 2));
		break;
	case '1':
		script.push_back(move_to(p1));
		script.push_back(extrude_to_xy(p5, e * len * 2));
		break;
	case '2':
		script.push_back(move_to(p0));
		script.push_back(extrude_to_xy(p1, e * len));
		script.push_back(extrude_to_xy(p2, e * len));
		script.push_back(extrude_to_xy(p3, e * len));
		script.push_back(extrude_to_xy(p4, e * len));
		script.push_back(extrude_to_xy(p5, e * len));
		break;
	case '3':
		script.push_back(move_to(p0));
		script.push_back(extrude_to_xy(p1, e * len));
		script.push_back(extrude_to_xy(p5, e * len * 2));
		script.push_back(extrude_to_xy(p4, e * len));
		script.push_back(move_to(p2));
		script.push_back(extrude_to_xy(p3, e * len));
		break;
	case '4':
		script.push_back(move_to(p0));
		script.push_back(extrude_to_xy(p3, e * len));
		script.push_back(extrude_to_xy(p2, e * len));
		script.push_back(move_to(p1));
		script.push_back(extrude_to_xy(p5, e * len * 2));
		break;
	case '5':
		script.push_back(move_to(p1));
		script.push_back(extrude_to_xy(p0, e * len));
		script.push_back(extrude_to_xy(p3, e * len));
		script.push_back(extrude_to_xy(p2, e * len));
		script.push_back(extrude_to_xy(p5, e * len));
		script.push_back(extrude_to_xy(p4, e * len));
		break;
	case '6':
		script.push_back(move_to(p1));
		script.push_back(extrude_to_xy(p0, e * len));
		script.push_back(extrude_to_xy(p4, e * len * 2));
		script.push_back(extrude_to_xy(p5, e * len));
		script.push_back(extrude_to_xy(p2, e * len));
		script.push_back(extrude_to_xy(p3, e * len));
		break;
	case '7':
		script.push_back(move_to(p0));
		script.push_back(extrude_to_xy(p1, e * len));
		script.push_back(extrude_to_xy(p5, e * len * 2));
		break;
	case '8':
		script.push_back(move_to(p2));
		script.push_back(extrude_to_xy(p3, e * len));
		script.push_back(extrude_to_xy(p4, e * len));
		script.push_back(extrude_to_xy(p5, e * len));
		script.push_back(extrude_to_xy(p1, e * len * 2));
		script.push_back(extrude_to_xy(p0, e * len));
		script.push_back(extrude_to_xy(p3, e * len));
		break;
	case '9':
		script.push_back(move_to(p5));
		script.push_back(extrude_to_xy(p1, e * len * 2));
		script.push_back(extrude_to_xy(p0, e * len));
		script.push_back(extrude_to_xy(p3, e * len));
		script.push_back(extrude_to_xy(p2, e * len));
		break;
	case '.':
		// script.push_back(move_to(p4 + Vec2d(len / 2, 0)));
		// script.push_back(extrude_to_xy(p4 + Vec2d(len / 2, len / 2), e * len));
		break;
	default:
		break;
	}
}
