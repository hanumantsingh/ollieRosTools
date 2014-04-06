#ifndef AUX_HPP
#define AUX_HPP

extern bool USEIMU;


#include <string>
#include <boost/assign.hpp>

#include <Eigen/Core>
#include <opengv/types.hpp>
#include <opencv2/opencv.hpp>

#include <tf/tf.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <visualization_msgs/MarkerArray.h>
#include <eigen_conversions/eigen_msg.h>




//forward declated
class Frame;
class OdoPoint;

#define __SHORTFILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef std::vector< cv::KeyPoint > KeyPoints;
typedef std::vector< cv::DMatch > DMatches;
typedef std::vector< DMatches > DMatchesKNN;
//typedef std::vector< cv::Point3d > Points3d;
typedef std::vector< cv::Point2d > Points2d;
typedef std::vector< cv::Point2f > Points2f;
typedef Points2d Points2;
typedef std::vector< int > Ints;
typedef std::vector< bool > Bools;
typedef std::vector< float > Floats;
typedef std::vector< double > Doubles;
typedef std::vector< cv::Mat > Mats;
typedef cv::Ptr<Frame> FramePtr;
typedef std::deque<FramePtr> FramePtrs;
typedef cv::Ptr<OdoPoint> PointPtr;
typedef std::deque<PointPtr> PointPtrs;

const float toRad = M_PI/180;
const float toDeg = 180/M_PI;



namespace OVO {

    /// TODO: put this in a look up table
    CvScalar getColor(const float range_min, const float range_max, const float depth, bool reverse = false);

    void tf2RPY(const tf::Transform& T, double& R, double& P, double& Y);
    void drawTextCenter(cv::Mat& img, const std::string& text, const CvScalar RGB, const float textScale, const int textThickness);



    // Removes rows of Mat if index is not in ind. Can also change the order of elements.
    void matReduceInd (const cv::Mat& matIn, cv::Mat& matOut, const Ints& ind);

    void match2ind(const DMatches& ms, Ints& query, Ints& train);

    static std::map<int, std::string> COLORS= boost::assign::map_list_of
            (-1, std::string(""))
            (0, sensor_msgs::image_encodings::BGR8)
            (1, sensor_msgs::image_encodings::RGB8)
            (2, sensor_msgs::image_encodings::MONO8)
            (3, sensor_msgs::image_encodings::YUV422);

    cv::Mat getRosImage(const sensor_msgs::ImageConstPtr& msg, int colorId = 0);

    visualization_msgs::Marker getPointsMarker(const opengv::points_t& worldPoints);



    // Draw a number onto an image
    void putInt(cv::Mat& img,  const float nr, const cv::Point& p, const CvScalar& col, const bool round, const std::string& str,const std::string& post="");

    // align bearing vectors
    void alignedBV (const Eigen::MatrixXd& bvm1, const Eigen::MatrixXd& bvm2, const DMatches& ms, opengv::bearingVectors_t& bv1, opengv::bearingVectors_t& bv2);
    void matReduceInd (const Eigen::MatrixXd& bvm1, opengv::bearingVectors_t& bv1, const Ints& ind);

    void transformPoints(const Eigen::Affine3d& transform, opengv::points_t& points);

    Eigen::VectorXd reprojectErrPointsVsBV(
            const Eigen::Affine3d& model,
            const opengv::points_t& points,
            const opengv::bearingVectors_t& bv);

    template <class T, class T2> void vecAlignMatch (const T& vec1in, const T2& vec2in,
                                                     T& vec1out,      T2& vec2out,
                                                     const DMatches& ms){
        vec1out.clear();
        vec2out.clear();
        vec1out.reserve(ms.size());
        vec2out.reserve(ms.size());
        for(uint i=0;i<ms.size(); ++i){
            vec1out.push_back(vec1in[ms[i].queryIdx]);
            vec2out.push_back(vec2in[ms[i].trainIdx]);
        }
    }

    //reduce vector using only indicies. eg [z a b c d e f],[1 2 5 3] -> [a b e c]
    template <class T> void vecReduceInd (const T& vecIn, T& vecOut, const Ints& ind){
        vecOut.clear();
        vecOut.reserve(ind.size());
        for(uint i=0;i<ind.size(); ++i){
            vecOut.push_back(vecIn[ind[i]]);
        }
    }


}

#endif // AUX_HPP
