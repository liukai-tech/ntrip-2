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

#ifndef INC_RTCM2DECODER_H
#define INC_RTCM2DECODER_H

#include "GPSDecoder.h"
#include "RTCM2.h"

class RTCM2Decoder: public GPSDecoder {

  public:
    RTCM2Decoder();
    ~RTCM2Decoder();
    virtual void Decode(char* buffer, int bufLen);

  private:

    std::string        _buffer;
    rtcm2::RTCM2_Obs   _ObsBlock;
    rtcm2::RTCM2packet _PP;

};

#endif  // include blocker
