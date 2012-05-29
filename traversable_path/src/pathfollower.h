#ifndef PATHFOLLOWER_H
#define PATHFOLLOWER_H

#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <motion_control/MotionAction.h>
#include <actionlib/client/simple_action_client.h>
#include <geometry_msgs/Point32.h>
#include <visualization_msgs/Marker.h>
#include <nav_msgs/OccupancyGrid.h>
#include <pcl_ros/point_cloud.h>

#define EIGEN_USE_NEW_STDVECTOR
#include <Eigen/Dense>
#include <Eigen/StdVector>

#include "point_types.h"
#include "exceptions.h"


/**
 * @brief Simple wrapper for linear functions f(x) = mx+c.
 * @author Felix Widmaier
 * @version 1.0
 */
class LinearFunction
{
public:
    float m;
    float c;

    LinearFunction(Eigen::Vector2f coeff)
    {
        m = coeff[0];
        c = coeff[1];
    }

    //! Calculate f(x).
    float operator()(float x) const
    {
        return m * x + c;
    }
};

/**
 * @brief Main class of the follow_path node.
 *
 * The class subscribes for the terrain classification of the node classify_path, determines goal points for navigation
 * and commands the robot to drive to this goals (using the motion_control package).
 *
 * @author Felix Widmaier
 * @version $Id$
 */
class PathFollower
{
public:
    typedef std::vector<Eigen::Vector2f, Eigen::aligned_allocator<Eigen::Vector2f> > vectorVector2f;

    PathFollower();

private:
    struct RobotPose {
        //! Position as point.
        Eigen::Vector2f position;
        //! Orientation as vector.
        Eigen::Vector2f orientation;
    };

    struct Line {
        Eigen::Vector2f point;
        Eigen::Vector2f direction;
        Eigen::Vector2f normal;
        float soundness;
    };

    ros::NodeHandle node_handle_;
    //! Subscriber for the terrain classification of the node classify_path.
    ros::Subscriber subscribe_scan_classification_;
    //! Subscriber for the traversability map.
    ros::Subscriber subscribe_map_;
    //! Publisher for rvis markers.
    ros::Publisher publish_rviz_marker_;
    ros::Publisher publish_goal_;
    //! Listener for tf data.
    tf::TransformListener tf_listener_;
    //! Sends commands to motion_control
    actionlib::SimpleActionClient<motion_control::MotionAction> motion_control_action_client_;

    //! The current goal.
    geometry_msgs::Point current_goal_;

    nav_msgs::OccupancyGridConstPtr map_;

    //! Pose of the robot
    RobotPose robot_pose_;

    //! Filtered path angle.
    float path_angle_;


    /**
     * @brief Chooses traversable segments on the path and determines a goal.
     *
     * This is the callback function for the terrain classification messages. It chooses the traversable segment, that
     * lies most in the mid of the scan (and thus should be straight in front of the robot) and determines goal points
     * to drive in the middle of the path.
     * The goals are send to motion_control so this method does the driving.
     *
     * @param The terrain classification of the current laser scan.
     */
    void scan_classification_callback(const pcl::PointCloud<PointXYZRGBT>::ConstPtr& scan_classification);

    //! Callback for the traversability map subscriber.
    void mapCallback(const nav_msgs::OccupancyGridConstPtr &msg);

    /**
     * @brief Sends a marker to rviz which visualizes the goal as an arrow.
     * The arrow starts at the goal position and points in the direction of the goal orientation.
     * @param goal The goal pose with position and orientation of the goal.
     */
    void publishGoalMarker(const geometry_msgs::PoseStamped &goal) const;

    /**
     * @brief Sends a marker to rviz which visualizes the choosen traversable segment.
     * The traversable segment will be displayed as a green line that connects the left and the right border of the
     * segment.
     * @param a Left border of the traversable segment.
     * @param b Right border of the traversable segment.
     * @param header Header information of the points a and b.
     */
    void publishTraversaleLineMarker(PointXYZRGBT a, PointXYZRGBT b, std_msgs::Header header) const;

    /**
     * @brief Sends a line marker to rviz.
     *
     * Creates a line marker from point p1 to point p2 (x,y-coords, z = 0) and publishes it.
     *
     * @param p1 Start point of the line.
     * @param p2 End point of the line.
     * @param id Id of the line. A published line will replace older lines with the same id.
     * @param color Color of the line.
     */
    void publishLineMarker(Eigen::Vector2f p1, Eigen::Vector2f p2, int id, std_msgs::ColorRGBA color) const;

    /**
     * @brief Sends a line marker to rviz.
     *
     * This method helps to print lines in rviz. It takes the coefficients a,b that represent a line (y = a*x + b) and
     * visualize it on the interval [min_x; max_x]. The z-value is set to 0.
     *
     * @param coefficients Coeeficients a,b of the line (y = a*x + b).
     * @param min_x Minimum x value. Start line at this value.
     * @param max_x Maximum x value. End line at this value.
     * @param id Id of the line. A published line will replace older lines with the same id.
     * @param color Color of the line.
     */
    void publishLineMarker(Eigen::Vector2f coefficients, int min_x, int max_x, int id, std_msgs::ColorRGBA color) const;

    /**
     * @brief Get the angle of the path direction (related to frame /map).
     *
     * @return Angle of the path direction as it can be used by motion_control.
     */
    float getPathDirectionAngle();

    /**
     * @brief Get the angle of the path direction (related to frame /map), using edge lines.
     * @deprecated Use getPathDirectionAngle() instead.
     * @return Angle of the path direction as it can be used by motion_control.
     */
    float getPathDirectionAngleUsingEdges() const;

    /**
     * @brief Find some points of the egdes of the current path.
     *
     * Searchs for points of the edges of the path in front of the robot. Using this points an function can be fitted
     * to them which will provied "cleaner" edges.
     *
     * @param out_points_left Output parameter.
     * @return True on success, False if some failure occures (e.g. transform failes).
     */
    bool findPathEdgePoints(vectorVector2f *out_points_left, vectorVector2f *out_points_right) const;

    /**
     * @brief Find some points of the moddle of the curretn path.
     *
     * Searches for points of the middle of the path. This points can then be used to fit a "path middle line" to them.
     * @param out Output parameter. The points will be stored to this vector.
     * @return True on success, False if some failure occures (e.g. transform failes).
     */
    bool findPathMiddlePoints(vectorVector2f *out) const;

    /**
     * @brief Transform coordinates of a point to the map cell.
     * @param point Some point. Has to be in the same frame than the map (which is '/map').
     * @return Index of the map cell.
     * @throws traversable_path::TransformMapException if point lies outside of the map.
     */
    size_t transformToMap(Eigen::Vector2f point) const;

    /**
     * @brief Fit a linear function (y = a*x + b) to the given points (using least squares).
     *
     * @param points A list of points.
     * @return The coefficients a and b of the resulting function.
     */
    static Eigen::Vector2f fitLinear(const vectorVector2f &points);

    //! Fit line in hesse normal form (no problems with points parallel to y-axis).
    static void fitLinearHesseNormalForm(const vectorVector2f &points, Eigen::Vector2f *n, float *d);

//    /* static */ void fitFoobar(const PathFollower::vectorVector2f &points, Eigen::Hyperplane<float, 2> *result,
//                          float *soundness = 0);

     static void fitFoobar(const PathFollower::vectorVector2f &points, Line *result);

    /**
     * @brief Refresh the current position and orientation of the robot.
     *
     * Gets the current position and orientation of the robot and stores it to robot_pose_.
     * Use robot_pose_ to access the pose, after refreshing it with this method.
     *
     * @return False if some failure occurs, otherwise true.
     */
    bool refreshRobotPose();
};

#endif // PATHFOLLOWER_H
