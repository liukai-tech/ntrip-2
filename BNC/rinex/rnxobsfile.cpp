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
 * Class:      t_rnxObsFile
 *
 * Purpose:    Reads RINEX Observation File
 *
 * Author:     L. Mervart
 *
 * Created:    24-Jan-2012
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>
#include <sstream>
#include "rinex/rnxobsfile.h"
#include "bncutils.h"

using namespace std;

const string t_rnxObsFile::t_rnxObsHeader::_emptyStr;

// Constructor
////////////////////////////////////////////////////////////////////////////
t_rnxObsFile::t_rnxObsHeader::t_rnxObsHeader() {
  _antNEU.ReSize(3); _antNEU = 0.0;
  _antXYZ.ReSize(3); _antXYZ = 0.0;
  _antBSG.ReSize(3); _antBSG = 0.0;
  _xyz.ReSize(3);    _xyz    = 0.0;
  _version  = 0.0;
  _interval = 0.0;
  for (unsigned iPrn = 1; iPrn <= MAXPRN_GPS; iPrn++) {
    _wlFactorsL1[iPrn] = 1;
    _wlFactorsL2[iPrn] = 1;
  }
}

// Destructor
////////////////////////////////////////////////////////////////////////////
t_rnxObsFile::t_rnxObsHeader::~t_rnxObsHeader() {
}

// Read Header
////////////////////////////////////////////////////////////////////////////
t_irc t_rnxObsFile::t_rnxObsHeader::read(ifstream* stream, int maxLines) {
  int numLines = 0;
  while ( stream->good() ) {
    string line;
    getline(*stream, line); ++numLines;
    if (line.empty()) {
      continue;
    }
    if (line.find("END OF FILE") != string::npos) {
      break;
    }
    string value = line.substr(0,60); stripWhiteSpace(value);
    string key   = line.substr(60);   stripWhiteSpace(key);
    if      (key == "END OF HEADER") {
      break;
    }
    else if (key == "RINEX VERSION / TYPE") {
      istringstream in(value);
      in >> _version;
    }
    else if (key == "MARKER NAME") {
      _markerName = value;
    }
    else if (key == "ANT # / TYPE") {
      _antennaName = line.substr(20,20); stripWhiteSpace(_antennaName);
    }
    else if (key == "INTERVAL") {
      istringstream in(value);
      in >> _interval;
    }
    else if (key == "WAVELENGTH FACT L1/2") {
      istringstream in(value);
      int wlFactL1 = 0;
      int wlFactL2 = 0;
      int numSat   = 0;
      in >> wlFactL1 >> wlFactL2 >> numSat;
      if (numSat == 0) {
        for (unsigned iPrn = 1; iPrn <= MAXPRN_GPS; iPrn++) {
          _wlFactorsL1[iPrn] = wlFactL1;
          _wlFactorsL2[iPrn] = wlFactL2;
        }
      }
      else {
        for (int ii = 0; ii < numSat; ii++) {
          string prn; in >> prn;
          if (prn[0] == 'G') {
            int iPrn;
            readInt(prn, 1, 2, iPrn);
            _wlFactorsL1[iPrn] = wlFactL1;
            _wlFactorsL2[iPrn] = wlFactL2;
          }
        }
      }
    }
    else if (key == "APPROX POSITION XYZ") {
      istringstream in(value);
      in >> _xyz[0] >> _xyz[1] >> _xyz[2];
    }
    else if (key == "ANTENNA: DELTA H/E/N") {
      istringstream in(value);
      in >> _antNEU[2] >> _antNEU[1] >> _antNEU[0];
    }
    else if (key == "ANTENNA: DELTA X/Y/Z") {
      istringstream in(value);
      in >> _antXYZ[0] >> _antXYZ[1] >> _antXYZ[2];
    }
    else if (key == "ANTENNA: B.SIGHT XYZ") {
      istringstream in(value);
      in >> _antBSG[0] >> _antBSG[1] >> _antBSG[2];
    }
    else if (key == "# / TYPES OF OBSERV") {
      istringstream in(value);
      int nTypes;
      in >> nTypes;
      _obsTypesV2.clear();
      for (int ii = 0; ii < nTypes; ii++) {
        string hlp;
        in >> hlp;
        _obsTypesV2.push_back(hlp);
      }
    }
    else if (key == "SYS / # / OBS TYPES") {
      istringstream* in = new istringstream(value);
      char sys;
      int nTypes;
      *in >> sys >> nTypes;
      _obsTypesV3[sys].clear();
      for (int ii = 0; ii < nTypes; ii++) {
        if (ii > 0 && ii % 13 == 0) {
          getline(*stream, line);  ++numLines;
          delete in;
          in = new istringstream(line);
        }
        string hlp;
        *in >> hlp;
        _obsTypesV3[sys].push_back(hlp);
      }
      delete in;
    }
    if (maxLines > 0 && numLines == maxLines) {
      break;
    }
  }

  return success;
}

// Number of Observation Types (satellite-system specific)
////////////////////////////////////////////////////////////////////////////
int t_rnxObsFile::t_rnxObsHeader::nTypes(char sys) const {
  if (_version < 3.0) {
    return _obsTypesV2.size();
  }
  else {
    map<char, vector<string> >::const_iterator it = _obsTypesV3.find(sys);
    if (it != _obsTypesV3.end()) {
      return it->second.size();
    }
    else {
      return 0;
    }
  }
}

// Observation Type (satellite-system specific)
////////////////////////////////////////////////////////////////////////////
const string& t_rnxObsFile::t_rnxObsHeader::obsType(char sys, int index) const {
  if (_version < 3.0) {
    return _obsTypesV2.at(index);
  }
  else {
    map<char, vector<string> >::const_iterator it = _obsTypesV3.find(sys);
    if (it != _obsTypesV3.end()) {
      return it->second.at(index);
    }
    else {
      return _emptyStr;
    }
  }
}

// Constructor
////////////////////////////////////////////////////////////////////////////
t_rnxObsFile::t_rnxObsFile(const string& fileName) {
  _stream       = 0;
  _flgPowerFail = false;
  open(fileName);
}

// Open
////////////////////////////////////////////////////////////////////////////
void t_rnxObsFile::open(const string& fileName) {
  _stream = new ifstream(expandEnvVar(fileName).c_str());
  _header.read(_stream);

  // Guess Observation Interval
  // --------------------------
  if (_header._interval == 0.0) {
    bncTime ttPrev;
    for (int iEpo = 0; iEpo < 10; iEpo++) {
      const t_rnxEpo* rnxEpo = nextEpoch();
      if (!rnxEpo) {
        throw string("t_rnxObsFile: not enough epochs");
      }
      if (iEpo > 0) {
        double dt = rnxEpo->tt - ttPrev;
        if (_header._interval == 0.0 || dt < _header._interval) {
          _header._interval = dt;
        }
      }
      ttPrev = rnxEpo->tt;
    }
    _stream->clear();
    _stream->seekg(0, ios::beg);
    _header.read(_stream);
  }
}

// Destructor
////////////////////////////////////////////////////////////////////////////
t_rnxObsFile::~t_rnxObsFile() {
  close();
}

// Close
////////////////////////////////////////////////////////////////////////////
void t_rnxObsFile::close() {
  delete _stream; _stream = 0;
}

// Handle Special Epoch Flag
////////////////////////////////////////////////////////////////////////////
void t_rnxObsFile::handleEpochFlag(int flag, const string& line) {

  // Power Failure
  // -------------
  if      (flag == 1) {
    _flgPowerFail = true;
  }

  // Start moving antenna
  // --------------------
  else if (flag == 2) {
    // no action
  }

  // Re-Read Header
  // -------------- 
  else if (flag == 3 || flag == 4) {
    int numLines = 0;
    if (version() < 3.0) {
      readInt(line, 29, 3, numLines);
    }
    else {
      readInt(line, 32, 3, numLines);
    }
    _header.read(_stream, numLines);
  }

  // Unhandled Flag
  // --------------
  else {
    throw string("t_rnxObsFile: unhandled flag\n" + line);
  }
}

// Retrieve single Epoch
////////////////////////////////////////////////////////////////////////////
const t_rnxObsFile::t_rnxEpo* t_rnxObsFile::nextEpoch() {

  _currEpo.clear();

  if (version() < 3.0) {
    return nextEpochV2();
  }
  else {
    return nextEpochV3();
  }
}

// Retrieve single Epoch (RINEX Version 3)
////////////////////////////////////////////////////////////////////////////
const t_rnxObsFile::t_rnxEpo* t_rnxObsFile::nextEpochV3() {

  while ( _stream->good() ) {

    string line;

    getline(*_stream, line);

    if (line.empty()) {
      continue;
    }

    int flag = 0;
    readInt(line, 31, 1, flag);
    if (flag > 0) {
      handleEpochFlag(flag, line);
      continue;
    }

    istringstream in(line.substr(1));

    // Epoch Time
    // ----------
    int    year, month, day, hour, min;
    double sec;
    in >> year >> month >> day >> hour >> min >> sec;
    _currEpo.tt.set(hour, min, sec, day, month, year);

    // Number of Satellites
    // --------------------
    int numSat;
    readInt(line, 32, 3, numSat);
  
    _currEpo.rnxSat.resize(numSat);

    // Observations
    // ------------
    for (int iSat = 0; iSat < numSat; iSat++) {
      getline(*_stream, line);
      _currEpo.rnxSat[iSat].satSys = line[0];
      readInt(line, 1, 2, _currEpo.rnxSat[iSat].satNum);
      char sys = line[0];
      for (int iType = 0; iType < _header.nTypes(sys); iType++) {
        int pos = 3 + 16*iType;
        double obsValue = 0.0;
        int    lli      = 0; 
        int    snr      = 0;
        readDbl(line, pos,     14, obsValue);
        readInt(line, pos + 14, 1, lli);
        readInt(line, pos + 15, 1, snr);

        if (_flgPowerFail) {
          lli |= 1;
        }

        _currEpo.rnxSat[iSat].obs.push_back(obsValue);
        _currEpo.rnxSat[iSat].lli.push_back(lli);
        _currEpo.rnxSat[iSat].snr.push_back(snr);
      }
    }

    _flgPowerFail = false;

    return &_currEpo;
  }

  return 0;
}

// Retrieve single Epoch (RINEX Version 2)
////////////////////////////////////////////////////////////////////////////
const t_rnxObsFile::t_rnxEpo* t_rnxObsFile::nextEpochV2() {

  while ( _stream->good() ) {

    string line;

    getline(*_stream, line);

    if (line.empty()) {
      continue;
    }

    int flag = 0;
    readInt(line, 28, 1, flag);
    if (flag > 0) {
      handleEpochFlag(flag, line);
      continue;
    }

    istringstream in(line);

    // Epoch Time
    // ----------
    int    year, month, day, hour, min;
    double sec;
    in >> year >> month >> day >> hour >> min >> sec;
    if      (year <  80) {
      year += 2000;
    }
    else if (year < 100) {
      year += 1900;
    }
    _currEpo.tt.set(hour, min, sec, day, month, year);

    // Number of Satellites
    // --------------------
    int numSat;
    readInt(line, 29, 3, numSat);
  
    _currEpo.rnxSat.resize(numSat);

    // Read Satellite Numbers
    // ----------------------
    int pos = 32;
    for (int iSat = 0; iSat < numSat; iSat++) {
      if (iSat > 0 && iSat % 12 == 0) {
        getline(*_stream, line);
        pos = 32;
      }

      _currEpo.rnxSat[iSat].satSys = line[pos];
      readInt(line, pos + 1, 2, _currEpo.rnxSat[iSat].satNum);

      pos += 3;
    }

    // Read Observation Records
    // ------------------------
    for (int iSat = 0; iSat < numSat; iSat++) {
      getline(*_stream, line);
      pos  = 0;
      for (int iType = 0; iType < _header.nTypes(_currEpo.rnxSat[iSat].satSys); iType++) {
        if (iType > 0 && iType % 5 == 0) {
          getline(*_stream, line);
          pos  = 0;
        }
        double obsValue = 0.0;
        int    lli      = 0;
        int    snr      = 0;
        readDbl(line, pos,     14, obsValue);
        readInt(line, pos + 14, 1, lli);
        readInt(line, pos + 15, 1, snr);

        if (_flgPowerFail) {
          lli |= 1;
        }

        _currEpo.rnxSat[iSat].obs.push_back(obsValue);
        _currEpo.rnxSat[iSat].lli.push_back(lli);
        _currEpo.rnxSat[iSat].snr.push_back(snr);

        pos += 16;
      }
    }
 
    _flgPowerFail = false;

    return &_currEpo;
  }
 
  return 0;
}

