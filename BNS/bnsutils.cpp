/* -------------------------------------------------------------------------
 * BKG NTRIP Server
 * -------------------------------------------------------------------------
 *
 * Class:      bncutils
 *
 * Purpose:    Auxiliary Functions
 *
 * Author:     L. Mervart
 *
 * Created:    08-Apr-2008
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include <iostream>
#include <ctime>
#include <math.h>

#include <QRegExp>
#include <QStringList>
#include <QDateTime>

#include "bnsutils.h"
#include "bnseph.h"

using namespace std;

// 
////////////////////////////////////////////////////////////////////////////
void expandEnvVar(QString& str) {

  QRegExp rx("(\\$\\{.+\\})");

  if (rx.indexIn(str) != -1) {
    QStringListIterator it(rx.capturedTexts());
    if (it.hasNext()) {
      QString rxStr  = it.next();
      QString envVar = rxStr.mid(2,rxStr.length()-3);
      str.replace(rxStr, qgetenv(envVar.toAscii()));
    }
  }

}

// 
////////////////////////////////////////////////////////////////////////////
QDateTime dateAndTimeFromGPSweek(int GPSWeek, double GPSWeeks) {

  static const QDate zeroEpoch(1980, 1, 6);
 
  QDate date(zeroEpoch);
  QTime time(0,0,0,0);

  int weekDays = int(GPSWeeks) / 86400;
  date = date.addDays( GPSWeek * 7 + weekDays );
  time = time.addMSecs( int( (GPSWeeks - 86400 * weekDays) * 1e3 ) );

  return QDateTime(date,time);
}

// 
////////////////////////////////////////////////////////////////////////////
void GPSweekFromDateAndTime(const QDateTime& dateTime, 
                            int& GPSWeek, double& GPSWeeks) {

  static const QDateTime zeroEpoch(QDate(1980, 1, 6));
 
  GPSWeek = zeroEpoch.daysTo(dateTime) / 7;

  int weekDay = dateTime.date().dayOfWeek() + 1;  // Qt: Monday = 1
  if (weekDay > 7) weekDay = 1;

  GPSWeeks = (weekDay - 1) * 86400.0
             - dateTime.time().msecsTo(QTime()) / 1e3; 
}

// 
////////////////////////////////////////////////////////////////////////////
void currentGPSWeeks(int& week, double& sec) {

  QDateTime currDateTime = QDateTime::currentDateTime().toUTC();
  QDate     currDate = currDateTime.date();
  QTime     currTime = currDateTime.time();

  week = int( (double(currDate.toJulianDay()) - 2444244.5) / 7 );

  sec = (currDate.dayOfWeek() % 7) * 24.0 * 3600.0 + 
        currTime.hour()                   * 3600.0 + 
        currTime.minute()                 *   60.0 + 
        currTime.second()                          +
        currTime.msec()                   / 1000.0;
}

// 
////////////////////////////////////////////////////////////////////////////
void mjdFromDateAndTime(const QDateTime& dateTime, int& mjd, double& dayfrac) {

  const static QDate zeroDate(1858, 11, 17);

  mjd     = zeroDate.daysTo(dateTime.date());

  dayfrac = (dateTime.time().hour() +
             (dateTime.time().minute() +
              (dateTime.time().second() + 
               dateTime.time().msec() / 1000.0) / 60.0) / 60.0) / 24.0;
}

// Transformation xyz --> radial, along track, out-of-plane
////////////////////////////////////////////////////////////////////////////
void XYZ_to_RSW(const ColumnVector& rr, const ColumnVector& vv,
                const ColumnVector& xyz, ColumnVector& rsw) {

  ColumnVector cross = crossproduct(rr, vv);

  rsw.ReSize(3);
  rsw(1) = DotProduct(xyz, rr)    / rr.norm_Frobenius();
  rsw(2) = DotProduct(xyz, vv)    / vv.norm_Frobenius();
  rsw(3) = DotProduct(xyz, cross) / cross.norm_Frobenius();
}

// Fourth order Runge-Kutta numerical integrator for ODEs
////////////////////////////////////////////////////////////////////////////
ColumnVector rungeKutta4(
  double xi,              // the initial x-value
  const ColumnVector& yi, // vector of the initial y-values
  double dx,              // the step size for the integration
  ColumnVector (*der)(double x, const ColumnVector& y)
                          // A pointer to a function that computes the 
                          // derivative of a function at a point (x,y)
                         ) {

  ColumnVector k1 = der(xi       , yi       ) * dx;
  ColumnVector k2 = der(xi+dx/2.0, yi+k1/2.0) * dx;
  ColumnVector k3 = der(xi+dx/2.0, yi+k2/2.0) * dx;
  ColumnVector k4 = der(xi+dx    , yi+k3    ) * dx;

  ColumnVector yf = yi + k1/6.0 + k2/3.0 + k3/3.0 + k4/6.0;
  
  return yf;
}
