#include <ollieRosTools/Detector.hpp>


/// HELPER FUNCTIONS

// Compares keypoints by response, larger better
bool kp_compare(const cv::KeyPoint& kp1, const cv::KeyPoint& kp2) {
    return std::abs((kp1.response) > std::abs(kp2.response));
}

// Compares matches by distance, smaller better
// DONT NEED THIS, <  overloaded
//bool match_compare(const cv::DMatch& m1, const cv::DMatch& m2) {
//    return m1.distance < m2.distance;
//}

// Returns true if keypoint not good enough
bool kp_bad(const cv::KeyPoint& kp, const double thresh){
    return (std::abs(kp.response) < thresh);
}


// Sort keypoints by response and take nr best
void kp_clip(KeyPoints& kps, const uint max_nr){
    if (kps.size()>max_nr){
        std::sort( kps.begin(),
                   kps.end(),
                   kp_compare
                   );
        kps.erase(kps.begin()+MIN(max_nr,kps.size()),kps.end());
    }
}



// Return the smallest, largest reponse along with the total nr of keypoints
void minMaxTotal(const KeyPoints& kps, double& minval, double& maxval, uint& total){
    minval = INFINITY;
    maxval = -INFINITY;
    for (uint x=0; x < kps.size(); ++x){
        double r = kps[x].response;
        if( r < minval ) {
            minval = r;
        }
        if( r > maxval ){
            maxval = r;
        }
    }
    total =kps.size();
}

//// Return the smallest, largest distance along with the total nr of matches
//void minMaxTotal(const DMatches& ms, double& minval, double& maxval, uint& total){
//    minval = INFINITY;
//    maxval = -INFINITY;
//    for (uint x=0; x < ms.size(); ++x){
//        double r = ms[x].distance;
//        if( r < minval ) {
//            minval = r;
//        }
//        if( r > maxval ){
//            maxval = r;
//        }
//    }
//    total =ms.size();
//}

void subPixKP(const cv::Mat& img, KeyPoints& kps, const int window2k1, const int zeroZone=-1, const int iterations = 20, const int accuracy=0.05 ){

    if (kps.size()>0){
        Points2f points;
        cv::KeyPoint::convert(kps, points);

        cv::cornerSubPix(img, points, cv::Size(window2k1,window2k1), cv::Size(zeroZone,zeroZone), cv::TermCriteria( CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, iterations, accuracy ));

        for (uint i=0; i<points.size(); ++i){
            kps[i].pt = points[i];
        }
    }

}

void kp_threshold(KeyPoints& kps, double thresh){
    kps.erase( std::remove_if( kps.begin(), kps.end(), boost::bind(kp_bad, _1, thresh)), kps.end() );
}

/// CLASS IMPLEMENTATION



Detector::Detector(){
    // Default values (actually set by dyn reconf)
    kp_octaves = 4;
    kp_octaveLayers = 2;
    kp_max = 200;
    kp_thresh = 0; // 0 = no threshold
    kp_removeDouble = false;
    kp_subPix = 0;
    kp_border = 0;
    detector_nr = 6; // orb
    extractor_nr = 6; // orb

}






void Detector::extract(const cv::Mat& image, KeyPoints &kps_inout, cv::Mat& descs_out, const double rotRad) const {


    // Deal with R-BRIEF pre rotation
    if ((extractor_nr == 203 || extractor_nr == 204 ||extractor_nr == 205 ) && std::abs(rotRad)>0.0017 ){
        (static_cast<cv::Ptr<cv::RBriefDescriptorExtractor> >(cv_extractor))->setRotationCase(rotRad); /// TODO check if should be negative or not
//shouldnt be dynamic cast?
    } else if(kp_imuRotate && std::abs(rotRad)<0.0001){

        for (uint i=0; i < kps_inout.size(); ++i){
            // PROBLEM: docs says angle is [-000,360) but sometimes it is [-180,180)
            kps_inout[i].angle = rotRad*toDeg;//cv::min(359.9,cv::max(0.,180 + rotRad*180./M_PI)) ;
        }
    }


    //ROS_INFO("DESCRIPTOR TYPE: %d   SIZE: %d", cv_extractor->descriptorType(), cv_extractor->descriptorSize());

    cv_extractor->compute(image, kps_inout, descs_out);

}




void Detector::detect(const cv::Mat& img, KeyPoints& kps_out, const cv::Mat& mask) const {
    kps_out.clear();

    // detector is off
    if (detector_nr<0){
        return;
    }

    //TODO This shouldnt happen but it does. FIXME
    if (cv_detector==0){
        ROS_ERROR("detector not instanciated...");
        return;
    }

    if (detector_nr>=100){
        ast_detect(ast_detector, img, kps_out);
        if (!mask.empty() ){
            kp_filter.runByPixelsMask(kps_out, mask);
        }
        if (kp_border){
            kp_filter.runByImageBorder(kps_out, img.size(), kp_border);
        }
    } else {
        cv_detector->detect(img, kps_out);
        if (!mask.empty() ){
            kp_filter.runByPixelsMask(kps_out, mask);
        }
        if (kp_border){
            kp_filter.runByImageBorder(kps_out, img.size(), kp_border);
        }


        // Threshold by score if possible. kp_thresh=0 means dont threshold, or we cannot
        /// TODO: best would be if one could put the thresh in the detector constructor...

        if (kp_thresh>0){

            // some have the threshold in their contstructors
            if (detector_nr!=20 && detector_nr>3
                    && detector_nr!=11 && detector_nr!=12
                    && detector_nr!=9 && detector_nr!=10
                    && detector_nr!=18 && detector_nr!=19
                    && (detector_nr>65 || detector_nr<30)){
                kp_threshold(kps_out, kp_thresh);
            }
        }

        // If we have too many, take X best by response. kp_max=0 do not threshold
        if (kp_max>0 && kps_out.size()>kp_max){
            //kp_clip(kps_out, kp_max);
            kp_filter.retainBest(kps_out, kp_max);

        }
    }

    if (kp_subPix){
        subPixKP(img, kps_out, kp_subPix);
    }

    if (kp_removeDouble){
        cv::KeyPointsFilter::removeDuplicated(kps_out);
    }



    uint total; double rmin; double rmax;
    minMaxTotal(kps_out, rmin, rmax, total);
    ROS_INFO("RESPONSE %g -> %g (%d) Thresh: %g", rmin, rmax, total, kp_thresh);


}

void Detector::ast_detect(cv::Ptr<agast::AstDetector> detecter, const cv::Mat& img, KeyPoints& keypoints, const cv::Mat& mask ) const{

    std::vector<CvPoint> points;
    detecter->set_imageSize(img.cols, img.rows);

    //TODO segfaults
    //detecter->set_threshold(kp_thresh);

    if (detector_nr>=104){
        detecter->processImage((unsigned char *)img.data, points);
    } else {
        detecter->detect((unsigned char *)img.data, points);
    }

    keypoints.clear();
    keypoints.reserve(points.size());
    for(uint i=0; i<points.size(); ++i) {
        keypoints.push_back(cv::KeyPoint(points[i],5));
    }

}


cv::Ptr<cv::Algorithm> Detector::getAlgo(const int id, const float thresh){
    cv::Ptr<cv::Algorithm> algo;
    // Create detector
    switch(id){
    case 0  : algo  = new cv::SURF(std::max(50.f,thresh), std::max(1, kp_octaves), std::max(1, kp_octaveLayers), true, false); break;
    case 1  : algo  = new cv::SURF(std::max(50.f,thresh), std::max(1, kp_octaves), std::max(1, kp_octaveLayers), false, false); break;
    case 2  : algo  = new cv::SURF(std::max(50.f,thresh), std::max(1, kp_octaves), std::max(1, kp_octaveLayers), true, true); break;
    case 3  : algo  = new cv::SURF(std::max(50.f,thresh), std::max(1, kp_octaves), std::max(1, kp_octaveLayers), false, true); break;
    case 4  : algo  = new cv::SIFT(kp_max ? kp_max : 2000, kp_octaveLayers+1, 0.04, 10, 1.6); break;
    case 5  : algo  = new cv::SIFT(kp_max ? kp_max : 2000, kp_octaveLayers+1, 0.04, 10, 0.8); break;
    case 6  : algo  = new cv::ORB(kp_max ? kp_max : 2000, 1.2, std::max(1, kp_octaves*2), 31, 0, 2, 0, 31); break;
    case 7  : algo  = new cv::ORB(kp_max ? kp_max : 2000, 1.2, std::max(1, kp_octaves*2), 31, 0, 3, 0, 31); break;
    case 8  : algo  = new cv::ORB(kp_max ? kp_max : 2000, 1.2, std::max(1, kp_octaves*2), 31, 0, 4, 0, 31); break;
    //case 9  : algo  = new cv::FastFeatureDetector(10, false); break;
    //case 10 : algo  = new cv::PyramidAdaptedFeatureDetector(new cv::FastFeatureDetector(10, false), std::max(1, kp_octaves)); break;
    case 11 : algo  = new cv::FastFeatureDetector(static_cast<int>(thresh), true); break;
    case 12 : algo  = new cv::PyramidAdaptedFeatureDetector(new cv::FastFeatureDetector(thresh, true), std::max(1, kp_octaves)); break;
    case 13 : algo  = new cv::GFTTDetector(kp_max ? kp_max : 2000, 0.01, 1, 3, false, 0.04); break;
    case 14 : algo  = new cv::PyramidAdaptedFeatureDetector(new cv::GFTTDetector(kp_max ? kp_max : 2000, 0.01, 1, 3, false, 0.04), std::max(1,kp_octaves)); break;
    case 15 : algo  = new cv::GFTTDetector(kp_max ? kp_max : 2000, 0.01, 1, 3, true, 0.04); break;
    case 16 : algo  = new cv::PyramidAdaptedFeatureDetector(new cv::GFTTDetector(kp_max ? kp_max : 2000, 0.01, 1, 3, true, 0.04), std::max(1, kp_octaves)); break;
    case 17 : algo  = new cv::MSER(5, 60, 14400, 0.25, 0.2, 200, 1.1, 0.003, 5); break;
    case 18 : algo  = new cv::StarDetector(45, static_cast<int>(thresh), 10, 8, 5); break;
    case 19 : algo  = new cv::PyramidAdaptedFeatureDetector(new cv::StarDetector(45, static_cast<int>(thresh), 10, 8, 5), std::max(1, kp_octaves)); break;
    case 20 : algo  = new cv::BRISK(thresh<0.001 ? 20 : static_cast<int>(thresh), kp_octaves, fmin(0.1,static_cast<float>(kp_octaveLayers)/2.f)); break; //HYDRO+






//int nfeatures = 500, int noctaves = DEFAULT_OCTAVE_MAX, int nlevels = DEFAULT_NSUBLEVELS,
//        float detectorThreshold = thresh,
//        int diffusivityType = DEFAULT_DIFFUSIVITY_TYPE,
//        int descriptorMode = DEFAULT_DESCRIPTOR,
//        int ldbSize = DEFAULT_LDB_DESCRIPTOR_SIZE,
//        int ldbChannels = DEFAULT_LDB_CHANNELS,
//        bool verbosity = DEFAULT_VERBOSITY


    case 30 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    DEFAULT_LDB_CHANNELS); break; // 486 3
    case 31 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    1); break; // 486 1
    case 32 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, 160,
                                    DEFAULT_LDB_CHANNELS); break; // 160 3
    case 33 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, 160,
                                    1); break; // 160 1
    case 34 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, 64,
                                    DEFAULT_LDB_CHANNELS); break; // 64 3
    case 35 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, DEFAULT_DIFFUSIVITY_TYPE,
                                    DEFAULT_DESCRIPTOR, 64,
                                1); break; // 64 1
    case 40 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    DEFAULT_LDB_CHANNELS); break; // 486 3
    case 41 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    1); break; // 486 1
    case 42 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, 160,
                                    DEFAULT_LDB_CHANNELS); break; // 160 3
    case 43 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, 160,
                                    1); break; // 160 1
    case 44 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, 64,
                                    DEFAULT_LDB_CHANNELS); break; // 64 3
    case 45 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 1,
                                    DEFAULT_DESCRIPTOR, 64,
                                    1); break; // 64 1
    case 50 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    DEFAULT_LDB_CHANNELS); break; // 486 3
    case 51 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    1); break; // 486 1
    case 52 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, 160,
                                    DEFAULT_LDB_CHANNELS); break; // 160 3
    case 53 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, 160,
                                    1); break; // 160 1
    case 54 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, 64,
                                    DEFAULT_LDB_CHANNELS); break; // 64 3
    case 55 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 2,
                                    DEFAULT_DESCRIPTOR, 64,
                                    1); break; // 64 1
    case 60 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    DEFAULT_LDB_CHANNELS); break; // 486 3
    case 61 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, DEFAULT_LDB_DESCRIPTOR_SIZE,
                                    1); break; // 486 1
    case 62 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, 160,
                                    DEFAULT_LDB_CHANNELS); break; // 160 3
    case 63 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, 160,
                                    1); break; // 160 1
    case 64 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, 64,
                                    DEFAULT_LDB_CHANNELS); break; // 64 3
    case 65 : algo  = new cv::AKAZE(kp_max ? kp_max : 2000, std::max(1, kp_octaves), std::max(1, kp_octaveLayers),
                                    thresh, 3,
                                    DEFAULT_DESCRIPTOR, 64,
                                    1); break; // 64 1

    case 200 : algo  = new cv::BriefDescriptorExtractor(16); break;
    case 201 : algo  = new cv::BriefDescriptorExtractor(32); break;
    case 202 : algo  = new cv::BriefDescriptorExtractor(64); break;
    case 203 : algo  = new cv::RBriefDescriptorExtractor(16); break;
    case 204 : algo  = new cv::RBriefDescriptorExtractor(32); break;
    case 205 : algo  = new cv::RBriefDescriptorExtractor(64); break;
    case 206 : algo  = new cv::FREAK(true, true, 22.0f, kp_octaves); break;
    case 207 : algo  = new cv::FREAK(false, true, 22.0f, kp_octaves);  break;//U-FREAK

    case -1: break; // off
    default:
        ROS_ERROR("ALGORITHM WITH ID <%d> DOES NOT EXIST", id);
        // should never happen
        break;
    }
    return algo;
}




void Detector::setParameter(ollieRosTools::VoNode_paramsConfig &config, uint32_t level){

    //////////////////////////////////////////////////////////////// DETECTOR
    kp_border = config.kp_border;
    kp_subPix = config.kp_subpix;
    kp_removeDouble = config.kp_removeDouble;
    kp_imuRotate = config.kp_imuRotate;
    double thresh = config.kp_thresh;
    // Need to normalise the threshold score 
    switch(config.detector){
    // SURF
    case 0:
    case 1:
    case 2:
    case 3:
        thresh*=5000;
        break;
    // SIFT
    case 4:
    case 5:
        thresh*=0.08;
        break;
    // ORB
    case 6:
    case 7:
    case 8:
        thresh*=0.004;
        break;
    // FAST
    case 9:
    case 10:
    case 11:
    case 12:
        //thresh*=125.;
            thresh*=50.f;
        break;
     // GFTT
    case 13:
    case 14:
        thresh = 0;
        break;
    // HARRIS
    case 15:
    case 16:
        thresh = 0;
        break;
    // MSER
    case 17:
        thresh = 0;
        break;
    // STAR
    case 18:
    case 19:
        //thresh*=90;
        thresh*=30;
        break;
    // BRISK
    case 20:
        thresh*=150;
        break;
    // AKAZE
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
        thresh*=0.015;
        break;
    // AGAST
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
        // SHould be anle to set threshold, but it doesnt work...
        thresh = 0;
        break;
    // None
    default:
        // should never happen
        break;
    }


    // IF something detector related changed, create new one
    if (detector_nr != config.detector ||
        static_cast<int>(kp_max) != config.kp_max ||
        kp_octaves != config.kp_octaves ||
        kp_octaveLayers != config.kp_octaveLayers ||
        kp_thresh != thresh ||
        cv_detector == 0
        ) {

        // Assign new values
        detector_nr = config.detector;
        kp_max = config.kp_max;
        kp_octaves = config.kp_octaves;
        kp_octaveLayers = config.kp_octaveLayers;
        kp_thresh = thresh;
        std::cout << "new thresh: "<<thresh <<std::endl;

        // Create detector
        switch(detector_nr){
            case 100:
            case 104: ast_detector = new agast::AgastDetector5_8();   break;
            case 101:
            case 105: ast_detector = new agast::AgastDetector7_12d(); break;
            case 102:
            case 106: ast_detector = new agast::AgastDetector7_12s(); break;
            case 103:
            case 107: ast_detector = new agast::OastDetector9_16();   break;
            default:
                cv_detector = getAlgo(detector_nr, thresh);
                break;
        }

    }

    //////////////////////////////////////////////////////////////// EXTRACTOR

    if(extractor_nr != config.extractor || cv_extractor == 0){
        extractor_nr = config.extractor;
        cv_extractor = getAlgo(extractor_nr, thresh);
    }

    // Cache to detect fugure differences
    config_pre = config;

}
