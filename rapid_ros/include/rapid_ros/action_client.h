#ifndef _RAPID_ROS_ACTION_CLIENT_H_
#define _RAPID_ROS_ACTION_CLIENT_H_

#include <string>

#include "actionlib/action_definition.h"
#include "actionlib/client/simple_action_client.h"
#include "ros/ros.h"

namespace rapid_ros {
// DECLARATIONS ----------------------------------------------------------------
// Interface wrapper for SimpleActionClient.
// Supports only a subset of SimpleActionClient functionality, more will be
// added as needed.
template <class ActionSpec>
class ActionClientInterface {
 private:
  ACTION_DEFINITION(ActionSpec)

 public:
  virtual ~ActionClientInterface() {}
  virtual ResultConstPtr getResult() = 0;
  virtual void sendGoal(const Goal& goal) = 0;
  virtual bool waitForResult(
      const ::ros::Duration& timeout = ::ros::Duration(0, 0)) = 0;
  virtual bool waitForServer(
      const ::ros::Duration& timeout = ::ros::Duration(0, 0)) = 0;
};

// Implements a SimpleActionClient by forwarding calls.
template <class ActionSpec>
class ActionClient : public ActionClientInterface<ActionSpec> {
 private:
  ACTION_DEFINITION(ActionSpec)
  actionlib::SimpleActionClient<ActionSpec> client_;

 public:
  ActionClient(const std::string& name, bool spin_thread = true);
  ResultConstPtr getResult();
  void sendGoal(const Goal& goal);
  bool waitForResult(const ::ros::Duration& timeout = ::ros::Duration(0, 0));
  bool waitForServer(const ::ros::Duration& timeout = ::ros::Duration(0, 0));
};

// A mock action client.
//
// One limitation of this mock is that it will work in single-threaded
// applications. Real SimpleActionClients don't necessarily work in
// single-threaded applications. It may be moot if spin_thread=true in the
// constructor, though, it's not well understood.
template <class ActionSpec>
class MockActionClient : public ActionClientInterface<ActionSpec> {
 private:
  ACTION_DEFINITION(ActionSpec)
  Goal last_goal_;
  Result result_;
  ::ros::Duration result_delay_;
  ::ros::Duration server_delay_;

 public:
  // Mocked methods
  MockActionClient();
  ResultConstPtr getResult();
  void sendGoal(const Goal& goal);
  bool waitForResult(const ::ros::Duration& timeout = ::ros::Duration(0, 0));
  bool waitForServer(const ::ros::Duration& timeout = ::ros::Duration(0, 0));

  // Mock helpers
  // Get the last goal that was sent.
  Goal last_goal() const { return last_goal_; }
  // Set the result to be returned by waitForResult.
  void set_result(const Result& result) { result_ = result; }
  // Set the amount of time to simulate a delay while waiting for the result.
  void set_result_delay(const ::ros::Duration& delay) { result_delay_ = delay; }
  // Set the amount of time to simulate a delay while waiting for the server.
  void set_server_delay(const ::ros::Duration& delay) { server_delay_ = delay; }
};

// DEFINITIONS -----------------------------------------------------------------
// ActionClient definitions
template <class ActionSpec>
ActionClient<ActionSpec>::ActionClient(const std::string& name,
                                       bool spin_thread)
    : client_(name, spin_thread) {}

template <class ActionSpec>
typename ActionClient<ActionSpec>::ResultConstPtr
ActionClient<ActionSpec>::getResult() {
  return client_.getResult();
}

template <class ActionSpec>
void ActionClient<ActionSpec>::sendGoal(const Goal& goal) {
  client_.sendGoal(goal);
}

template <class ActionSpec>
bool ActionClient<ActionSpec>::waitForResult(const ::ros::Duration& timeout) {
  return client_.waitForResult(timeout);
}

template <class ActionSpec>
bool ActionClient<ActionSpec>::waitForServer(const ::ros::Duration& timeout) {
  return client_.waitForServer(timeout);
}

// MockActionClient definitions
template <class ActionSpec>
MockActionClient<ActionSpec>::MockActionClient()
    : last_goal_(), result_(), result_delay_(0), server_delay_(0) {}

template <class ActionSpec>
typename MockActionClient<ActionSpec>::ResultConstPtr
MockActionClient<ActionSpec>::getResult() {
  return ResultConstPtr(new Result(result_));
}

template <class ActionSpec>
void MockActionClient<ActionSpec>::sendGoal(const Goal& goal) {
  last_goal_ = goal;
}

template <class ActionSpec>
bool MockActionClient<ActionSpec>::waitForResult(
    const ::ros::Duration& timeout) {
  if (timeout > result_delay_) {
    return true;
  } else {
    if (timeout.isZero()) {
      ROS_WARN(
          "waitForResult given infinite timeout, are you sure that's what you "
          "want? Failing.");
    }
    return false;
  }
}

template <class ActionSpec>
bool MockActionClient<ActionSpec>::waitForServer(
    const ::ros::Duration& timeout) {
  if (timeout > server_delay_) {
    return true;
  } else {
    if (timeout.isZero()) {
      ROS_WARN(
          "waitForServer given infinite timeout, are you sure that's what you "
          "want? Failing.");
    }
    return false;
  }
}
}  //   namespace rapid_ros
#endif  // _RAPID_ROS_ACTION_CLIENT_H_