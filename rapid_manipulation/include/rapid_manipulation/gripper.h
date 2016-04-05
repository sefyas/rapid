// A class for opening and closing the grippers.
//
// Sample usage:
//  rapid::pr2::Gripper right_gripper(
//    Gripper::RIGHT_GRIPPER);
//  right_gripper.Open();
//  right_gripper.Close();
//
// Notes:
// 1. The gripper does not work quite right in Gazebo
// 2. Return value is based on whether the gripper reached the target position.
//    So, if the gripper is closing around an object, Close() will typically
//    return false because the object stalls the gripper before it reaches
//    the goal position.

#ifndef _RAPID_MANIPULATION_GRIPPER_H_
#define _RAPID_MANIPULATION_GRIPPER_H_

#include "actionlib/client/simple_action_client.h"
#include "gmock/gmock.h"
#include "pr2_controllers_msgs/Pr2GripperCommandAction.h"
#include "ros/ros.h"
#include "tf/transform_listener.h"

namespace rapid {
namespace manipulation {
typedef actionlib::SimpleActionClient<
    pr2_controllers_msgs::Pr2GripperCommandAction> GripperClient;

class GripperInterface {
 public:
  virtual ~GripperInterface() {}
  virtual bool SetPosition(double position, double effort = -1) const = 0;
  virtual double GetPosition() const = 0;
  virtual bool IsOpen() const = 0;
  virtual bool Open(double effort = -1.0) const = 0;
  virtual bool Close(double effort = -1.0) const = 0;
};

class Gripper : public GripperInterface {
 private:
  GripperClient* gripper_client_;
  tf::TransformListener transform_listener_;
  const int gripper_id_;

  static const double OPEN_THRESHOLD;  // Gripper open threshold

 public:
  static const double OPEN;    // Canonical "open" position.
  static const double CLOSED;  // Canonical "closed" position.

  // Gripper ids
  static const int LEFT_GRIPPER = 0;
  static const int RIGHT_GRIPPER = 1;

  // Gripper action topics
  static const std::string LEFT_GRIPPER_ACTION;
  static const std::string RIGHT_GRIPPER_ACTION;

  // Constructor that takes the gripper id.
  // gripper_id: Gripper::LEFT_GRIPPER or Gripper::RIGHT_GRIPPER
  Gripper(const int gripper_id);

  ~Gripper();

  // Gets the gripper to the given position.
  // position - how wide to open or close the gripper
  // effort - now much force to exert, negative is full force
  bool SetPosition(double position, double effort = -1.0) const;

  // Gets the gripper's current position. Note: may not agree with "SetPosition"
  double GetPosition() const;

  // Returns whether the gripper is open or not.
  bool IsOpen() const;

  // Opens the gripper. Returns true if the gripper opened successfully, false
  // otherwise.
  // effort - defaults to -1.0, to open with unlimited effort
  bool Open(double effort = -1.0) const;

  // Closes the gripper. Returns true if the gripper opened successfully, false
  // otherwise.
  // effort - defaults to 50.0 to close gently
  bool Close(double effort = -1.0) const;
};

class MockGripper : public GripperInterface {
 public:
  MOCK_CONST_METHOD2(SetPosition, bool(double position, double effort));
  MOCK_CONST_METHOD0(GetPosition, double());
  MOCK_CONST_METHOD0(IsOpen, bool());
  MOCK_CONST_METHOD1(Open, bool(double effort));
  MOCK_CONST_METHOD1(Close, bool(double effort));
};
}  // namespace manipulation
}  // namespace rapid

#endif  // _RAPID_MANIPULATION_GRIPPER_H_
