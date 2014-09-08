
// Part of BNC, a utility for retrieving decoding and
// converting GNSS data streams from NTRIP broadcasters.
//
// Copyright (C) 2007
// German Federal Agency for Cartography and Geodesy (BKG)
// http://www.bkg.bund.de
// Czech Technical University Prague, Department of Geodesy
// http://www.fsv.cvut.cz
//
// Email: euref-ip@bkg.bund.de
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation, version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

/* -------------------------------------------------------------------------
 * BKG NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      t_astro, t_tides, t_tropo
 *
 * Purpose:    Observation model
 *
 * Author:     L. Mervart
 *
 * Created:    29-Jul-2014
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/


#include <cmath>

#include "pppModel.h"
#include "bncutils.h"

using namespace BNC_PPP;
using namespace std;

Matrix t_astro::rotX(double Angle) {
  const double C = cos(Angle);
  const double S = sin(Angle);
  Matrix UU(3,3);
  UU[0][0] = 1.0;  UU[0][1] = 0.0;  UU[0][2] = 0.0;
  UU[1][0] = 0.0;  UU[1][1] =  +C;  UU[1][2] =  +S;
  UU[2][0] = 0.0;  UU[2][1] =  -S;  UU[2][2] =  +C;
  return UU;
}

Matrix t_astro::rotY(double Angle) {
  const double C = cos(Angle);
  const double S = sin(Angle);
  Matrix UU(3,3);
  UU[0][0] =  +C;  UU[0][1] = 0.0;  UU[0][2] =  -S;
  UU[1][0] = 0.0;  UU[1][1] = 1.0;  UU[1][2] = 0.0;
  UU[2][0] =  +S;  UU[2][1] = 0.0;  UU[2][2] =  +C;
  return UU;
}

Matrix t_astro::rotZ(double Angle) {
  const double C = cos(Angle);
  const double S = sin(Angle);
  Matrix UU(3,3);
  UU[0][0] =  +C;  UU[0][1] =  +S;  UU[0][2] = 0.0;
  UU[1][0] =  -S;  UU[1][1] =  +C;  UU[1][2] = 0.0;
  UU[2][0] = 0.0;  UU[2][1] = 0.0;  UU[2][2] = 1.0;
  return UU;
}

// Greenwich Mean Sidereal Time
///////////////////////////////////////////////////////////////////////////
double t_astro::GMST(double Mjd_UT1) {

  const double Secs = 86400.0;

  double Mjd_0 = floor(Mjd_UT1);
  double UT1   = Secs*(Mjd_UT1-Mjd_0);
  double T_0   = (Mjd_0  -MJD_J2000)/36525.0; 
  double T     = (Mjd_UT1-MJD_J2000)/36525.0; 

  double gmst  = 24110.54841 + 8640184.812866*T_0 + 1.002737909350795*UT1
                 + (0.093104-6.2e-6*T)*T*T;

  return  2.0*M_PI*Frac(gmst/Secs);
}

// Nutation Matrix
///////////////////////////////////////////////////////////////////////////
Matrix t_astro::NutMatrix(double Mjd_TT) {

  const double T  = (Mjd_TT-MJD_J2000)/36525.0;

  double ls = 2.0*M_PI*Frac(0.993133+  99.997306*T);
  double D  = 2.0*M_PI*Frac(0.827362+1236.853087*T);
  double F  = 2.0*M_PI*Frac(0.259089+1342.227826*T);
  double N  = 2.0*M_PI*Frac(0.347346-   5.372447*T);

  double dpsi = ( -17.200*sin(N)   - 1.319*sin(2*(F-D+N)) - 0.227*sin(2*(F+N))
                + 0.206*sin(2*N) + 0.143*sin(ls) ) / RHO_SEC;
  double deps = ( + 9.203*cos(N)   + 0.574*cos(2*(F-D+N)) + 0.098*cos(2*(F+N))
                - 0.090*cos(2*N)                 ) / RHO_SEC;

  double eps  = 0.4090928-2.2696E-4*T;

  return  rotX(-eps-deps)*rotZ(-dpsi)*rotX(+eps);
}

// Precession Matrix
///////////////////////////////////////////////////////////////////////////
Matrix t_astro::PrecMatrix(double Mjd_1, double Mjd_2) {

  const double T  = (Mjd_1-MJD_J2000)/36525.0;
  const double dT = (Mjd_2-Mjd_1)/36525.0;
  
  double zeta  =  ( (2306.2181+(1.39656-0.000139*T)*T)+
                        ((0.30188-0.000344*T)+0.017998*dT)*dT )*dT/RHO_SEC;
  double z     =  zeta + ( (0.79280+0.000411*T)+0.000205*dT)*dT*dT/RHO_SEC;
  double theta =  ( (2004.3109-(0.85330+0.000217*T)*T)-
                        ((0.42665+0.000217*T)+0.041833*dT)*dT )*dT/RHO_SEC;

  return rotZ(-z) * rotY(theta) * rotZ(-zeta);
}    

// Sun's position
///////////////////////////////////////////////////////////////////////////
ColumnVector t_astro::Sun(double Mjd_TT) {

  const double eps = 23.43929111/RHO_DEG;
  const double T   = (Mjd_TT-MJD_J2000)/36525.0;

  double M = 2.0*M_PI * Frac ( 0.9931267 + 99.9973583*T);
  double L = 2.0*M_PI * Frac ( 0.7859444 + M/2.0/M_PI + 
                        (6892.0*sin(M)+72.0*sin(2.0*M)) / 1296.0e3);
  double r = 149.619e9 - 2.499e9*cos(M) - 0.021e9*cos(2*M);
  
  ColumnVector r_Sun(3); 
  r_Sun << r*cos(L) << r*sin(L) << 0.0; r_Sun = rotX(-eps) * r_Sun;

  return    rotZ(GMST(Mjd_TT))
          * NutMatrix(Mjd_TT) 
          * PrecMatrix(MJD_J2000, Mjd_TT)
          * r_Sun;
}

// Moon's position
///////////////////////////////////////////////////////////////////////////
ColumnVector t_astro::Moon(double Mjd_TT) {

  const double eps = 23.43929111/RHO_DEG;
  const double T   = (Mjd_TT-MJD_J2000)/36525.0;

  double L_0 = Frac ( 0.606433 + 1336.851344*T );
  double l   = 2.0*M_PI*Frac ( 0.374897 + 1325.552410*T );
  double lp  = 2.0*M_PI*Frac ( 0.993133 +   99.997361*T );
  double D   = 2.0*M_PI*Frac ( 0.827361 + 1236.853086*T );
  double F   = 2.0*M_PI*Frac ( 0.259086 + 1342.227825*T );
    
  double dL = +22640*sin(l) - 4586*sin(l-2*D) + 2370*sin(2*D) +  769*sin(2*l) 
              -668*sin(lp) - 412*sin(2*F) - 212*sin(2*l-2*D)- 206*sin(l+lp-2*D)
              +192*sin(l+2*D) - 165*sin(lp-2*D) - 125*sin(D) - 110*sin(l+lp)
              +148*sin(l-lp) - 55*sin(2*F-2*D);

  double L = 2.0*M_PI * Frac( L_0 + dL/1296.0e3 );

  double S  = F + (dL+412*sin(2*F)+541*sin(lp)) / RHO_SEC; 
  double h  = F-2*D;
  double N  = -526*sin(h) + 44*sin(l+h) - 31*sin(-l+h) - 23*sin(lp+h) 
              +11*sin(-lp+h) - 25*sin(-2*l+F) + 21*sin(-l+F);

  double B = ( 18520.0*sin(S) + N ) / RHO_SEC;
    
  double cosB = cos(B);

  double R = 385000e3 - 20905e3*cos(l) - 3699e3*cos(2*D-l) - 2956e3*cos(2*D)
      -570e3*cos(2*l) + 246e3*cos(2*l-2*D) - 205e3*cos(lp-2*D) 
      -171e3*cos(l+2*D) - 152e3*cos(l+lp-2*D);   

  ColumnVector r_Moon(3); 
  r_Moon << R*cos(L)*cosB << R*sin(L)*cosB << R*sin(B);
  r_Moon = rotX(-eps) * r_Moon;
    
  return    rotZ(GMST(Mjd_TT)) 
          * NutMatrix(Mjd_TT) 
          * PrecMatrix(MJD_J2000, Mjd_TT)
          * r_Moon;
}

// Tidal Correction 
////////////////////////////////////////////////////////////////////////////
ColumnVector t_tides::displacement(const bncTime& time, const ColumnVector& xyz) {

  if (time.undef()) {
    ColumnVector dX(3); dX = 0.0;
    return dX;
  }

  double Mjd = time.mjd() + time.daysec() / 86400.0;

  if (Mjd != _lastMjd) {
    _lastMjd = Mjd;
    _xSun = t_astro::Sun(Mjd);
    _rSun = sqrt(DotProduct(_xSun,_xSun));
    _xSun /= _rSun;
    _xMoon = t_astro::Moon(Mjd);
    _rMoon = sqrt(DotProduct(_xMoon,_xMoon));
    _xMoon /= _rMoon;
  }

  double       rRec    = sqrt(DotProduct(xyz, xyz));
  ColumnVector xyzUnit = xyz / rRec;

  // Love's Numbers
  // --------------
  const double H2 = 0.6078;
  const double L2 = 0.0847;

  // Tidal Displacement
  // ------------------
  double scSun  = DotProduct(xyzUnit, _xSun);
  double scMoon = DotProduct(xyzUnit, _xMoon);

  double p2Sun  = 3.0 * (H2/2.0-L2) * scSun  * scSun  - H2/2.0;
  double p2Moon = 3.0 * (H2/2.0-L2) * scMoon * scMoon - H2/2.0;

  double x2Sun  = 3.0 * L2 * scSun;
  double x2Moon = 3.0 * L2 * scMoon;
  
  const double gmWGS = 398.6005e12;
  const double gms   = 1.3271250e20;
  const double gmm   = 4.9027890e12;

  double facSun  = gms / gmWGS * 
                   (rRec * rRec * rRec * rRec) / (_rSun * _rSun * _rSun);

  double facMoon = gmm / gmWGS * 
                   (rRec * rRec * rRec * rRec) / (_rMoon * _rMoon * _rMoon);

  ColumnVector dX = facSun  * (x2Sun  * _xSun  + p2Sun  * xyzUnit) + 
                    facMoon * (x2Moon * _xMoon + p2Moon * xyzUnit);

  return dX;
}

// Constructor
///////////////////////////////////////////////////////////////////////////
t_windUp::t_windUp() {
  for (unsigned ii = 0; ii <= t_prn::MAXPRN; ii++) {
    sumWind[ii]   = 0.0;
    lastEtime[ii] = 0.0;
  }
}

// Phase Wind-Up Correction
///////////////////////////////////////////////////////////////////////////
double t_windUp::value(const bncTime& etime, const ColumnVector& rRec, 
                       t_prn prn, const ColumnVector& rSat) {

  if (etime.mjddec() != lastEtime[prn.toInt()]) {

    // Unit Vector GPS Satellite --> Receiver
    // --------------------------------------
    ColumnVector rho = rRec - rSat;
    rho /= rho.norm_Frobenius();
    
    // GPS Satellite unit Vectors sz, sy, sx
    // -------------------------------------
    ColumnVector sz = -rSat / rSat.norm_Frobenius();

    ColumnVector xSun = t_astro::Sun(etime.mjddec());
    xSun /= xSun.norm_Frobenius();

    ColumnVector sy = crossproduct(sz, xSun);
    ColumnVector sx = crossproduct(sy, sz);

    // Effective Dipole of the GPS Satellite Antenna
    // ---------------------------------------------
    ColumnVector dipSat = sx - rho * DotProduct(rho,sx) 
                                                - crossproduct(rho, sy);
    
    // Receiver unit Vectors rx, ry
    // ----------------------------
    ColumnVector rx(3);
    ColumnVector ry(3);

    double recEll[3]; xyz2ell(rRec.data(), recEll) ;
    double neu[3];
    
    neu[0] = 1.0;
    neu[1] = 0.0;
    neu[2] = 0.0;
    neu2xyz(recEll, neu, rx.data());
    
    neu[0] =  0.0;
    neu[1] = -1.0;
    neu[2] =  0.0;
    neu2xyz(recEll, neu, ry.data());
    
    // Effective Dipole of the Receiver Antenna
    // ----------------------------------------
    ColumnVector dipRec = rx - rho * DotProduct(rho,rx) 
                                                   + crossproduct(rho, ry);
    
    // Resulting Effect
    // ----------------
    double alpha = DotProduct(dipSat,dipRec) / 
                      (dipSat.norm_Frobenius() * dipRec.norm_Frobenius());
    
    if (alpha >  1.0) alpha =  1.0;
    if (alpha < -1.0) alpha = -1.0;
    
    double dphi = acos(alpha) / 2.0 / M_PI;  // in cycles
    
    if ( DotProduct(rho, crossproduct(dipSat, dipRec)) < 0.0 ) {
      dphi = -dphi;
    }

    if (lastEtime[prn.toInt()] == 0.0) {
      sumWind[prn.toInt()] = dphi;
    }
    else {
      sumWind[prn.toInt()] = nint(sumWind[prn.toInt()] - dphi) + dphi;
    }

    lastEtime[prn.toInt()] = etime.mjddec();
  }

  return sumWind[prn.toInt()];  
}

// Tropospheric Model (Saastamoinen)
////////////////////////////////////////////////////////////////////////////
double t_tropo::delay_saast(const ColumnVector& xyz, double Ele) {

  Tracer tracer("bncModel::delay_saast");

  if (xyz[0] == 0.0 && xyz[1] == 0.0 && xyz[2] == 0.0) {
    return 0.0;
  }

  double ell[3]; 
  xyz2ell(xyz.data(), ell);
  double height = ell[2];

  double pp =  1013.25 * pow(1.0 - 2.26e-5 * height, 5.225);
  double TT =  18.0 - height * 0.0065 + 273.15;
  double hh =  50.0 * exp(-6.396e-4 * height);
  double ee =  hh / 100.0 * exp(-37.2465 + 0.213166*TT - 0.000256908*TT*TT);

  double h_km = height / 1000.0;
  
  if (h_km < 0.0) h_km = 0.0;
  if (h_km > 5.0) h_km = 5.0;
  int    ii   = int(h_km + 1); if (ii > 5) ii = 5;
  double href = ii - 1;
  
  double bCor[6]; 
  bCor[0] = 1.156;
  bCor[1] = 1.006;
  bCor[2] = 0.874;
  bCor[3] = 0.757;
  bCor[4] = 0.654;
  bCor[5] = 0.563;
  
  double BB = bCor[ii-1] + (bCor[ii]-bCor[ii-1]) * (h_km - href);
  
  double zen  = M_PI/2.0 - Ele;

  return (0.002277/cos(zen)) * (pp + ((1255.0/TT)+0.05)*ee - BB*(tan(zen)*tan(zen)));
}

