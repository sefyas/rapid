#include <iostream>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"
#include "moveit/move_group_interface/move_group.h"
#include "moveit/planning_scene_interface/planning_scene_interface.h"
#include "pcl/point_cloud.h"
#include "pcl/point_types.h"
#include "pcl_conversions/pcl_conversions.h"
#include "pcl_ros/transforms.h"
#include "rapid_manipulation/arm.h"
#include "rapid_manipulation/gripper.h"
#include "rapid_manipulation/pick_place.h"
#include "rapid_perception/pr2.h"
#include "rapid_perception/rgbd.hpp"
#include "rapid_perception/scene.h"
#include "rapid_pr2/pr2.h"
#include "ros/ros.h"
#include "sensor_msgs/PointCloud2.h"
#include "tf/transform_listener.h"
#include "visualization_msgs/Marker.h"

using boost::shared_ptr;
using rapid::manipulation::ArmInterface;
using rapid::manipulation::MoveItArm;
using rapid::manipulation::GripperInterface;
using rapid::manipulation::Gripper;
using rapid::perception::Object;
using rapid::perception::Scene;
using rapid::perception::ScenePrimitive;
using rapid::pr2::Pr2;
using std::string;
using std::vector;

int main(int argc, char** argv) {
  ros::init(argc, argv, "pick_place");
  ros::NodeHandle nh;
  ros::AsyncSpinner spinner(2);
  spinner.start();
  tf::TransformListener tf_listener;
  shared_ptr<Pr2> pr2 = rapid::pr2::BuildReal();
  rapid::manipulation::Picker picker(pr2->right_arm, pr2->right_gripper);
  rapid::manipulation::Placer placer(pr2->right_arm, pr2->right_gripper);

  bool first = true;
  while (true) {
    if (!first) {
      string response;
      std::cout << "Regrasp (y/n)? ";
      std::cin >> response;
      if (response != "y") {
        break;
      }
    }
    first = false;

    // Read point cloud.
    pr2->tuck_arms.DeployArms();
    pcl::PointCloud<pcl::PointXYZRGB> cloud;
    sensor_msgs::PointCloud2ConstPtr msg =
        ros::topic::waitForMessage<sensor_msgs::PointCloud2>("/cloud_in");
    sensor_msgs::PointCloud2 transformed;
    pcl_ros::transformPointCloud("/base_footprint", *msg, transformed,
                                 tf_listener);
    pcl::fromROSMsg(transformed, cloud);

    // Crop point cloud
    visualization_msgs::Marker ws;
    rapid::perception::pr2::GetManipulationWorkspace(&ws);
    pcl::PointCloud<pcl::PointXYZRGB> ws_cloud;
    rapid::perception::CropWorkspace(cloud, ws, &ws_cloud);

    // Parse scene
    Scene scene;
    scene.set_cloud(ws_cloud);
    scene.Parse();
    scene.Visualize();
    const vector<Object>* objects = scene.GetPrimarySurface()->objects();
    if (objects->size() == 0) {
      ROS_ERROR("No objects found.");
      pr2->tuck_arms.DeployArms();
      continue;
    } else {
      ROS_INFO("Found %ld objects", objects->size());
    }

    // Pick up first object and place it somewhere else.
    Object first_obj = (*objects)[0];
    ScenePrimitive obj(first_obj.pose(), first_obj.scale(), first_obj.name());
    ROS_INFO("Attempting to pick up %s", first_obj.name().c_str());
    bool success = picker.Pick(obj);
    if (!success) {
      pr2->right_gripper.Open();
      pr2->tuck_arms.DeployArms();
      continue;
    }

    success = placer.Place(obj, *scene.GetPrimarySurface());
    if (!success) {
      pr2->right_gripper.Open();
      pr2->tuck_arms.DeployArms();
      ROS_ERROR("Place failed.");
      continue;
    } else {
      ROS_INFO("Place succeeded.");
    }
  }
  return 0;
}