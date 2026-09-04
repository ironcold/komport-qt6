/***************************************************************************
                          komportserial.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "komportserial.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

typedef struct tBaudTable {
      KomportSerial::Baud    nBaud;
      char*   sBaud;
};

tBaudTable BaudTable[] = {
      { KomportSerial::b50,      "50"    },
      { KomportSerial::b75,      "75"    },
      { KomportSerial::b110,      "110"    },
      { KomportSerial::b134,      "134"    },
      { KomportSerial::b150,      "150"    },
      { KomportSerial::b300,      "300"    },
      { KomportSerial::b600,      "600"    },
      { KomportSerial::b1200,      "1200"    },
      { KomportSerial::b1800,      "1800"    },
      { KomportSerial::b2400,      "2400"    },
      { KomportSerial::b4800,      "4800"    },
      { KomportSerial::b9600,      "9600"    },
      { KomportSerial::b19200,      "19200"    },
      { KomportSerial::b38400,      "38400"    },
      { KomportSerial::b57600,      "57600"    },
      { KomportSerial::b115200,      "115200"    },
      { KomportSerial::b230400,      "230400"    },
      { KomportSerial::b0,       "0"     }
};
    
KomportSerial::KomportSerial()
: mDeviceName("/dev/ttyS0")
, mFileNo(-1)
, mBaudRate(b9600)
, mSocketNotifier(NULL)
, mFlushRate(250)
{
    setFraming();
    QObject::connect(this,SIGNAL(settingsChanged()),this,SLOT(slotSettingsChanged()));
    mTimerId = startTimer( mFlushRate );
}

KomportSerial::~KomportSerial(){
  close();
}

/** Set the name of the serial port device (e.g. /dev/ttyxx). */
void KomportSerial::setDeviceName(QCString _dn){
  bool reset = ( _dn != mDeviceName ) && isOpen();
  if ( reset )
      close();
  mDeviceName=_dn;
  if ( reset )
      open();
}

/** Set the name of the serial port device (e.g. /dev/ttyxx). */
void KomportSerial::setDeviceName(QString _dn){
    setDeviceName( _dn.local8Bit() );
}

/** Return the name of the serial port device (e.g. /dev/ttyxx).
 */
QCString KomportSerial::deviceName(){
  return mDeviceName;
}

/** Open the serial port for communication. */
bool KomportSerial::open(){
  close();
  mFileNo = ::open( (const char*)mDeviceName, O_RDWR | O_NDELAY );
  if ( isOpen() ) {
      mSocketNotifier = new QSocketNotifier( mFileNo, QSocketNotifier::Read);
      QObject::connect(mSocketNotifier,SIGNAL(activated(int)),this,SLOT(slotDataAvailable()));
  }
  emit settingsChanged();
  return isOpen();
}

/** Return the file number (handle).
 */
int KomportSerial::handle(){
  return mFileNo;
}

/** Close the port. */
void KomportSerial::close(){
  if ( isOpen() ) {
    ::close(handle());
  }
  if ( mSocketNotifier != NULL ) {
      delete mSocketNotifier;
      mSocketNotifier=NULL;
  }
  mFileNo=(-1);
}

/** In the serial port open? */
bool KomportSerial::isOpen(){
  return mFileNo >=0 ;
}

/** Set the baud rate to one of the predevied enums. */
void KomportSerial::setBaudRate(Baud _baud){
  mBaudRate=_baud;
  emit settingsChanged();
}



/** Set the baud rate to one of the predevied enums. */
void KomportSerial::setBaudRate(QString _baud){
    for( int index=0; BaudTable[index].nBaud != b0; index++ ) {
        if ( strcmp( _baud.local8Bit(), BaudTable[index].sBaud ) == 0 ) {
            setBaudRate( BaudTable[index].nBaud );
            break;
        }
    }
}

/** Return the current baud rate setting. */
KomportSerial::Baud KomportSerial::baudRate() {
  return mBaudRate;
}

/** Commits settings changes to the serial port. */
void KomportSerial::slotSettingsChanged(){
  if ( isOpen() ) {
   speed_t speed;
   switch( mBaudRate ) {
     default:
     case  b0:       speed = B0;       break;
     case  b50:      speed = B50;      break;
     case  b75:      speed = B75;      break;
     case  b110:     speed = B110;     break;
     case  b134:     speed = B134;     break;
     case  b150:     speed = B150;     break;
     case  b300:     speed = B300;     break;
     case  b600:     speed = B600;     break;
     case  b1200:    speed = B1200;    break;
     case  b1800:    speed = B1800;    break;
     case  b2400:    speed = B2400;    break;
     case  b4800:    speed = B4800;    break;
     case  b9600:    speed = B9600;    break;
     case  b19200:   speed = B19200;   break;
     case  b38400:   speed = B38400;   break;
     case  b57600:   speed = B57600;   break;
     case  b115200:  speed = B115200;  break;
     case  b230400:  speed = B230400;  break;
   }
   struct termios tc;
   int rc = tcgetattr( handle(), &tc );
   if ( rc == 0 ) {
     // speed...
     tc.c_ispeed = speed;
     tc.c_ospeed = speed;
     // line control...
     tc.c_lflag &= ~ECHO;
     tc.c_lflag &= ~ICANON;
     // input control...
     tc.c_iflag &= ~ICRNL;
     tc.c_iflag &= ~INLCR;
     // ouput control...
     tc.c_oflag &= ~ONLCR;
     tc.c_oflag &= ~OCRNL;
     
     rc = tcsetattr( handle(), TCSANOW,  &tc );
   }
   if ( rc < 0 ) emit settingsFailed();
  }
}

/** put a character */
void KomportSerial::putChar(char _ch){
  char ch = (char)_ch;
  if(_ch) {
   while ( write( handle(), &ch, 1 ) < 0 ) {
     if ( errno == EAGAIN ) {
       perror("write");
       break;
     }
   }
  }
}

/** timer event */
void KomportSerial::timerEvent(QTimerEvent* _e){
  if ( mTimerId == _e->timerId() ) {
      while( !mRxQueue.empty() )  {
          emit receivedChar( mRxQueue.get() );
      }
  }
}

/** transmit a string */
void KomportSerial::putStr(const char* str){
  if ( NULL!=str ) {
    int len = strlen(str);
    for( int n=0; n < len; n++ ) {
      putChar(str[n]);
    }
  }
}

/** set size of RX queue */
int KomportSerial::setRxQueue(int _i){
    mRxQueue.setSize(_i);
    return mRxQueue.size();
}
/** socket notifier has detected data */
void KomportSerial::slotDataAvailable(){
    char ch;
    while( read( handle(), &ch, 1 ) == 1 ) {
        mRxQueue.put( ch );
    }
}
/** set the rate at which the Tx/Rx queue(s) are flushed */
void KomportSerial::setFlushRate(int _i){
    mFlushRate=_i;
    killTimer( mTimerId );
    mTimerId = startTimer( mFlushRate );
}
/** set the character framing */
void KomportSerial::setFraming( QString _startbits, QString _databits, QString _stopbits, QString _parity ){
    mStrStartBits = _startbits;
    mStrDataBits = _databits;
    mStrStopBits = _stopbits;
    mStrParity = _parity;    
}
