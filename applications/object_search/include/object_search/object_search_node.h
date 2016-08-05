#ifndef _OBJECT_SEARCH_OBJECT_SEARCH_NODE_H_
#define _OBJECT_SEARCH_OBJECT_SEARCH_NODE_H_

#include <string>

#include "geometry_msgs/Transform.h"
#include "object_search_msgs/Search.h"
#include "pcl/point_cloud.h"
#include "pcl/point_types.h"
#include "rapid_perception/pose_estimation.h"

namespace object_search {
class ObjectSearchNode {
 public:
  ObjectSearchNode(const rapid::perception::PoseEstimator& estimator);
  bool Serve(object_search_msgs::SearchRequest& req,
             object_search_msgs::SearchResponse& resp);

 private:
  void UpdateParams();
  void Downsample(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr in,
                  pcl::PointCloud<pcl::PointXYZRGB>::Ptr out);
  void TransformToBase(pcl::PointCloud<pcl::PointXYZRGB>::ConstPtr in,
                       const std::string& parent_frame_id,
                       const geometry_msgs::Transform& base_to_camera,
                       pcl::PointCloud<pcl::PointXYZRGB>::Ptr out);
  void CropScene(pcl::PointCloud<pcl::PointXYZRGB>::Ptr scene);

  rapid::perception::PoseEstimator estimator_;

  // Parameters
  // Voxelization
  double leaf_size_;

  // Scene cropping
  double min_x_;
  double min_y_;
  double min_z_;
  double max_x_;
  double max_y_;
  double max_z_;

  // Search
  double sample_ratio_;
  int max_samples_;
  double fitness_threshold_;
  double sigma_threshold_;
  double nms_radius_;
};
}  // namespace object_search

#endif  // _OBJECT_SEARCH_OBJECT_SEARCH_NODE_H_
