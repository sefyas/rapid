#include <iostream>
#include <string>

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
#include "ros/ros.h"
#include "sensor_msgs/PointCloud2.h"
#include "tf/transform_listener.h"
#include "visualization_msgs/Marker.h"

using boost::shared_ptr;
using rapid::manipulation::ArmInterface;
using rapid::manipulation::MoveItArm;
using rapid::manipulation::GripperInterface;
using rapid::manipulation::Gripper;
using rapid::manipulation::PickError;
using rapid::perception::Scene;

int main(int argc, char** argv) {
  ros::init(argc, argv, "pick_place");
  ros::NodeHandle nh;
  ros::AsyncSpinner spinner(2);
  spinner.start();

  // Read point cloud.
  pcl::PointCloud<pcl::PointXYZRGB> cloud;
  tf::TransformListener tf_listener;
  shared_ptr<moveit::planning_interface::MoveGroup> arm_group(
      new moveit::planning_interface::MoveGroup("right_arm"));

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

  Scene scene;
  scene.set_cloud(ws_cloud);
  scene.Parse();
  scene.Visualize();

  shared_ptr<ArmInterface> right_arm(new MoveItArm(arm_group));
  shared_ptr<GripperInterface> right_gripper(
      new Gripper(Gripper::RIGHT_GRIPPER));
  rapid::manipulation::Picker picker(right_arm, right_gripper);
  picker.UpdatePlanningScene(scene);
  ROS_INFO("Updated planning scene with %ld objects",
           scene.GetPrimarySurface()->objects().size());

  PickError error = picker.Pick("object_0", "table");
  ROS_INFO("Picker returned %s", error.error().c_str());

  spinner.stop();
  return 0;
}