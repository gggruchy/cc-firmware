#include "gcode_arcs.h"
#include "klippy.h"

enum ARC_PLANE
{
    ARC_PLANE_X_Y = 0,
    ARC_PLANE_X_Z,
    ARC_PLANE_Y_Z
};

enum AXIS
{
    X_AXIS = 0,
    Y_AXIS,
    Z_AXIS,
    E_AXIS
};

ArcSupport::ArcSupport(std::string section_name)
{
    m_mm_per_arc_segment = Printer::GetInstance()->m_pconfig->GetDouble(section_name, "resolution", 1., DBL_MIN, DBL_MAX, 0.0);
    Printer::GetInstance()->m_gcode->register_command("G2", std::bind(&ArcSupport::cmd_G2, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G3", std::bind(&ArcSupport::cmd_G3, this, std::placeholders::_1));

    Printer::GetInstance()->m_gcode->register_command("G17", std::bind(&ArcSupport::cmd_G17, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G18", std::bind(&ArcSupport::cmd_G18, this, std::placeholders::_1));
    Printer::GetInstance()->m_gcode->register_command("G19", std::bind(&ArcSupport::cmd_G19, this, std::placeholders::_1));

    m_plane = ARC_PLANE_X_Y;
}

ArcSupport::~ArcSupport()
{
}


void ArcSupport::cmd_G2(GCodeCommand &gcmd)
{
    cmd_inner(gcmd);
}

void ArcSupport::cmd_G3(GCodeCommand &gcmd)
{
    cmd_inner(gcmd);
}

void ArcSupport::cmd_G17(GCodeCommand &gcmd)
{
    m_plane = ARC_PLANE_X_Y;
}

void ArcSupport::cmd_G18(GCodeCommand &gcmd)
{
    m_plane = ARC_PLANE_X_Z;
}

void ArcSupport::cmd_G19(GCodeCommand &gcmd)
{
    m_plane = ARC_PLANE_Y_Z;
}

void ArcSupport::cmd_inner(GCodeCommand &gcmd)
{
    gcode_move_state_t gcodestatus = Printer::GetInstance()->m_gcode_move->get_status(0.);
    if(!gcodestatus.absolute_coord)
    {
        printf("G2/G3 does not support relative move mode \n");
    }
    std::vector<double> currentPos = gcodestatus.base_position;
    std::vector<double> asTarget;
    double asX = gcmd.get_double("X", currentPos[AXIS_X]);
    double asY = gcmd.get_double("Y", currentPos[AXIS_Y]);
    double asZ = gcmd.get_double("Z", currentPos[AXIS_Z]);
    asTarget.push_back(asX);
    asTarget.push_back(asY);
    asTarget.push_back(asZ);
    if(gcmd.get_double("R", DBL_MIN) != DBL_MIN)
    {
        printf("G2/G3 does not support R moves\n");
    }
    std::vector<double> asPlanar;
    std::vector<int> axes;
    if(m_plane == ARC_PLANE_X_Y)
    {
        asPlanar.push_back(gcmd.get_double("I", 0.));
        asPlanar.push_back(gcmd.get_double("J", 0.));
        axes.push_back(X_AXIS);
        axes.push_back(Y_AXIS);
        axes.push_back(Z_AXIS);
    }
    else if(m_plane == ARC_PLANE_X_Z)
    {
        asPlanar.push_back(gcmd.get_double("I", 0.));
        asPlanar.push_back(gcmd.get_double("K", 0.)); 
        axes.push_back(X_AXIS);
        axes.push_back(Z_AXIS);
        axes.push_back(Y_AXIS);
    }
    else if(m_plane == ARC_PLANE_Y_Z)
    {
        asPlanar.push_back(gcmd.get_double("J", 0.));
        asPlanar.push_back(gcmd.get_double("K", 0.)); 
        axes.push_back(Y_AXIS);
        axes.push_back(Z_AXIS);
        axes.push_back(X_AXIS);
    }
    if(!asPlanar[0] && !asPlanar[1])
    {
        printf("G2/G3 requires IJ, IK or JK parameters\n");
    }
    double asE = gcmd.get_double("E", DBL_MIN);
    double asF = gcmd.get_double("F", DBL_MIN);

    // Build list of linear coordinates to move to
    bool clockwise = (gcmd.get_command() == "G2");
    std::vector<std::vector<double>> coords = this->planArc(currentPos, asTarget, asPlanar, clockwise, axes);
    double e_per_move = 0.;
    double e_base = 0.;
    if(asE != DBL_MIN)
    {
        if(gcodestatus.absolute_extrude)
            e_base = currentPos[AXIS_E];
        e_per_move = (asE - e_base) / (coords.size());
    }

    // Convert coords into G1 commands
    for(auto coord : coords)
    {
        // std::vector<double> g1_params;
        // memset(&g1_params, 0, sizeof(std::vector<double>));
        std::map<std::string, std::string> g1_params;
        g1_params["X"] = std::to_string(coord[0]);
        g1_params["Y"] = std::to_string(coord[1]);
        g1_params["Z"] = std::to_string(coord[2]);
        if(e_per_move)
        {
            g1_params["E"] = std::to_string(e_base + e_per_move);
            if(gcodestatus.absolute_extrude)
            {
                e_base += e_per_move;
            }
        }
        if(asF != DBL_MIN)
        {
            g1_params["F"] = std::to_string(asF);
        }
        
        GCodeCommand g1_gcmd = Printer::GetInstance()->m_gcode->create_gcode_command("G1", "G1", g1_params);
        Printer::GetInstance()->m_gcode_move->cmd_G1(g1_gcmd);
    }
}

// The arc is approximated by generating many small linear segments.
// The length of each segment is configured in MM_PER_ARC_SEGMENT
// Arcs smaller then this value, will be a Line only

std::vector<std::vector<double>> ArcSupport::planArc(std::vector<double> currentPos, std::vector<double> targetPos, std::vector<double> offset, bool clockwise, std::vector<int> axes)
{
    int alpha_axis = axes[0];
    int beta_axis = axes[1];
    int helical_axis = axes[2];
// todo: sometimes produces full circles
    // #define X_AXIS 0
    // #define Y_AXIS 1 
    // #define Z_AXIS 2

// Radius vector from center to current location
    double r_P = -offset[0];
    double r_Q = -offset[1];

// Determine angular travel
    double center_P = currentPos[alpha_axis] - r_P;
    double center_Q = currentPos[beta_axis] - r_Q;
    double rt_Alpha = targetPos[alpha_axis] - center_P;
    double rt_Beta = targetPos[beta_axis] - center_Q;
    double angular_travel = atan2(r_P * rt_Beta - r_Q * rt_Alpha, r_P * rt_Alpha + r_Q * rt_Beta);
    if(angular_travel < 0.)
    {
        angular_travel += 2. * M_PI;
    }
    if(clockwise)
    {
        angular_travel -= 2. * M_PI;
    }
    if(angular_travel == 0. && currentPos[alpha_axis] == targetPos[alpha_axis] && currentPos[beta_axis] == targetPos[beta_axis])
    {
        // # Make a circle if the angular rotation is 0 and the
        // # target is current position
        angular_travel = 2. * M_PI;
    }

    // Determine number of segments
    double linear_travel = targetPos[helical_axis] - currentPos[helical_axis];
    double radius = hypot(r_P, r_Q);
    double flat_mm = radius * angular_travel;
    double mm_of_travel = 0;
    if(linear_travel)
        mm_of_travel = hypot(flat_mm, linear_travel);
    else
        mm_of_travel = fabs(flat_mm);
    double segments = std::max(1.0, floor(mm_of_travel / m_mm_per_arc_segment));

    // Generate coordinates
    std::vector<std::vector<double>> coords;
    double theta_per_segment = angular_travel / segments;
    double linear_per_segment = linear_travel / segments;
    for (int i = 1; i < (int)segments; i++)
    {
        double dist_Helical = i * linear_per_segment;
        double cos_Ti = cos(i * theta_per_segment);
        double sin_Ti = sin(i * theta_per_segment);
        r_P = -offset[0] * cos_Ti + offset[1] * sin_Ti;
        r_Q = -offset[0] * sin_Ti - offset[1] * cos_Ti;

        // Coord doesn't support index assignment, create list
        std::vector<double> c = {center_P + r_P, center_Q + r_Q, currentPos[helical_axis] + dist_Helical};
        coords.push_back(c);
    }
    coords.push_back(targetPos);
    return coords;
}