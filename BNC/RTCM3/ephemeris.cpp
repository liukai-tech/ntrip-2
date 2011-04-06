#include <math.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>

#include <newmatio.h>

#include "ephemeris.h"
#include "bncutils.h"
#include "timeutils.h"
#include "bnctime.h"

using namespace std;

#define PI          3.1415926535898
// Returns nearest integer value
////////////////////////////////////////////////////////////////////////////
static double NearestInt(double fl, double * remain)
{
  bool isneg = fl < 0.0;
  double intval;
  if(isneg) fl *= -1.0;
  intval = (double)((unsigned long)(fl+0.5));
  if(isneg) {fl *= -1.0; intval *= -1.0;}
  if(remain)
    *remain = fl-intval;
  return intval;
} /* NearestInt() */

// Returns CRC24
////////////////////////////////////////////////////////////////////////////
static unsigned long CRC24(long size, const unsigned char *buf)
{
  unsigned long crc = 0;
  int i;

  while(size--)
  {
    crc ^= (*buf++) << (16);
    for(i = 0; i < 8; i++)
    {
      crc <<= 1;
      if(crc & 0x1000000)
        crc ^= 0x01864cfb;
    }
  }
  return crc;
} /* CRC24 */

// 
////////////////////////////////////////////////////////////////////////////
bool t_eph::isNewerThan(const t_eph* eph) const {
  if (_GPSweek >  eph->_GPSweek ||
      (_GPSweek == eph->_GPSweek && _GPSweeks > eph->_GPSweeks)) {
    return true;
  }
  else {
    return false;
  }
}

// Set GPS Satellite Position
////////////////////////////////////////////////////////////////////////////
void t_ephGPS::set(const gpsephemeris* ee) {

  _prn = QString("G%1").arg(ee->satellite, 2, 10, QChar('0'));

  // TODO: check if following two lines are correct
  _GPSweek  = ee->GPSweek;
  _GPSweeks = ee->TOE;

  _TOW  = ee->TOW;
  _TOC  = ee->TOC;
  _TOE  = ee->TOE;
  _IODE = ee->IODE;             
  _IODC = ee->IODC;             

  _clock_bias      = ee->clock_bias     ;
  _clock_drift     = ee->clock_drift    ;
  _clock_driftrate = ee->clock_driftrate;

  _Crs      = ee->Crs;
  _Delta_n  = ee->Delta_n;
  _M0       = ee->M0;
  _Cuc      = ee->Cuc;
  _e        = ee->e;
  _Cus      = ee->Cus;
  _sqrt_A   = ee->sqrt_A;
  _Cic      = ee->Cic;
  _OMEGA0   = ee->OMEGA0;
  _Cis      = ee->Cis;
  _i0       = ee->i0;
  _Crc      = ee->Crc;
  _omega    = ee->omega;
  _OMEGADOT = ee->OMEGADOT;
  _IDOT     = ee->IDOT;

  _TGD      = ee->TGD;
}

// Compute GPS Satellite Position (virtual)
////////////////////////////////////////////////////////////////////////////
void t_ephGPS::position(int GPSweek, double GPSweeks, 
			double* xc,
			double* vv) const {

  static const double secPerWeek = 7 * 86400.0;
  static const double omegaEarth = 7292115.1467e-11;
  static const double gmWGS      = 398.6005e12;

  memset(xc, 0, 4*sizeof(double));
  memset(vv, 0, 3*sizeof(double));

  double a0 = _sqrt_A * _sqrt_A;
  if (a0 == 0) {
    return;
  }

  double n0 = sqrt(gmWGS/(a0*a0*a0));
  double tk = GPSweeks - _TOE;
  if (GPSweek != _GPSweek) {  
    tk += (GPSweek - _GPSweek) * secPerWeek;
  }
  double n  = n0 + _Delta_n;
  double M  = _M0 + n*tk;
  double E  = M;
  double E_last;
  do {
    E_last = E;
    E = M + _e*sin(E);
  } while ( fabs(E-E_last)*a0 > 0.001 );
  double v      = 2.0*atan( sqrt( (1.0 + _e)/(1.0 - _e) )*tan( E/2 ) );
  double u0     = v + _omega;
  double sin2u0 = sin(2*u0);
  double cos2u0 = cos(2*u0);
  double r      = a0*(1 - _e*cos(E)) + _Crc*cos2u0 + _Crs*sin2u0;
  double i      = _i0 + _IDOT*tk + _Cic*cos2u0 + _Cis*sin2u0;
  double u      = u0 + _Cuc*cos2u0 + _Cus*sin2u0;
  double xp     = r*cos(u);
  double yp     = r*sin(u);
  double OM     = _OMEGA0 + (_OMEGADOT - omegaEarth)*tk - 
                   omegaEarth*_TOE;
  
  double sinom = sin(OM);
  double cosom = cos(OM);
  double sini  = sin(i);
  double cosi  = cos(i);
  xc[0] = xp*cosom - yp*cosi*sinom;
  xc[1] = xp*sinom + yp*cosi*cosom;
  xc[2] = yp*sini;                 
  
  double tc = GPSweeks - _TOC;
  if (GPSweek != _GPSweek) {  
    tc += (GPSweek - _GPSweek) * secPerWeek;
  }
  xc[3] = _clock_bias + _clock_drift*tc + _clock_driftrate*tc*tc;

  // Velocity
  // --------
  double tanv2 = tan(v/2);
  double dEdM  = 1 / (1 - _e*cos(E));
  double dotv  = sqrt((1.0 + _e)/(1.0 - _e)) / cos(E/2)/cos(E/2) / (1 + tanv2*tanv2) 
               * dEdM * n;
  double dotu  = dotv + (-_Cuc*sin2u0 + _Cus*cos2u0)*2*dotv;
  double dotom = _OMEGADOT - omegaEarth;
  double doti  = _IDOT + (-_Cic*sin2u0 + _Cis*cos2u0)*2*dotv;
  double dotr  = a0 * _e*sin(E) * dEdM * n 
                + (-_Crc*sin2u0 + _Crs*cos2u0)*2*dotv;
  double dotx  = dotr*cos(u) - r*sin(u)*dotu;
  double doty  = dotr*sin(u) + r*cos(u)*dotu;

  vv[0]  = cosom   *dotx  - cosi*sinom   *doty      // dX / dr
           - xp*sinom*dotom - yp*cosi*cosom*dotom   // dX / dOMEGA
                       + yp*sini*sinom*doti;        // dX / di

  vv[1]  = sinom   *dotx  + cosi*cosom   *doty
           + xp*cosom*dotom - yp*cosi*sinom*dotom
                          - yp*sini*cosom*doti;

  vv[2]  = sini    *doty  + yp*cosi      *doti;

  // Relativistic Correction
  // -----------------------
  //  xc(4) -= 4.442807633e-10 * _e * sqrt(a0) *sin(E);
  xc[3] -= 2.0 * (xc[0]*vv[0] + xc[1]*vv[1] + xc[2]*vv[2]) / t_CST::c / t_CST::c;
}

// build up RTCM3 for GPS
////////////////////////////////////////////////////////////////////////////
#define GPSTOINT(type, value) static_cast<type>(NearestInt(value,0))

#define GPSADDBITS(a, b) {bitbuffer = (bitbuffer<<(a)) \
                       |(GPSTOINT(long long,b)&((1ULL<<a)-1)); \
                       numbits += (a); \
                       while(numbits >= 8) { \
                       buffer[size++] = bitbuffer>>(numbits-8);numbits -= 8;}}
#define GPSADDBITSFLOAT(a,b,c) {long long i = GPSTOINT(long long,(b)/(c)); \
                             GPSADDBITS(a,i)};

int t_ephGPS::RTCM3(unsigned char *buffer)
{

  unsigned char *startbuffer = buffer;
  buffer= buffer+3;
  int size = 0;
  int numbits = 0;
  unsigned long long bitbuffer = 0;
  if (_ura <= 2.40){
    _ura = 0;
  }
  else if (_ura <= 3.40){
    _ura = 1;
  }
  else if (_ura <= 6.85){
    _ura = 2;
  }
  else if (_ura <= 9.65){
    _ura = 3;
  }
  else if (_ura <= 13.65){
    _ura = 4;
  }
  else if (_ura <= 24.00){
    _ura = 5;
  }
  else if (_ura <= 48.00){
    _ura = 6;
  }
  else if (_ura <= 96.00){
    _ura = 7;
  }
  else if (_ura <= 192.00){
    _ura = 8;
  }
  else if (_ura <= 384.00){
    _ura = 9;
  }
  else if (_ura <= 768.00){
    _ura = 10;
  }
  else if (_ura <= 1536.00){
    _ura = 11;
  }
  else if (_ura <= 1536.00){
    _ura = 12;
  }
  else if (_ura <= 2072.00){
    _ura = 13;
  }
  else if (_ura <= 6144.00){
    _ura = 14;
  }
  else{
    _ura = 15;
  }

  GPSADDBITS(12, 1019)
  GPSADDBITS(6,_prn.right((_prn.length()-1)).toInt())
  GPSADDBITS(10, _GPSweek)
  GPSADDBITS(4, _ura)
  GPSADDBITS(2,_L2Codes)
  GPSADDBITSFLOAT(14, _IDOT, PI/static_cast<double>(1<<30)
  /static_cast<double>(1<<13))
  GPSADDBITS(8, _IODE)
  GPSADDBITS(16, static_cast<int>(_TOC)>>4)
  GPSADDBITSFLOAT(8, _clock_driftrate, 1.0/static_cast<double>(1<<30)
  /static_cast<double>(1<<25))
  GPSADDBITSFLOAT(16, _clock_drift, 1.0/static_cast<double>(1<<30)
  /static_cast<double>(1<<13))
  GPSADDBITSFLOAT(22, _clock_bias, 1.0/static_cast<double>(1<<30)
  /static_cast<double>(1<<1))
  GPSADDBITS(10, _IODC)
  GPSADDBITSFLOAT(16, _Crs, 1.0/static_cast<double>(1<<5))
  GPSADDBITSFLOAT(16, _Delta_n, PI/static_cast<double>(1<<30)
  /static_cast<double>(1<<13))
  GPSADDBITSFLOAT(32, _M0, PI/static_cast<double>(1<<30)/static_cast<double>(1<<1))
  GPSADDBITSFLOAT(16, _Cuc, 1.0/static_cast<double>(1<<29))
  GPSADDBITSFLOAT(32, _e, 1.0/static_cast<double>(1<<30)/static_cast<double>(1<<3))
  GPSADDBITSFLOAT(16, _Cus, 1.0/static_cast<double>(1<<29))
  GPSADDBITSFLOAT(32, _sqrt_A, 1.0/static_cast<double>(1<<19))
  GPSADDBITS(16, static_cast<int>(_TOE)>>4)
  GPSADDBITSFLOAT(16, _Cic, 1.0/static_cast<double>(1<<29))
  GPSADDBITSFLOAT(32, _OMEGA0, PI/static_cast<double>(1<<30)
  /static_cast<double>(1<<1))
  GPSADDBITSFLOAT(16, _Cis, 1.0/static_cast<double>(1<<29))
  GPSADDBITSFLOAT(32, _i0, PI/static_cast<double>(1<<30)/static_cast<double>(1<<1))
  GPSADDBITSFLOAT(16, _Crc, 1.0/static_cast<double>(1<<5))
  GPSADDBITSFLOAT(32, _omega, PI/static_cast<double>(1<<30)
  /static_cast<double>(1<<1))
  GPSADDBITSFLOAT(24, _OMEGADOT, PI/static_cast<double>(1<<30)
  /static_cast<double>(1<<13))
  GPSADDBITSFLOAT(8, _TGD, 1.0/static_cast<double>(1<<30)/static_cast<double>(1<<1))
  GPSADDBITS(6, _health) 
  GPSADDBITS(1, _L2PFlag)
  GPSADDBITS(1, 0) /* GPS fit interval */

  startbuffer[0]=0xD3;
  startbuffer[1]=(size >> 8);
  startbuffer[2]=size;
  unsigned long  i = CRC24(size+3, startbuffer);
  buffer[size++] = i >> 16;
  buffer[size++] = i >> 8;
  buffer[size++] = i;
  size += 3;
  return size;
}

// Derivative of the state vector using a simple force model (static)
////////////////////////////////////////////////////////////////////////////
ColumnVector t_ephGlo::glo_deriv(double /* tt */, const ColumnVector& xv,
                                 double* acc) {

  // State vector components
  // -----------------------
  ColumnVector rr = xv.rows(1,3);
  ColumnVector vv = xv.rows(4,6);

  // Acceleration 
  // ------------
  static const double GM    = 398.60044e12;
  static const double AE    = 6378136.0;
  static const double OMEGA = 7292115.e-11;
  static const double C20   = -1082.6257e-6;

  double rho = rr.norm_Frobenius();
  double t1  = -GM/(rho*rho*rho);
  double t2  = 3.0/2.0 * C20 * (GM*AE*AE) / (rho*rho*rho*rho*rho);
  double t3  = OMEGA * OMEGA;
  double t4  = 2.0 * OMEGA;
  double z2  = rr(3) * rr(3);

  // Vector of derivatives
  // ---------------------
  ColumnVector va(6);
  va(1) = vv(1);
  va(2) = vv(2);
  va(3) = vv(3);
  va(4) = (t1 + t2*(1.0-5.0*z2/(rho*rho)) + t3) * rr(1) + t4*vv(2) + acc[0]; 
  va(5) = (t1 + t2*(1.0-5.0*z2/(rho*rho)) + t3) * rr(2) - t4*vv(1) + acc[1]; 
  va(6) = (t1 + t2*(3.0-5.0*z2/(rho*rho))     ) * rr(3)            + acc[2];

  return va;
}

// Compute Glonass Satellite Position (virtual)
////////////////////////////////////////////////////////////////////////////
void t_ephGlo::position(int GPSweek, double GPSweeks, 
                        double* xc, double* vv) const {

  static const double secPerWeek  = 7 * 86400.0;
  static const double nominalStep = 10.0;

  memset(xc, 0, 4*sizeof(double));
  memset(vv, 0, 3*sizeof(double));

  double dtPos = GPSweeks - _tt;
  if (GPSweek != _GPSweek) {  
    dtPos += (GPSweek - _GPSweek) * secPerWeek;
  }

  int nSteps  = int(fabs(dtPos) / nominalStep) + 1;
  double step = dtPos / nSteps;

  double acc[3];
  acc[0] = _x_acceleration * 1.e3;
  acc[1] = _y_acceleration * 1.e3;
  acc[2] = _z_acceleration * 1.e3;
  for (int ii = 1; ii <= nSteps; ii++) { 
    _xv = rungeKutta4(_tt, _xv, step, acc, glo_deriv);
    _tt += step;
  }

  // Position and Velocity
  // ---------------------
  xc[0] = _xv(1);
  xc[1] = _xv(2);
  xc[2] = _xv(3);

  vv[0] = _xv(4);
  vv[1] = _xv(5);
  vv[2] = _xv(6);

  // Clock Correction
  // ----------------
  double dtClk = GPSweeks - _GPSweeks;
  if (GPSweek != _GPSweek) {  
    dtClk += (GPSweek - _GPSweek) * secPerWeek;
  }
  xc[3] = -_tau + _gamma * dtClk;
}

// IOD of Glonass Ephemeris (virtual)
////////////////////////////////////////////////////////////////////////////
int t_ephGlo::IOD() const {

  bool old = false;

  if (old) { // 5 LSBs of iod are equal to 5 LSBs of tb
    unsigned int tb  = int(fmod(_GPSweeks,86400.0)); //sec of day
    const int shift = sizeof(tb) * 8 - 5;
    unsigned int iod = tb << shift;
    return (iod >> shift);
  }
  else     {  
    bncTime tGPS(_GPSweek, _GPSweeks);
    int hlpWeek = _GPSweek;
    int hlpSec  = int(_GPSweeks);
    int hlpMsec = int(_GPSweeks * 1000);
    updatetime(&hlpWeek, &hlpSec, hlpMsec, 0);
    bncTime tHlp(hlpWeek, hlpSec);
    double diffSec = tGPS - tHlp;
    bncTime tMoscow = tGPS + diffSec;
    return int(tMoscow.daysec() / 900);
  }
}

// Set Glonass Ephemeris
////////////////////////////////////////////////////////////////////////////
void t_ephGlo::set(const glonassephemeris* ee) {

  _prn = QString("R%1").arg(ee->almanac_number, 2, 10, QChar('0'));

  int ww  = ee->GPSWeek;
  int tow = ee->GPSTOW; 
  updatetime(&ww, &tow, ee->tb*1000, 0);  // Moscow -> GPS

  // Check the day once more 
  // -----------------------
  {
    int    currentWeek;
    double currentSec;
    currentGPSWeeks(currentWeek, currentSec);
    bncTime currentTime(currentWeek, currentSec);
    bncTime hTime(ww, (double) tow);

    bool changed = false;
    if      (hTime - currentTime > 12 * 3600.0) {
      changed = true;
      hTime = hTime - 24 * 3600.0;
      ww  = hTime.gpsw();
      tow = (int) hTime.gpssec();
      updatetime(&ww, &tow, ee->tb*1000, 0);  // Moscow -> GPS
    }
    else if (hTime - currentTime < 12 * 3600.0) {
      changed = true;
      hTime = hTime + 24 * 3600.0;
      ww  = hTime.gpsw();
      tow = (int) hTime.gpssec();
      updatetime(&ww, &tow, ee->tb*1000, 0);  // Moscow -> GPS
    }

    if (changed) {
      bncTime newHTime(ww, (double) tow);
      cout << "GLONASS Time Changed: " << currentTime.timestr() << " " 
           << hTime.timestr() << " " << newHTime.timestr() << endl
           << ee->GPSWeek << " " << ee->GPSTOW << " " << ee->tb << endl
           << ww << " " << tow << endl;
    }
  }

  bncTime hlpTime(ww, (double) tow);
  unsigned year, month, day;
  hlpTime.civil_date(year, month, day);
  _gps_utc = gnumleap(year, month, day);

  _GPSweek           = ww;
  _GPSweeks          = tow;
  _E                 = ee->E;
  _tau               = ee->tau;
  _gamma             = ee->gamma;
  _x_pos             = ee->x_pos;
  _x_velocity        = ee->x_velocity;     
  _x_acceleration    = ee->x_acceleration;
  _y_pos             = ee->y_pos;         
  _y_velocity        = ee->y_velocity;    
  _y_acceleration    = ee->y_acceleration;
  _z_pos             = ee->z_pos;         
  _z_velocity        = ee->z_velocity;    
  _z_acceleration    = ee->z_acceleration;
  _health            = 0;
  _frequency_number  = ee->frequency_number;
  _tki               = ee->tk-3*60*60; if (_tki < 0) _tki += 86400;

  // Initialize status vector
  // ------------------------
  _tt = _GPSweeks;

  _xv(1) = _x_pos * 1.e3; 
  _xv(2) = _y_pos * 1.e3; 
  _xv(3) = _z_pos * 1.e3; 
  _xv(4) = _x_velocity * 1.e3; 
  _xv(5) = _y_velocity * 1.e3; 
  _xv(6) = _z_velocity * 1.e3; 
}

// build up RTCM3 for GLONASS
////////////////////////////////////////////////////////////////////////////
#define GLONASSTOINT(type, value) static_cast<type>(NearestInt(value,0))

#define GLONASSADDBITS(a, b) {bitbuffer = (bitbuffer<<(a)) \
                       |(GLONASSTOINT(long long,b)&((1ULL<<(a))-1)); \
                       numbits += (a); \
                       while(numbits >= 8) { \
                       buffer[size++] = bitbuffer>>(numbits-8);numbits -= 8;}}
#define GLONASSADDBITSFLOATM(a,b,c) {int s; long long i; \
                       if(b < 0.0) \
                       { \
                         s = 1; \
                         i = GLONASSTOINT(long long,(-b)/(c)); \
                         if(!i) s = 0; \
                       } \
                       else \
                       { \
                         s = 0; \
                         i = GLONASSTOINT(long long,(b)/(c)); \
                       } \
                       GLONASSADDBITS(1,s) \
                       GLONASSADDBITS(a-1,i)}

int t_ephGlo::RTCM3(unsigned char *buffer)
{

  int size = 0;
  int numbits = 0;
  long long bitbuffer = 0;
  unsigned char *startbuffer = buffer;
  buffer= buffer+3;

  GLONASSADDBITS(12, 1020)
  GLONASSADDBITS(6, _prn.right((_prn.length()-1)).toInt())
  GLONASSADDBITS(5, 7+_frequency_number)
  GLONASSADDBITS(1, 0)
  GLONASSADDBITS(1, 0)
  GLONASSADDBITS(2, 0)
  _tki=_tki+3*60*60;
  GLONASSADDBITS(5, static_cast<int>(_tki)/(60*60))
  GLONASSADDBITS(6, (static_cast<int>(_tki)/60)%60)
  GLONASSADDBITS(1, (static_cast<int>(_tki)/30)%30)
  GLONASSADDBITS(1, _health) 
  GLONASSADDBITS(1, 0)
  unsigned long long timeofday = (static_cast<int>(_tt+3*60*60-_gps_utc)%86400);
  GLONASSADDBITS(7, timeofday/60/15)
  GLONASSADDBITSFLOATM(24, _x_velocity*1000, 1000.0/static_cast<double>(1<<20))
  GLONASSADDBITSFLOATM(27, _x_pos*1000, 1000.0/static_cast<double>(1<<11))
  GLONASSADDBITSFLOATM(5, _x_acceleration*1000, 1000.0/static_cast<double>(1<<30))
  GLONASSADDBITSFLOATM(24, _y_velocity*1000, 1000.0/static_cast<double>(1<<20))
  GLONASSADDBITSFLOATM(27, _y_pos*1000, 1000.0/static_cast<double>(1<<11))
  GLONASSADDBITSFLOATM(5, _y_acceleration*1000, 1000.0/static_cast<double>(1<<30))
  GLONASSADDBITSFLOATM(24, _z_velocity*1000, 1000.0/static_cast<double>(1<<20))
  GLONASSADDBITSFLOATM(27,_z_pos*1000, 1000.0/static_cast<double>(1<<11))
  GLONASSADDBITSFLOATM(5, _z_acceleration*1000, 1000.0/static_cast<double>(1<<30))
  GLONASSADDBITS(1, 0)
  GLONASSADDBITSFLOATM(11, _gamma, 1.0/static_cast<double>(1<<30)
  /static_cast<double>(1<<10))
  GLONASSADDBITS(2, 0) /* GLONASS-M P */
  GLONASSADDBITS(1, 0) /* GLONASS-M ln(3) */
  GLONASSADDBITSFLOATM(22, _tau, 1.0/static_cast<double>(1<<30))
  GLONASSADDBITS(5, 0) /* GLONASS-M delta tau */
  GLONASSADDBITS(5, _E)
  GLONASSADDBITS(1, 0) /* GLONASS-M P4 */
  GLONASSADDBITS(4, 0) /* GLONASS-M FT */
  GLONASSADDBITS(11, 0) /* GLONASS-M NT */
  GLONASSADDBITS(2, 0) /* GLONASS-M active? */
  GLONASSADDBITS(1, 0) /* GLONASS additional data */
  GLONASSADDBITS(11, 0) /* GLONASS NA */
  GLONASSADDBITS(32, 0) /* GLONASS tau C */
  GLONASSADDBITS(5, 0) /* GLONASS-M N4 */
  GLONASSADDBITS(22, 0) /* GLONASS-M tau GPS */
  GLONASSADDBITS(1, 0) /* GLONASS-M ln(5) */
  GLONASSADDBITS(7, 0) /* Reserved */

  startbuffer[0]=0xD3;
  startbuffer[1]=(size >> 8);
  startbuffer[2]=size;
  unsigned long i = CRC24(size+3, startbuffer);
  buffer[size++] = i >> 16;
  buffer[size++] = i >> 8;
  buffer[size++] = i;
  size += 3;
  return size;
}

// Set Galileo Satellite Position
////////////////////////////////////////////////////////////////////////////
void t_ephGal::set(const galileoephemeris* ee) {

  _prn = QString("E%1").arg(ee->satellite, 2, 10, QChar('0'));

  _GPSweek  = ee->Week;
  _GPSweeks = ee->TOE;

  _TOC    = ee->TOC;
  _TOE    = ee->TOE;
  _IODnav = ee->IODnav;             

  _clock_bias      = ee->clock_bias     ;
  _clock_drift     = ee->clock_drift    ;
  _clock_driftrate = ee->clock_driftrate;

  _Crs      = ee->Crs;
  _Delta_n  = ee->Delta_n;
  _M0       = ee->M0;
  _Cuc      = ee->Cuc;
  _e        = ee->e;
  _Cus      = ee->Cus;
  _sqrt_A   = ee->sqrt_A;
  _Cic      = ee->Cic;
  _OMEGA0   = ee->OMEGA0;
  _Cis      = ee->Cis;
  _i0       = ee->i0;
  _Crc      = ee->Crc;
  _omega    = ee->omega;
  _OMEGADOT = ee->OMEGADOT;
  _IDOT     = ee->IDOT;
}

// Compute Galileo Satellite Position (virtual)
////////////////////////////////////////////////////////////////////////////
void t_ephGal::position(int GPSweek, double GPSweeks, 
			double* xc,
			double* vv) const {

  static const double secPerWeek = 7 * 86400.0;
  static const double omegaEarth = 7292115.1467e-11;
  static const double gmWGS      = 398.6005e12;

  memset(xc, 0, 4*sizeof(double));
  memset(vv, 0, 3*sizeof(double));

  double a0 = _sqrt_A * _sqrt_A;
  if (a0 == 0) {
    return;
  }

  double n0 = sqrt(gmWGS/(a0*a0*a0));
  double tk = GPSweeks - _TOE;
  if (GPSweek != _GPSweek) {  
    tk += (GPSweek - _GPSweek) * secPerWeek;
  }
  double n  = n0 + _Delta_n;
  double M  = _M0 + n*tk;
  double E  = M;
  double E_last;
  do {
    E_last = E;
    E = M + _e*sin(E);
  } while ( fabs(E-E_last)*a0 > 0.001 );
  double v      = 2.0*atan( sqrt( (1.0 + _e)/(1.0 - _e) )*tan( E/2 ) );
  double u0     = v + _omega;
  double sin2u0 = sin(2*u0);
  double cos2u0 = cos(2*u0);
  double r      = a0*(1 - _e*cos(E)) + _Crc*cos2u0 + _Crs*sin2u0;
  double i      = _i0 + _IDOT*tk + _Cic*cos2u0 + _Cis*sin2u0;
  double u      = u0 + _Cuc*cos2u0 + _Cus*sin2u0;
  double xp     = r*cos(u);
  double yp     = r*sin(u);
  double OM     = _OMEGA0 + (_OMEGADOT - omegaEarth)*tk - 
                   omegaEarth*_TOE;
  
  double sinom = sin(OM);
  double cosom = cos(OM);
  double sini  = sin(i);
  double cosi  = cos(i);
  xc[0] = xp*cosom - yp*cosi*sinom;
  xc[1] = xp*sinom + yp*cosi*cosom;
  xc[2] = yp*sini;                 
  
  double tc = GPSweeks - _TOC;
  if (GPSweek != _GPSweek) {  
    tc += (GPSweek - _GPSweek) * secPerWeek;
  }
  xc[3] = _clock_bias + _clock_drift*tc + _clock_driftrate*tc*tc;

  // Velocity
  // --------
  double tanv2 = tan(v/2);
  double dEdM  = 1 / (1 - _e*cos(E));
  double dotv  = sqrt((1.0 + _e)/(1.0 - _e)) / cos(E/2)/cos(E/2) / (1 + tanv2*tanv2) 
               * dEdM * n;
  double dotu  = dotv + (-_Cuc*sin2u0 + _Cus*cos2u0)*2*dotv;
  double dotom = _OMEGADOT - omegaEarth;
  double doti  = _IDOT + (-_Cic*sin2u0 + _Cis*cos2u0)*2*dotv;
  double dotr  = a0 * _e*sin(E) * dEdM * n 
                + (-_Crc*sin2u0 + _Crs*cos2u0)*2*dotv;
  double dotx  = dotr*cos(u) - r*sin(u)*dotu;
  double doty  = dotr*sin(u) + r*cos(u)*dotu;

  vv[0]  = cosom   *dotx  - cosi*sinom   *doty      // dX / dr
           - xp*sinom*dotom - yp*cosi*cosom*dotom   // dX / dOMEGA
                       + yp*sini*sinom*doti;        // dX / di

  vv[1]  = sinom   *dotx  + cosi*cosom   *doty
           + xp*cosom*dotom - yp*cosi*sinom*dotom
                          - yp*sini*cosom*doti;

  vv[2]  = sini    *doty  + yp*cosi      *doti;

  // Relativistic Correction
  // -----------------------
  //  xc(4) -= 4.442807633e-10 * _e * sqrt(a0) *sin(E);
  xc[3] -= 2.0 * (xc[0]*vv[0] + xc[1]*vv[1] + xc[2]*vv[2]) / t_CST::c / t_CST::c;
}

// build up RTCM3 for Galileo
////////////////////////////////////////////////////////////////////////////
int t_ephGal::RTCM3(unsigned char *buffer) {

  return 0;
}
