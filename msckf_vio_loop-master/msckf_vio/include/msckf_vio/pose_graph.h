/*
 * COPYRIGHT AND PERMISSION NOTICE
 * Penn Software MSCKF_VIO
 * Copyright (C) 2017 The Trustees of the University of Pennsylvania
 * All rights reserved.
 */
#pragma once
#ifndef MSCKF_VIO_POSE_GRAPH_H
#define MSCKF_VIO_POSE_GRAPH_H

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/video.hpp>

#include <ros/ros.h>
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/Image.h>
#include <message_filters/subscriber.h>
#include <message_filters/time_synchronizer.h>

#include <thread>
#include <mutex>
#include <eigen3/Eigen/Dense>
#include <string>
#include <ceres/ceres.h>
#include <ceres/rotation.h>
#include <queue>
#include <assert.h>
#include <nav_msgs/Path.h>
#include <geometry_msgs/PointStamped.h>
#include <nav_msgs/Odometry.h>
#include <stdio.h>
#include "msckf_vio/keyframe.h"
#include "msckf_vio/tic_toc.h"
#include "msckf_vio/utility.h"
#include "msckf_vio/CameraPoseVisualization.h"
#include "msckf_vio/DBoW2.h"
#include "msckf_vio/DVision.h"
#include "msckf_vio/TemplatedDatabase.h"
#include "msckf_vio/TemplatedVocabulary.h"
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/image_encodings.h>
#include <visualization_msgs/Marker.h>
#include <std_msgs/Bool.h>
#include <iostream>
#include <ros/package.h>
#include <opencv2/core/eigen.hpp>
#include "msckf_vio/parameters.h"

namespace msckf_vio {
#define SKIP_FIRST_CNT 10
using namespace std;

#define SHOW_S_EDGE false
#define SHOW_L_EDGE true
#define SAVE_LOOP_PATH true

using namespace DVision;
using namespace DBoW2;


/*
 * @brief ImageProcessor Detects and tracks features
 *    in image sequences.
 */
class PoseGraph {
public:
  // Constructor
  PoseGraph(ros::NodeHandle& n);
  // Disable copy and assign constructors.
  PoseGraph(const PoseGraph&) = delete;
  PoseGraph operator=(const PoseGraph&) = delete;

  // Destructor
  ~PoseGraph();

  // Initialize the object.
  bool initialize();

  typedef boost::shared_ptr<PoseGraph> Ptr;
  typedef boost::shared_ptr<const PoseGraph> ConstPtr;
  
  void registerPub(ros::NodeHandle &n);
  void addKeyFrame(KeyFrame* cur_kf, bool flag_detect_loop);
  void loadKeyFrame(KeyFrame* cur_kf, bool flag_detect_loop);
  void loadVocabulary(std::string voc_path);
  void updateKeyFrameLoop(int index, Eigen::Matrix<double, 8, 1 > &_loop_info);
  KeyFrame* getKeyFrame(int index);
  nav_msgs::Path path[10];
  nav_msgs::Path base_path;
  CameraPoseVisualization* posegraph_visualization;
  void savePoseGraph();
  void loadPoseGraph();
  void publish();
  Vector3d t_drift;
  double yaw_drift;
  Matrix3d r_drift;
  // world frame( base sequence or first sequence)<----> cur sequence frame  
  Vector3d w_t_vio;
  Matrix3d w_r_vio;
private:

  int detectLoop(KeyFrame* keyframe, int frame_index);
  void addKeyFrameIntoVoc(KeyFrame* keyframe);
  void optimize4DoF();
  void updatePath();
  list<KeyFrame*> keyframelist;
  std::mutex m_keyframelist;
  std::mutex m_optimize_buf;
  std::mutex m_path;
  std::mutex m_drift;
  std::thread t_optimization;
  std::thread measurement_process;
  std::thread keyboard_command_process;
  std::queue<int> optimize_buf;

  int global_index;
  int sequence_cnt;
  vector<bool> sequence_loop;
  map<int, cv::Mat> image_pool;
  int earliest_loop_index;
  int base_sequence;

  BriefDatabase db;
  BriefVocabulary* voc;

  ros::Publisher pub_pg_path;
  ros::Publisher pub_base_path;
  ros::Publisher pub_pose_graph;
  ros::Publisher pub_path[10];
  // Ros node handle
  ros::NodeHandle nh;
  bool loadParameters();
  bool createRosIO();
  ros::Subscriber sub_vio;
  ros::Subscriber sub_image;
  ros::Subscriber sub_pose;
  ros::Subscriber sub_extrinsic;
  ros::Subscriber sub_point;
  ros::Publisher pub_camera_pose_visual;
  ros::Publisher pub_key_odometrys;
  ros::Publisher pub_vio_path;
  queue<sensor_msgs::ImageConstPtr> image_buf;
  queue<sensor_msgs::PointCloudConstPtr> point_buf;
  queue<nav_msgs::Odometry::ConstPtr> pose_buf;
  queue<Eigen::Vector3d> odometry_buf;
  std::mutex m_buf;
  std::mutex m_process;
  
  void new_sequence();
  void image_callback(const sensor_msgs::ImageConstPtr &image_msg);
  void point_callback(const sensor_msgs::PointCloudConstPtr &point_msg);
  void pose_callback(const nav_msgs::Odometry::ConstPtr &pose_msg);
  void relo_relative_pose_callback(const nav_msgs::Odometry::ConstPtr &pose_msg);
  void vio_callback(const nav_msgs::Odometry::ConstPtr &pose_msg);
  void extrinsic_callback(const nav_msgs::Odometry::ConstPtr &pose_msg);
  void process();
  void command();
  std::string IMAGE_TOPIC;
};

typedef PoseGraph::Ptr PoseGraphPtr;
typedef PoseGraph::ConstPtr PoseGraphConstPtr;

template <typename T>
T NormalizeAngle(const T& angle_degrees) {
  if (angle_degrees > T(180.0))
  	return angle_degrees - T(360.0);
  else if (angle_degrees < T(-180.0))
  	return angle_degrees + T(360.0);
  else
  	return angle_degrees;
};

class AngleLocalParameterization {
 public:

  template <typename T>
  bool operator()(const T* theta_radians, const T* delta_theta_radians,
                  T* theta_radians_plus_delta) const {
    *theta_radians_plus_delta =
        NormalizeAngle(*theta_radians + *delta_theta_radians);

    return true;
  }

  static ceres::LocalParameterization* Create() {
    return (new ceres::AutoDiffLocalParameterization<AngleLocalParameterization,
                                                     1, 1>);
  }
};

template <typename T> 
void YawPitchRollToRotationMatrix(const T yaw, const T pitch, const T roll, T R[9])
{

	T y = yaw / T(180.0) * T(M_PI);
	T p = pitch / T(180.0) * T(M_PI);
	T r = roll / T(180.0) * T(M_PI);


	R[0] = cos(y) * cos(p);
	R[1] = -sin(y) * cos(r) + cos(y) * sin(p) * sin(r);
	R[2] = sin(y) * sin(r) + cos(y) * sin(p) * cos(r);
	R[3] = sin(y) * cos(p);
	R[4] = cos(y) * cos(r) + sin(y) * sin(p) * sin(r);
	R[5] = -cos(y) * sin(r) + sin(y) * sin(p) * cos(r);
	R[6] = -sin(p);
	R[7] = cos(p) * sin(r);
	R[8] = cos(p) * cos(r);
};

template <typename T> 
void RotationMatrixTranspose(const T R[9], T inv_R[9])
{
	inv_R[0] = R[0];
	inv_R[1] = R[3];
	inv_R[2] = R[6];
	inv_R[3] = R[1];
	inv_R[4] = R[4];
	inv_R[5] = R[7];
	inv_R[6] = R[2];
	inv_R[7] = R[5];
	inv_R[8] = R[8];
};

template <typename T> 
void RotationMatrixRotatePoint(const T R[9], const T t[3], T r_t[3])
{
	r_t[0] = R[0] * t[0] + R[1] * t[1] + R[2] * t[2];
	r_t[1] = R[3] * t[0] + R[4] * t[1] + R[5] * t[2];
	r_t[2] = R[6] * t[0] + R[7] * t[1] + R[8] * t[2];
};

struct FourDOFError
{
	FourDOFError(double t_x, double t_y, double t_z, double relative_yaw, double pitch_i, double roll_i)
				  :t_x(t_x), t_y(t_y), t_z(t_z), relative_yaw(relative_yaw), pitch_i(pitch_i), roll_i(roll_i){}

	template <typename T>
	bool operator()(const T* const yaw_i, const T* ti, const T* yaw_j, const T* tj, T* residuals) const
	{
		T t_w_ij[3];
		t_w_ij[0] = tj[0] - ti[0];
		t_w_ij[1] = tj[1] - ti[1];
		t_w_ij[2] = tj[2] - ti[2];

		// euler to rotation
		T w_R_i[9];
		YawPitchRollToRotationMatrix(yaw_i[0], T(pitch_i), T(roll_i), w_R_i);
		// rotation transpose
		T i_R_w[9];
		RotationMatrixTranspose(w_R_i, i_R_w);
		// rotation matrix rotate point
		T t_i_ij[3];
		RotationMatrixRotatePoint(i_R_w, t_w_ij, t_i_ij);

		residuals[0] = (t_i_ij[0] - T(t_x));
		residuals[1] = (t_i_ij[1] - T(t_y));
		residuals[2] = (t_i_ij[2] - T(t_z));
		residuals[3] = NormalizeAngle(yaw_j[0] - yaw_i[0] - T(relative_yaw));

		return true;
	}

	static ceres::CostFunction* Create(const double t_x, const double t_y, const double t_z,
									   const double relative_yaw, const double pitch_i, const double roll_i) 
	{
	  return (new ceres::AutoDiffCostFunction<
	          FourDOFError, 4, 1, 3, 1, 3>(
	          	new FourDOFError(t_x, t_y, t_z, relative_yaw, pitch_i, roll_i)));
	}

	double t_x, t_y, t_z;
	double relative_yaw, pitch_i, roll_i;

};

struct FourDOFWeightError
{
	FourDOFWeightError(double t_x, double t_y, double t_z, double relative_yaw, double pitch_i, double roll_i)
				  :t_x(t_x), t_y(t_y), t_z(t_z), relative_yaw(relative_yaw), pitch_i(pitch_i), roll_i(roll_i){
				  	weight = 1;
				  }

	template <typename T>
	bool operator()(const T* const yaw_i, const T* ti, const T* yaw_j, const T* tj, T* residuals) const
	{
		T t_w_ij[3];
		t_w_ij[0] = tj[0] - ti[0];
		t_w_ij[1] = tj[1] - ti[1];
		t_w_ij[2] = tj[2] - ti[2];

		// euler to rotation
		T w_R_i[9];
		YawPitchRollToRotationMatrix(yaw_i[0], T(pitch_i), T(roll_i), w_R_i);
		// rotation transpose
		T i_R_w[9];
		RotationMatrixTranspose(w_R_i, i_R_w);
		// rotation matrix rotate point
		T t_i_ij[3];
		RotationMatrixRotatePoint(i_R_w, t_w_ij, t_i_ij);

		residuals[0] = (t_i_ij[0] - T(t_x)) * T(weight);
		residuals[1] = (t_i_ij[1] - T(t_y)) * T(weight);
		residuals[2] = (t_i_ij[2] - T(t_z)) * T(weight);
		residuals[3] = NormalizeAngle((yaw_j[0] - yaw_i[0] - T(relative_yaw))) * T(weight) / T(10.0);

		return true;
	}

	static ceres::CostFunction* Create(const double t_x, const double t_y, const double t_z,
									   const double relative_yaw, const double pitch_i, const double roll_i) 
	{
	  return (new ceres::AutoDiffCostFunction<
	          FourDOFWeightError, 4, 1, 3, 1, 3>(
	          	new FourDOFWeightError(t_x, t_y, t_z, relative_yaw, pitch_i, roll_i)));
	}

	double t_x, t_y, t_z;
	double relative_yaw, pitch_i, roll_i;
	double weight;

};

} // end namespace msckf_vio

#endif
