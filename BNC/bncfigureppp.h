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

#ifndef BNCFIGUREPPP_H
#define BNCFIGUREPPP_H

#include <QtGui>
#include "bnctime.h"

class bncFigurePPP : public QWidget {
  Q_OBJECT
 public:
  bncFigurePPP(QWidget *parent);
  ~bncFigurePPP();

 public slots:
  void slotNewPosition(bncTime time, double x, double y, double z);

 protected:
  void paintEvent(QPaintEvent *event);

 private:
  class pppPos {
   public:
    bncTime time;
    double  xyz[3];
  };

  QPoint pltPoint(double tt, double yy);

  QMutex           _mutex;
  QVector<pppPos*> _pos;
  double           _neuMax;
  double           _tRange;
  double           _tMin;
  int              _width;
  int              _height;
  double           _xyzRef[3];
  int              _daySec;
  int              _hours;
  int              _minutes;
  int              _seconds;
  QString          _strTic;
  const static int MAXNUMPOS = 300;
};

#endif
