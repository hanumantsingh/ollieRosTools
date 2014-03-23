
/*****************************************************************************
 ** Ifdefs
 *****************************************************************************/

#ifndef VONODE_HPP_
#define VONODE_HPP_

/*****************************************************************************
 ** Includes
 *****************************************************************************/

#include <dynamic_reconfigure/server.h>
#include <image_geometry/pinhole_camera_model.h>
#include <image_transport/image_transport.h>
#include <ollieRosTools/VoNode_paramsConfig.h>
#include <ollieRosTools/aux.hpp>
#include <ollieRosTools/Frame.hpp>
#include <opencv2/opencv.hpp>
#include <ros/package.h>
#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <tf/transform_listener.h>


#include <sensor_msgs/image_encodings.h>
#include <string>
#include <ollieRosTools/CameraATAN.hpp>
#include <map>
/*****************************************************************************
 ** Class
 *****************************************************************************/


class VoNode{



	public:
        VoNode(ros::NodeHandle& _n);
        ~VoNode();

	private:

        /// ROS specific stuff
        ros::NodeHandle& n;
        image_transport::ImageTransport imTransport;
        image_transport::CameraPublisher pubCamera;
        image_transport::Subscriber subImage;
        tf::TransformBroadcaster pubTF;
        tf::TransformListener subTF;

        /// Dynamic Reconfigure
        ollieRosTools::VoNode_paramsConfig configLast;
        dynamic_reconfigure::Server<ollieRosTools::VoNode_paramsConfig> srv;
        ollieRosTools::VoNode_paramsConfig&  setParameter(ollieRosTools::VoNode_paramsConfig &config, uint32_t level);

        /// Members
        float timeAlpha;
        ros::Time lastTime; // Keep track of time to detect loops in bag files


        /// Dynamic parameters
        bool nodeOn;
        bool repeatOn;
        float timeAvg;
        int colorId;
        ros::Duration imgDelay;


        /// Parameters
        std::string inputTopic;
        bool useIMU;
        std::string imuFrame;
        std::string worldFrame;

        /// Callbacks

        void incomingImage(const sensor_msgs::ImageConstPtr& msg);




        /// REMOVE
        Frame prevFrame;



};

#endif