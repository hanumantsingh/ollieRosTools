#ifndef CAMERAATAN_HPP
#define CAMERAATAN_HPP

#include <sensor_msgs/CameraInfo.h>
#include <image_geometry/pinhole_camera_model.h>
#include <ollieRosTools/PreProcNode_paramsConfig.h>


/**
  * Implements the ATAN camera model. For similar notation and more information see:
  * Straight Lines Have to Be Straight (2001) by Frédéric Devernay and Olivier Faugeras
  */

class CameraATAN {

    public:
        CameraATAN():
            fx(0.425198),
            fy(0.579108),
            cx(0.477752),
            cy(0.581669),
            fov(0.970746),
            inWidth(-1),
            inHeight(-1),
            outWidth(720),
            outHeight(576),
            outZoom(1),
            zoomType(NOZOOM),
            interpolation(1),
            outSizeIsInSize(true)
        {
            //initialise(); // Now initialised on first input image
        }


        void rectify(const cv::Mat& imgIn, cv::Mat& imgOut, sensor_msgs::CameraInfoPtr& camInfo ){
            /// Rectifiy image using precomputed matricies and camInfoMessage

            // Check input image size vs previous size. Initial previous size is -1,
            // so this forces an update the first time this function is called

            if (imgIn.cols != inWidth || imgIn.rows != inHeight){
                inWidth  = imgIn.cols;
                inHeight = imgIn.rows;

                if (outSizeIsInSize){
                    outWidth = inWidth;
                    outHeight = inHeight;
                }
                initialise();
            }

            if (interpolation>=0){
                cv::remap(imgIn, imgOut, rect_mapx, rect_mapy, interpolation);
            } else {
                imgIn.copyTo(imgOut);
            }
            camInfo = infoMsgPtr;

        }


        ollieRosTools::PreProcNode_paramsConfig& setParameter(ollieRosTools::PreProcNode_paramsConfig &config, uint32_t level){
            outZoom = config.zoomFactor;
            interpolation = config.PTAMRectify;           
            outSizeIsInSize = config.sameOutInSize;

            if (outSizeIsInSize){
                outWidth = inWidth;
                outHeight = inHeight;
            } else {
                outWidth = config.width;
                outHeight = config.height;
            }

            zoomType = static_cast<ZoomType>(config.zoom);


            fx = config.fx;
            fy = config.fy;
            cx = config.cx;
            cy = config.cy;
            fov = config.s;

            if (inWidth>0 && inHeight>0){
                // Only call this after we have an image size
                initialise();
            }

            return config;
        }



    private:
        enum ZoomType { NOZOOM, FULL, CROP, MANUAL };

        // Warping matricies, precomputed using PTAM paramters
        cv::Mat rect_mapx, rect_mapy;

        // Camera_info that gets sent with the rectified image.
        sensor_msgs::CameraInfoPtr infoMsgPtr;

        // PTAM parameters
        double fx,fy,cx,cy,fov;

        // Set according to imcoming images
        int inWidth, inHeight;

        // User changeable variables
        // Rectified image size
        int outWidth, outHeight;
        // Rectified image scale
        double outZoom;
        ZoomType zoomType;
        // interpolation method
        int interpolation;
        bool outSizeIsInSize;


        void initialise(){
            /// Depending on the incoming image size, outgoing image size/zoom, update the camera_info message and then precompute the warping matrices




            // Input parameters
            const double d2t = 2.0 * tan(fov / 2.0); // equation (13) inner part [see reference at top]
            const double ifx = fx * inWidth ;
            const double ify = fy * inHeight;
            const double icx = cx * inWidth  - 0.5;
            const double icy = cy * inHeight - 0.5;

            // Output paramters
            double ofx, ofy, ocx, ocy;

            if (zoomType == NOZOOM){
                /// Dont apply any zoom, just use the input and output image sizes
                ROS_INFO("Initialising calibration: No Zoom");
                ofx = fx * outWidth;
                ofy = fy * outHeight;
                ocx = cx * outWidth  - 0.5;
                ocy = cy * outHeight - 0.5;

            } else if (zoomType == FULL){
                /// Zoom so the furthest vertical/horizontal valid part of the image is also present.
                /// This might still crop the extremly distroted corners
                ROS_INFO("Initialising calibration: Zoom Full");

                // Find radii to each side
                const double radL = icx/ifx;
                const double radR = (inWidth-1 - icx)/ifx;
                const double radT = icy/ify;
                const double radB = (inHeight-1 - icy)/ify;

                // Find radii to each corner distored
                const double radTL = sqrt(pow(radT, 2) + pow(radL, 2));
                const double radTR = sqrt(pow(radT, 2) + pow(radR, 2));
                const double radBL = sqrt(pow(radB, 2) + pow(radL, 2));
                const double radBR = sqrt(pow(radB, 2) + pow(radR, 2));

                // Radii to each undistored corner: equation (14) inverse distortion function [see reference at top]
                const double invTL = tan(radTL * fov)/d2t;
                const double invTR = tan(radTR * fov)/d2t;
                const double invBL = tan(radBL * fov)/d2t;
                const double invBR = tan(radBR * fov)/d2t;

                // std::maximum distorted
                double hor  = std::max(radBR, radTR) + std::max(radBL, radTL);
                double vert = std::max(radTR, radTL) + std::max(radBL, radBR);
                // std::maximum undistroted
                double invHor  = std::max(invBR, invTR) + std::max(invBL, invTL);
                double invVert = std::max(invTR, invTL) + std::max(invBL, invBR);

                // Ratios define output
                ofx = ifx * ((hor ) / (invHor )) * ((double)outWidth /(double)inWidth );
                ofy = ify * ((vert) / (invVert)) * ((double)outHeight/(double)inHeight);
                ocx = std::max(invBL/radBL, invTL/radTL)*ofx*icx/ifx;
                ocy = std::max(invTL/radTL, invTR/radTR)*ofy*icy/ify;

            } else if (zoomType == CROP){
                ROS_INFO("Initialising calibration: Zoom Crop");
                /// Zoom so the whole output image is valid. Might crop sides of image depending on distortion
                // find left-most and right-most radius
                double radL = (icx)/ifx;
                double radR = (inWidth-1 - icx)/ifx;
                double radT = (icy)/ify;
                double radB = (inHeight-1 - icy)/ify;

                // Undisorted radii to each side
                double invL = tan(radL * fov)/d2t;
                double invR = tan(radR * fov)/d2t;
                double invT = tan(radT * fov)/d2t;
                double invB = tan(radB * fov)/d2t;

                // Ratios define output
                ofx = ifx * ((radL + radR) / (invL + invR)) * ((double)outWidth /(double)inWidth);
                ofy = ify * ((radT + radB) / (invT + invB)) * ((double)outHeight/(double)inHeight);
                ocx = (invL/radL)*ofx*icx/ifx;
                ocy = (invT/radT)*ofy*icy/ify;


            } else { //if (zoomType == MANUAL){
                ROS_INFO("Initialising calibration: Manual Zoom: %.2f", outZoom);
                ofx = fx * outWidth  * outZoom;
                ofy = fy * outHeight * outZoom;
                ocx = cx * outWidth  - 0.5;
                ocy = cy * outHeight - 0.5;
            }


            /// PRECOMPUTE WARP MATRIX
            rect_mapx = cv::Mat(outHeight, outWidth, CV_32FC1, cv::Scalar(0));
            rect_mapy = cv::Mat(outHeight, outWidth, CV_32FC1, cv::Scalar(0));

            for (int x=0; x<outWidth; ++x){
                for (int y=0; y<outHeight; ++y){
                    const float ix = (x - ocx) / ofx;
                    const float iy = (y - ocy) / ofy;
                    const float r = sqrt(ix*ix + iy*iy);
                    const float fac = r==0 ? 1:atan(r * d2t)/(fov*r);
                    rect_mapx.at<float>(y,x) = ifx*fac*ix+icx;
                    rect_mapy.at<float>(y,x) = ify*fac*iy+icy;
                }
            }


            /// CREATE PINHILE CAMERA_INFO MESSAGE
            sensor_msgs::CameraInfo infoMsg;
            infoMsg.width = outWidth;
            infoMsg.height = outHeight;
            infoMsg.distortion_model = "plumb_bob";

            // Rectification rotation matrix (only needed stereo cameras really...)
            infoMsg.R.at(0) = 1;
            infoMsg.R.at(4) = 1;
            infoMsg.R.at(8) = 1;

            // Intrinsic camera matrix for the raw (distorted) image
            infoMsg.K.at(0) = ifx;
            infoMsg.K.at(4) = ify;
            infoMsg.K.at(8) = 1;
            infoMsg.K.at(2) = icx;
            infoMsg.K.at(5) = icy;

            // Projection/camera matrix that specifies the intrinsic (camera) matrix of the processed (rectified) image
            infoMsg.P.at(0) = ifx;
            infoMsg.P.at(5) = ify;
            infoMsg.P.at(10) = 1;
            infoMsg.P.at(2) = icx;
            infoMsg.P.at(6) = icy;

            // The distortion parameters
            infoMsg.D.clear();
            infoMsg.D.push_back(fov);
            infoMsg.D.push_back(0);
            infoMsg.D.push_back(0);
            infoMsg.D.push_back(0);
            infoMsg.D.push_back(0);

            infoMsgPtr = sensor_msgs::CameraInfoPtr(new sensor_msgs::CameraInfo(infoMsg));

            ROS_INFO("   Input [%dpx*%dpx (%.3f)] Output: [%dpx*%dpx (%.3f)]",
                     inWidth, inHeight, static_cast<float>(inWidth)/inHeight,
                     outWidth, outHeight, static_cast<float>(outWidth)/outHeight);

            ROS_INFO("   ATAN MODEL:  F: [%.1f, %.1f, (%.3f)] C: [%.1f, %.1f]", ofx, ofy, ofx/ofy, ocx, ocy);

        }





};

#endif // CAMERAATAN_HPP
