/* -------------------------------------------------------------------------
 * BKG NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      bncSocket
 *
 * Purpose:    Combines QTcpSocket (NTRIP v1) and QHttp (NTRIP v2)
 *
 * Author:     L. Mervart
 *
 * Created:    27-Dec-2008
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>

#include "bncsocket.h"
#include "bncapp.h"

using namespace std;

#define BNCVERSION "1.7"

// Constructor
////////////////////////////////////////////////////////////////////////////
bncSocket::bncSocket() {
  bncApp* app = (bncApp*) qApp;
  app->connect(this, SIGNAL(newMessage(QByteArray,bool)), 
               app, SLOT(slotMessage(const QByteArray,bool)));
  _socket    = 0;
  _manager   = 0;
  _reply     = 0;
  _eventLoop = new QEventLoop();
  connect(this, SIGNAL(quitEventLoop()), _eventLoop, SLOT(quit()));
}

// Destructor
////////////////////////////////////////////////////////////////////////////
bncSocket::~bncSocket() {
  delete _eventLoop;
  delete _reply;
  delete _manager;
  delete _socket;
}

// 
////////////////////////////////////////////////////////////////////////////
QAbstractSocket::SocketState bncSocket::state() const {
  if      (_manager) {
    if (_reply) {
      return QAbstractSocket::ConnectedState;
    }
    else {
      return QAbstractSocket::UnconnectedState;
    }
  }
  else if (_socket) {
    return _socket->state();
  }
  else {
    return QAbstractSocket::UnconnectedState;
  }
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::close() {
  if (_socket) {
    _socket->close();
  }
}

// 
////////////////////////////////////////////////////////////////////////////
qint64 bncSocket::bytesAvailable() const {
  if      (_manager) { 
    return _buffer.size();
  }
  else if (_socket) {
    return _socket->bytesAvailable();
  }
  else {
    return 0;
  }
}

// 
////////////////////////////////////////////////////////////////////////////
bool bncSocket::canReadLine() const {
  if      (_manager) {
    if (_buffer.indexOf('\n') != -1) {
      return true;
    }
    else {
      return false;
    }
  }
  else if (_socket) {
    return _socket->canReadLine();
  }
  else {
    return false;
  }
}

// 
////////////////////////////////////////////////////////////////////////////
QByteArray bncSocket::readLine() {
  if      (_manager) {
    int ind = _buffer.indexOf('\n');
    if (ind != -1) {
      QByteArray ret = _buffer.left(ind+1);
      _buffer = _buffer.right(_buffer.size()-ind-1);
      return ret;
    }
    else {
      return "";
    }
  }
  else if (_socket) {
    return _socket->readLine();
  }
  else {
    return "";
  }
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::waitForReadyRead(int msecs) {
  if      (_manager) {
    if (bytesAvailable() > 0) {
      return;
    }
    else {
      _eventLoop->exec(QEventLoop::ExcludeUserInputEvents);
    }
  }
  else if (_socket) {
    _socket->waitForReadyRead(msecs);
  }
}

// 
////////////////////////////////////////////////////////////////////////////
QByteArray bncSocket::read(qint64 maxSize) {
  if      (_manager) {
    QByteArray ret = _buffer.left(maxSize);
    _buffer = _buffer.right(_buffer.size()-maxSize);
    return ret; 
  }
  else if (_socket) {
    return _socket->read(maxSize);
  }
  else {
    return "";
  }
}

// Connect to Caster, send the Request
////////////////////////////////////////////////////////////////////////////
t_irc bncSocket::request(const QUrl& mountPoint, const QByteArray& latitude, 
                         const QByteArray& longitude, const QByteArray& nmea,
                         const QByteArray& ntripVersion, 
                         int timeOut, QString& msg) {

  if      (ntripVersion == "AUTO") {
    emit newMessage("NTRIP Version AUTO not yet implemented", true);
    return failure;
  }
  else if (ntripVersion == "2") {
    return request2(mountPoint, latitude, longitude, nmea, timeOut, msg);
  }
  else if (ntripVersion != "1") {
    emit newMessage("Unknown NTRIP Version " + ntripVersion, true);
    return failure;
  }

  delete _socket;
  _socket = new QTcpSocket();

  // Connect the Socket
  // ------------------
  QSettings settings;
  QString proxyHost = settings.value("proxyHost").toString();
  int     proxyPort = settings.value("proxyPort").toInt();
 
  if ( proxyHost.isEmpty() ) {
    _socket->connectToHost(mountPoint.host(), mountPoint.port());
  }
  else {
    _socket->connectToHost(proxyHost, proxyPort);
  }
  if (!_socket->waitForConnected(timeOut)) {
    msg += "Connect timeout\n";
    delete _socket; 
    _socket = 0;
    return failure;
  }

  // Send Request
  // ------------
  QString uName = QUrl::fromPercentEncoding(mountPoint.userName().toAscii());
  QString passW = QUrl::fromPercentEncoding(mountPoint.password().toAscii());
  QByteArray userAndPwd;

  if(!uName.isEmpty() || !passW.isEmpty())
  {
    userAndPwd = "Authorization: Basic " + (uName.toAscii() + ":" +
    passW.toAscii()).toBase64() + "\r\n";
  }

  QUrl hlp;
  hlp.setScheme("http");
  hlp.setHost(mountPoint.host());
  hlp.setPort(mountPoint.port());
  hlp.setPath(mountPoint.path());

  QByteArray reqStr;
  if ( proxyHost.isEmpty() ) {
    if (hlp.path().indexOf("/") != 0) hlp.setPath("/");
    reqStr = "GET " + hlp.path().toAscii() + " HTTP/1.0\r\n"
             + "User-Agent: NTRIP BNC/" BNCVERSION "\r\n"
             + userAndPwd + "\r\n";
  } else {
    reqStr = "GET " + hlp.toEncoded() + " HTTP/1.0\r\n"
             + "User-Agent: NTRIP BNC/" BNCVERSION "\r\n"
             + "Host: " + hlp.host().toAscii() + "\r\n"
             + userAndPwd + "\r\n";
  }

  // NMEA string to handle VRS stream
  // --------------------------------
  double lat, lon;

  lat = strtod(latitude,NULL);
  lon = strtod(longitude,NULL);

  if ((nmea == "yes") && (hlp.path().length() > 2) && (hlp.path().indexOf(".skl") < 0)) {
    const char* flagN="N";
    const char* flagE="E";
    if (lon >180.) {lon=(lon-360.)*(-1.); flagE="W";}
    if ((lon < 0.) && (lon >= -180.))  {lon=lon*(-1.); flagE="W";}
    if (lon < -180.)  {lon=(lon+360.); flagE="E";}
    if (lat < 0.)  {lat=lat*(-1.); flagN="S";}
    QTime ttime(QDateTime::currentDateTime().toUTC().time());
    int lat_deg = (int)lat;  
    double lat_min=(lat-lat_deg)*60.;
    int lon_deg = (int)lon;  
    double lon_min=(lon-lon_deg)*60.;
    int hh = 0 , mm = 0;
    double ss = 0.0;
    hh=ttime.hour();
    mm=ttime.minute();
    ss=(double)ttime.second()+0.001*ttime.msec();
    QString gga;
    gga += "GPGGA,";
    gga += QString("%1%2%3,").arg((int)hh, 2, 10, QLatin1Char('0')).arg((int)mm, 2, 10, QLatin1Char('0')).arg((int)ss, 2, 10, QLatin1Char('0'));
    gga += QString("%1%2,").arg((int)lat_deg,2, 10, QLatin1Char('0')).arg(lat_min, 7, 'f', 4, QLatin1Char('0'));
    gga += flagN;
    gga += QString(",%1%2,").arg((int)lon_deg,3, 10, QLatin1Char('0')).arg(lon_min, 7, 'f', 4, QLatin1Char('0'));
    gga += flagE + QString(",1,05,1.00,+00100,M,10.000,M,,");
    int xori;
    char XOR = 0;
    char *Buff =gga.toAscii().data();
    int iLen = strlen(Buff);
    for (xori = 0; xori < iLen; xori++) {
      XOR ^= (char)Buff[xori];
    }
    gga += QString("*%1").arg(XOR, 2, 16, QLatin1Char('0'));
    reqStr += "$";
    reqStr += gga;
    reqStr += "\r\n";
  }

  msg += reqStr;

  _socket->write(reqStr, reqStr.length());

  if (!_socket->waitForBytesWritten(timeOut)) {
    msg += "Write timeout\n";
    delete _socket;
    _socket = 0;
    return failure;
  }

  return success;
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::slotReadyRead() {
  _buffer.append(_reply->readAll());
  emit quitEventLoop();
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::slotReplyFinished() {
  emit quitEventLoop();
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::slotError(QNetworkReply::NetworkError) {
  emit newMessage("slotError " + _reply->errorString().toAscii(), true);
}

// 
////////////////////////////////////////////////////////////////////////////
void bncSocket::slotSslErrors(const QList<QSslError>&) {
  emit newMessage("slotSslErrors", true);
}

// Connect to Caster NTRIP Version 2
////////////////////////////////////////////////////////////////////////////
t_irc bncSocket::request2(const QUrl& url, const QByteArray& latitude, 
                         const QByteArray& longitude, const QByteArray& nmea,
                         int timeOut, QString& msg) {

  // Network Access Manager
  // ----------------------
  if (_manager == 0) {
    _manager = new QNetworkAccessManager(this);
  }
  else {
    return failure;
  }

  // Default scheme and path
  // -----------------------
  QUrl urlLoc(url);
  if (urlLoc.scheme().isEmpty()) {
    urlLoc.setScheme("http");
  }
  if (urlLoc.path().isEmpty()) {
    urlLoc.setPath("/");
  }

  // Network Request
  // ---------------
  QNetworkRequest request;
  request.setUrl(urlLoc);
  request.setRawHeader("Host"         , urlLoc.host().toAscii());
  request.setRawHeader("Ntrip-Version", "NTRIP/2.0");
  request.setRawHeader("User-Agent"   , "NTRIP BNC/1.7");
  if (!urlLoc.userName().isEmpty()) {
    request.setRawHeader("Authorization", "Basic " + 
           (urlLoc.userName() + ":" + urlLoc.password()).toAscii().toBase64());
  } 
  request.setRawHeader("Connection"   , "close");

  _reply = _manager->get(request);

  connect(_reply, SIGNAL(finished()), this, SLOT(slotReplyFinished()));
  connect(_reply, SIGNAL(readyRead()), this, SLOT(slotReadyRead()));
  connect(_reply, SIGNAL(error(QNetworkReply::NetworkError)),
          this, SLOT(slotError(QNetworkReply::NetworkError)));
  connect(_reply, SIGNAL(sslErrors(const QList<QSslError>&)), 
          this, SLOT(slotSslErrors(const QList<QSslError>&)));


  return success;
}
