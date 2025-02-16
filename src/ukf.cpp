#include "ukf.h"
#include "Eigen/Dense"
#include<iostream>

using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;
  //use_laser_ = false;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);
  
  // initial covariance matrix
  P_ = MatrixXd(5, 5);
 
  		
  // Process noise standard deviation longitudinal acceleration in m/s^2
  //std_a_ = 30;
  std_a_ =3;

  // Process noise standard deviation yaw acceleration in rad/s^2
  //std_yawdd_ = 30;
  std_yawdd_ = 0.6;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */
  time_us_ = 0;
  is_initialized_=false;
  n_x_ = 5;
  n_aug_ = 7;
  Q = MatrixXd(2,2);
  Q<< std_a_*std_a_,0,
  		0		   ,std_yawdd_ *std_yawdd_ ;
  
  // measurement noise covariance matrix lidar
  R_lidr= MatrixXd(2,2);
  R_lidr<<  std_laspx_* std_laspx_,0,
  		  0,std_laspy_*std_laspy_;
  // measurement noise covariance matrix radar
  R_rdr= MatrixXd(3,3);
  R_rdr<< std_radr_*std_radr_,     0,0,
  		  0,std_radphi_*std_radphi_ ,0,
  		  0,0,  std_radrd_ *std_radrd_;
  H= MatrixXd(2,5);
  H<< 1,0,0,0,0,
  	  0,1,0,0,0;
  
  lambda_ = (3-n_aug_);
  weights_ = VectorXd(2*n_aug_+1);
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_[0]=weight_0;
  double weight = 0.5/(n_aug_+lambda_);
  for (int i=1; i<2*n_aug_+1; ++i) {  
    weights_(i) = weight;
  }
  Xsig_pred_ = MatrixXd(n_x_,2*n_aug_+1);
 
 
}

UKF::~UKF() {}

//************************************************ProcessMeasurement STARTS******************************************************************************

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */
  
   if (!is_initialized_) {
       if((meas_package.sensor_type_==MeasurementPackage::LASER))
    		{   
      			x_<<meas_package.raw_measurements_[0],
                meas_package.raw_measurements_[1],0,0,0;
             }
    	else if((meas_package.sensor_type_==MeasurementPackage::RADAR))
        {
           x_<< (meas_package.raw_measurements_[0])*cos(meas_package.raw_measurements_[1]),(meas_package.raw_measurements_[0])*sin(meas_package.raw_measurements_[1]),0,0,0;
         }  
     //diagonals of P_ as Variance  expected on X,Y,v,yaw and yawd
     P_<< 0.12,   0,    0,    0,    0,
  		     0, 0.21,   0,    0,    0,
  		     0,    0,   25,    0,    0,
  	 	     0,    0,    0, 0.02,    0,
  		     0,    0,    0,    0,  0.03;
     time_us_= meas_package.timestamp_;
     is_initialized_ = true;
     
     return;
    }
  
    double delta_t = (meas_package.timestamp_ - time_us_)/1000000.0;
        
    Prediction(delta_t);
    
    if(use_laser_&&(meas_package.sensor_type_==MeasurementPackage::LASER))
    		{UpdateLidar(meas_package);}
    else if(use_radar_&&(meas_package.sensor_type_==MeasurementPackage::RADAR))
        {UpdateRadar(meas_package);}

    time_us_= meas_package.timestamp_;

     return;
  
}
//**************************************************Prediction STARTS************************************************************************************

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */
  
  //************************************************************************************************************
   //**************************CREATING AUGMENTED MATRICES*******************************************************
   //************************************************************************************************************

  // create augmented mean vector
  VectorXd x_aug = VectorXd(7);

  // create augmented state covariance
  MatrixXd P_aug = MatrixXd(7, 7);

  // create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);
  // create augmented mean state*******************************

  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;
// create augmented covariance matrix***************************

  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug.bottomRightCorner(2,2) = Q;

  // create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

// create augmented sigma points******************************

  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; ++i) 
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
        
  }
  
//************************************************************************************************************
//**************************   PREDICT SIGMA POINTS    *******************************************************
//************************************************************************************************************
  
// predict sigma points
  for (int i = 0; i< 2*n_aug_+1; ++i) 
  { 
    // extract values for better readability
    double const  p_x  = Xsig_aug(0,i);
    double const p_y   = Xsig_aug(1,i);
    double const v     = Xsig_aug(2,i);
    double const yaw   = Xsig_aug(3,i);
    double const yawd  = Xsig_aug(4,i);
    double const nu_a  = Xsig_aug(5,i);
    double const nu_yawdd = Xsig_aug(6,i);

    // predicted state values*********************
    double px_p, py_p;

    // avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    } else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    // add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    // write predicted sigma point into right column
    Xsig_pred_(0,i) = px_p;
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p;
  }
 
//************************************************************************************************************
//**************************   FIND PREDICTED STATE MEAN x      **********************************************
//*************************  FIND PREDICTED STATE COVARIANCE P   *********************************************
//************************************************************************************************************
  
// predicted state mean
  VectorXd x = VectorXd(n_x_);
  x.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // iterate over sigma points
    x = x + weights_(i) * Xsig_pred_.col(i);
  }
  x_ = x;
  
  // predicted state covariance matrix
  MatrixXd P = MatrixXd(n_x_,n_x_);
  P.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // iterate over sigma points
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P = P + weights_(i) * x_diff * x_diff.transpose() ;
  }
  P_ =P;
  
  return;
}

//**************************************************UpdateLidar STARTS************************************************************************************

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
  // set measurement dimension, lidar 
   int n_z = 2;
  
  //*************************************************************************************************************
//************PROCESSING INPUT*********************************************************************************
//*************************************************************************************************************  
  
  // create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);

  z<< meas_package.raw_measurements_[0], 
      meas_package.raw_measurements_[1];
 
  
 //********************EKF IMPLEMENTATION ***************************************
  //**************************************************************************************************************
//************  FIND RESIDUAL y = z - H*x,             *****************************************************************
//************  FIND  R,S_ekf,KALMAN GAIN K **************************************************************
//************         UPDATE x_ and P_       ******************************************************************
//**************************************************************************************************************
  
  //residual
  VectorXd y = VectorXd(n_z);
  y = z - H*x_;
 
  
  //covariance matrx S_ekf
  MatrixXd S_ekf = MatrixXd(n_z,n_z);
  S_ekf = H*(P_)*(H.transpose()) + R_lidr;
    
  // Kalman gain K;
  MatrixXd K = (P_)*(H.transpose())*(S_ekf.inverse());
  MatrixXd I = MatrixXd::Identity(n_x_,n_x_);
  
  x_ = x_ + K * y;
  P_ = (I-K*H)*(P_);
 //* 
  for(int i =P_(0);i<P_.size();i++)
  { if(i%5==0)
     std::cout<<std::endl;
     std::cout<<P_(i)<<",";
  }
   std::cout<<std::endl;
 //*/
  
  return;

}

//**************************************************UpdateRadar STARTS************************************************************************************

void UKF::UpdateRadar(MeasurementPackage meas_package) {
 
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */
  
   //******************************************************************************************************
 //**********TRANSFORM SIGMA POINTS TO MEASUREMENT SPACE*************************************************
 //******************************************************************************************************

  // set measurement dimension, radar can measure r, phi, and r_dot
    int n_z = 3;
  
  // create matrix for sigma points in measurement space
  MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

  // mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  
  // measurement covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  
    // transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ +1; ++i) {  // 2n+1 simga points
    // extract values for better readability
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y);              // r
    Zsig(1,i) = atan2(p_y,p_x);                       // phi
    Zsig(2,i) = (p_x*v1 + p_y*v2) / sqrt(p_x*p_x + p_y*p_y);// r_dot
  }

//***********************************************************************************************************
//*************CALCULATING MEAN PREDICTED MEASUREMENT z_pred,****S and  R************************************
//***********************************************************************************************************

  // mean predicted measurement
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; ++i) {
    z_pred = z_pred + weights_(i) * Zsig.col(i);
  }

    // innovation covariance matrix S
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  // add measurement noise covariance matrix
  
  S = S + R_rdr;
  
//*************************************************************************************************************
//************PROCESSING INPUT*********************************************************************************
//*************************************************************************************************************   
 
  // create vector for incoming radar measurement
  VectorXd z = VectorXd(n_z);

  z<< meas_package.raw_measurements_[0], 
      meas_package.raw_measurements_[1],
  	  meas_package.raw_measurements_[2];
 
//**************************************************************************************************************
//************   FIND  CROSS CORRELATION MATRIX Tc   ***********************************************************
//**************************************************************************************************************
  
  // calculate cross correlation matrix 
  MatrixXd Tc = MatrixXd(n_x_, n_z);
  Tc.fill(0.0);
  
  for (int i = 0; i < 2 * n_aug_ + 1; ++i) {  // 2n+1 simga points
    // residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }


//************        FIND  KALMAN GAIN K      *****************************************************************
//************  FIND RESIDUAL z_diff = z - z_pred **************************************************************
//************         UPDATE x_ and P_       ******************************************************************
//**************************************************************************************************************

  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  // residual
  VectorXd z_diff = z - z_pred;

  // angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  // update state mean and covariance matrix
  
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();
  
 //* 
 for(int i =P_(0);i<P_.size();i++)
  { if(i%5==0)
     std::cout<<std::endl;
     std::cout<<P_(i)<<",";
  }
   std::cout<<std::endl;
   //*/
  return;
}
//***************************************EOF CODE ukf.cc ***************************************************************************************************
