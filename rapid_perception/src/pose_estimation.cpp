#include "rapid_perception/pose_estimation.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits.h>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/Geometry"
#include "geometry_msgs/Vector3.h"
#include "pcl/PointIndices.h"
#include "pcl/common/common.h"
#include "pcl/filters/crop_box.h"
#include "pcl/filters/extract_indices.h"
#include "pcl/point_cloud.h"
#include "pcl/point_types.h"
#include "pcl/registration/icp.h"
#include "pcl/search/kdtree.h"
#include "pcl_conversions/pcl_conversions.h"
#include "sensor_msgs/PointCloud2.h"

#include "rapid_msgs/Roi3D.h"
#include "rapid_utils/stochastic_universal_sampling.h"
#include "rapid_viz/publish.h"

typedef pcl::PointXYZRGB PointC;
typedef pcl::PointXYZ PointP;
typedef pcl::PointCloud<PointC> PointCloudC;
typedef pcl::PointCloud<PointP> PointCloudP;
typedef pcl::FPFHSignature33 FPFH;
typedef pcl::PointCloud<FPFH> PointCloudF;
typedef pcl::search::KdTree<FPFH> FeatureTree;
typedef pcl::search::KdTree<PointC> PointCTree;
typedef pcl::search::KdTree<PointP> PointPTree;
using std::pair;
using std::string;
using std::vector;

namespace rapid {
namespace perception {
PoseEstimationMatch::PoseEstimationMatch(PointCloudC::Ptr cloud, double fitness)
    : cloud_(new PointCloudC), fitness_(fitness) {
  *cloud_ = *cloud;
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D(*cloud_, min_pt, max_pt);
  center_.x = min_pt.x() + (max_pt.x() - min_pt.x()) / 2;
  center_.y = min_pt.y() + (max_pt.y() - min_pt.y()) / 2;
  center_.z = min_pt.z() + (max_pt.z() - min_pt.z()) / 2;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr PoseEstimationMatch::cloud() {
  return cloud_;
}

pcl::PointXYZ PoseEstimationMatch::center() const { return center_; }

double PoseEstimationMatch::fitness() const { return fitness_; }

void PoseEstimationMatch::set_fitness(double fitness) { fitness_ = fitness; }

PoseEstimator::PoseEstimator()
    : scene_(new PointCloudC()),
      object_(new PointCloudC()),
      object_center_(),
      num_candidates_(100),
      fitness_threshold_(0.00002),
      nms_radius_(0.03),
      debug_(false),
      candidates_pub_(),
      alignment_pub_(),
      output_pub_() {}

void PoseEstimator::set_heat_mapper(PoseEstimationHeatMapper* heat_mapper) {
  heat_mapper_ = heat_mapper;
}

void PoseEstimator::set_scene(PointCloudC::Ptr scene) {
  scene_ = scene;
  heat_mapper_->set_scene(scene);
}

void PoseEstimator::set_object(
    const pcl::PointCloud<pcl::PointXYZRGB>::Ptr& object,
    const rapid_msgs::Roi3D& roi) {
  object_ = object;
  object_roi_ = roi;

  // Estimate object radius, just average of x/y/z dimensions.
  Eigen::Vector4f min_pt;
  Eigen::Vector4f max_pt;
  pcl::getMinMax3D(*object_, min_pt, max_pt);
  object_center_.x() = (max_pt.x() + min_pt.x()) / 2;
  object_center_.y() = (max_pt.y() + min_pt.y()) / 2;
  object_center_.z() = (max_pt.z() + min_pt.z()) / 2;

  heat_mapper_->set_object(object_);
}

PoseEstimationHeatMapper* PoseEstimator::heat_mapper() { return heat_mapper_; }

void PoseEstimator::set_debug(bool val) {
  debug_ = val;
  heat_mapper_->set_debug(val);
}
void PoseEstimator::set_num_candidates(int val) { num_candidates_ = val; }
void PoseEstimator::set_fitness_threshold(double val) {
  fitness_threshold_ = val;
}
void PoseEstimator::set_nms_radius(double val) { nms_radius_ = val; }

void PoseEstimator::set_candidates_publisher(const ros::Publisher& pub) {
  candidates_pub_ = pub;
}
void PoseEstimator::set_alignment_publisher(const ros::Publisher& pub) {
  alignment_pub_ = pub;
}
void PoseEstimator::set_output_publisher(const ros::Publisher& pub) {
  output_pub_ = pub;
}

bool PoseEstimator::Find() {
  // Compute heatmap of features
  pcl::PointIndicesPtr heatmap_indices(new pcl::PointIndices);
  Eigen::VectorXd importances;
  heat_mapper_->Compute(heatmap_indices, &importances);

  // Sample candidate points from the heatmap
  pcl::PointIndices::Ptr candidate_indices(new pcl::PointIndices);
  ComputeTopCandidates(importances, heatmap_indices, candidate_indices);

  // For each candidate point, run ICP
  vector<PoseEstimationMatch> output_objects;
  RunIcpCandidates(candidate_indices, &output_objects);
  if (output_objects.size() == 0) {
    ROS_WARN("No matches found.");
    return false;
  }

  // Non-max suppression
  vector<bool> keep;
  NonMaxSuppression(output_objects, &keep);

  PointCloudC::Ptr output_cloud(new PointCloudC);
  int num_instances = 0;
  for (size_t i = 0; i < output_objects.size(); ++i) {
    if (keep[i]) {
      double r = static_cast<double>(rand()) / RAND_MAX;
      double g = static_cast<double>(rand()) / RAND_MAX;
      double b = static_cast<double>(rand()) / RAND_MAX;
      Colorize(output_objects[i].cloud(), r, g, b);
      *output_cloud += *output_objects[i].cloud();
      ++num_instances;
    }
  }
  ROS_INFO("Found %d instances of the model", num_instances);
  output_cloud->header.frame_id = scene_->header.frame_id;
  viz::PublishCloud(output_pub_, *output_cloud);
  return true;
}

void PoseEstimator::ComputeCandidates(Eigen::VectorXd& importances,
                                      pcl::PointIndicesPtr heatmap_indices,
                                      pcl::PointIndicesPtr candidate_indices) {
  // Sample points based on importance
  double sum = importances.sum();
  importances /= sum;

  vector<double> cdf;
  double cdf_val = 0;
  for (size_t indices_i = 0; indices_i < heatmap_indices->indices.size();
       ++indices_i) {
    double prob = importances(indices_i);
    cdf_val += prob;
    cdf.push_back(cdf_val);
  }

  // Indices of indices! We are sampling the indices vector by importance. The
  // sampled indices themselves are stored in sampled_important_indices, so the
  // actual point index for i in sampled_important_indices is
  // cloud[indices[sampled_important_indices[i]]].
  vector<int> sampled_important_indices;
  utils::StochasticUniversalSampling(cdf, num_candidates_,
                                     &sampled_important_indices);

  for (size_t i = 0; i < sampled_important_indices.size(); ++i) {
    int index_into_indices = sampled_important_indices[i];
    int point_index = heatmap_indices->indices[index_into_indices];
    candidate_indices->indices.push_back(point_index);
  }

  // Visualize the candidate points
  PointCloudC::Ptr viz_cloud(new PointCloudC());
  viz_cloud.reset(new PointCloudC());
  pcl::ExtractIndices<PointC> extract;
  extract.setInputCloud(scene_);
  extract.setIndices(candidate_indices);
  extract.filter(*viz_cloud);
  viz::PublishCloud(candidates_pub_, *viz_cloud);
}

// This is used to sort heatmap scores, represented as (index, score).
// Higher scores are better.
bool CompareScores(const pair<int, double>& a, const pair<int, double>& b) {
  return a.second > b.second;
}

void PoseEstimator::ComputeTopCandidates(
    Eigen::VectorXd& importances, pcl::PointIndicesPtr heatmap_indices,
    pcl::PointIndicesPtr candidate_indices) {
  std::vector<std::pair<int, double> > scores;
  for (size_t i = 0; i < heatmap_indices->indices.size(); ++i) {
    int index = heatmap_indices->indices[i];
    double score = importances[i];
    scores.push_back(std::make_pair(index, score));
  }

  size_t num = std::min(scores.size(), static_cast<size_t>(num_candidates_));
  std::partial_sort(scores.begin(), scores.begin() + num, scores.end(),
                    &CompareScores);

  candidate_indices->indices.clear();
  for (size_t i = 0; i < num; ++i) {
    candidate_indices->indices.push_back(scores[i].first);
  }

  // Visualize the candidate points
  PointCloudC::Ptr viz_cloud(new PointCloudC());
  viz_cloud.reset(new PointCloudC());
  pcl::ExtractIndices<PointC> extract;
  extract.setInputCloud(scene_);
  extract.setIndices(candidate_indices);
  extract.filter(*viz_cloud);
  viz::PublishCloud(candidates_pub_, *viz_cloud);
}

void PoseEstimator::RunIcpCandidates(
    pcl::PointIndices::Ptr candidate_indices,
    vector<PoseEstimationMatch>* output_objects) {
  // Copy the object and its center.
  PointCloudC::Ptr working_object(new PointCloudC);
  PointCloudC::Ptr aligned_object(new PointCloudC);
  pcl::IterativeClosestPoint<PointC, PointC> icp;
  icp.setInputTarget(scene_);
  output_objects->clear();
  for (size_t ci = 0; ci < candidate_indices->indices.size(); ++ci) {
    int index = candidate_indices->indices[ci];
    const PointC& center = scene_->points[index];
    // Transform the object to be centered on the center point
    // So far we just use a translation offset
    // Also assume that the object and scene are in the same frame
    Eigen::Vector3d translation;
    translation.x() = center.x - object_center_.x();
    translation.y() = center.y - object_center_.y();
    translation.z() = center.z - object_center_.z();
    pcl::transformPointCloud(*object_, *working_object, translation,
                             Eigen::Quaterniond::Identity());

    // Run ICP
    icp.setInputSource(working_object);
    icp.align(*aligned_object);
    viz::PublishCloud(alignment_pub_, *aligned_object);
    if (icp.hasConverged()) {
      Eigen::Matrix4f final_transform = icp.getFinalTransformation();
      rapid_msgs::Roi3D roi = object_roi_;
      roi.transform.translation.x += translation.x();
      roi.transform.translation.y += translation.y();
      roi.transform.translation.z += translation.z();
      Eigen::Matrix3f rot_matrix(final_transform.topLeftCorner(3, 3));
      Eigen::Quaternionf q(rot_matrix);
      roi.transform.rotation.w = q.w();
      roi.transform.rotation.x = q.x();
      roi.transform.rotation.y = q.y();
      roi.transform.rotation.z = q.z();

      double fitness = ComputeIcpFitness(scene_, aligned_object, roi);

      string threshold_marker = "";
      if (fitness < fitness_threshold_) {
        output_objects->push_back(PoseEstimationMatch(aligned_object, fitness));
        threshold_marker = " -- below threshold";
      }
      ROS_INFO("ICP converged with score: %f%s", fitness,
               threshold_marker.c_str());
    }
    if (debug_) {
      std::cout << "Press enter to continue ";
      string user_input;
      std::getline(std::cin, user_input);
    }
  }
}

void PoseEstimator::NonMaxSuppression(
    vector<PoseEstimationMatch>& output_objects, vector<bool>* keep) {
  // Non-max supression of output objects
  // Index centers into a tree
  PointCloudP::Ptr output_centers(new PointCloudP);
  for (size_t i = 0; i < output_objects.size(); ++i) {
    output_centers->push_back(output_objects[i].center());
  }
  PointPTree output_centers_tree;
  output_centers_tree.setInputCloud(output_centers);

  // Suppress non max within a radius
  for (int i = 0; i < static_cast<int>(output_objects.size()); ++i) {
    PoseEstimationMatch& match = output_objects[i];
    vector<int> neighbor_indices;
    vector<float> neighbor_distances;
    output_centers_tree.radiusSearch(match.center(), nms_radius_,
                                     neighbor_indices, neighbor_distances);

    bool keep_match = true;
    for (size_t k = 0; k < neighbor_indices.size(); ++k) {
      int index = neighbor_indices[k];
      if (index == i) {
        continue;
      }
      PoseEstimationMatch& neighbor = output_objects[index];
      if (neighbor.fitness() < match.fitness()) {
        if (debug_) {
          ROS_INFO("Suppressing match %d, %f >= neighbor %d, %f", i,
                   match.fitness(), index, neighbor.fitness());
        }
        keep_match = false;
        break;
      } else if (neighbor.fitness() == match.fitness()) {
        // Keep this one, bump neighbor's fitness score up so that it won't come
        // up again.
        neighbor.set_fitness(neighbor.fitness() * 1.01);
      }
    }
    keep->push_back(keep_match);
  }
}

void Colorize(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, double r, double g,
              double b) {
  for (size_t i = 0; i < cloud->size(); ++i) {
    cloud->points[i].r = static_cast<int>(round(r * 255));
    cloud->points[i].g = static_cast<int>(round(g * 255));
    cloud->points[i].b = static_cast<int>(round(b * 255));
  }
}

double PoseEstimator::ComputeIcpFitness(PointCloudC::Ptr& scene,
                                        PointCloudC::Ptr& object,
                                        const rapid_msgs::Roi3D& roi) {
  Eigen::Vector4f min;
  min.x() = -roi.dimensions.x / 2;
  min.y() = -roi.dimensions.y / 2;
  min.z() = -roi.dimensions.z / 2;
  Eigen::Vector4f max;
  max.x() = roi.dimensions.x / 2;
  max.y() = roi.dimensions.y / 2;
  max.z() = roi.dimensions.z / 2;
  Eigen::Vector3f translation;
  translation.x() = roi.transform.translation.x;
  translation.y() = roi.transform.translation.y;
  translation.z() = roi.transform.translation.z;
  Eigen::Quaternionf rotation;
  rotation.w() = roi.transform.rotation.w;
  rotation.x() = roi.transform.rotation.x;
  rotation.y() = roi.transform.rotation.y;
  rotation.z() = roi.transform.rotation.z;
  Eigen::Matrix3f rot_mat = rotation.toRotationMatrix();
  Eigen::Vector3f ea;
  ea.x() = atan2(rot_mat(2, 1), rot_mat(1, 1));
  ea.y() = atan2(rot_mat(2, 0), rot_mat(0, 0));
  ea.z() = atan2(rot_mat(1, 0), rot_mat(0, 0));

  pcl::CropBox<PointC> crop;
  crop.setInputCloud(scene);
  crop.setMin(min);
  crop.setMax(max);
  crop.setTranslation(translation);
  crop.setRotation(ea);
  vector<int> indices;
  crop.filter(indices);

  // if (debug_) {
  //  std::cout << "Press enter to view cropped area" << std::endl;
  //  string input;
  //  std::getline(std::cin, input);
  //  PointCloudC::Ptr cropped(new PointCloudC);
  //  crop.filter(*cropped);
  //  viz::PublishCloud(alignment_pub_, *cropped);
  //  std::cout << "Press enter to finish viewing cropped area" << std::endl;
  //  std::getline(std::cin, input);
  //}

  PointCTree object_tree;
  object_tree.setInputCloud(object);
  vector<int> nn_indices(1);
  vector<float> nn_dists(1);
  double fitness = 0;
  for (size_t index_i = 0; index_i < indices.size(); ++index_i) {
    int index = indices[index_i];
    const PointC& scene_pt = scene->at(index);
    object_tree.nearestKSearch(scene_pt, 1, nn_indices, nn_dists);
    fitness += nn_dists[0];
  }
  fitness /= indices.size();
  return fitness;
}
}  // namespace perception
}  // namespace rapid
