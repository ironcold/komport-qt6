/***************************************************************************
                          komportdoc.cpp  -  Komport Serial Port Communicator
                             -------------------
    begin                : Mon Feb 17 2003
    copyright            : (C) 2003 by Mike Sharkey
    email                : michael@sharkey.servebeer.com
    ported to Qt6         : 2026
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// include files for Qt
#include <QApplication>
#include <QPointer>
#include <QDir>
#include <QWidget>
#include <QMessageBox>
#include <QFileInfo>

// application specific includes
#include "komportdoc.h"
#include "komport.h"
#include "komportview.h"

QList<KomportView*> *KomportDoc::pViewList = nullptr;

KomportDoc::KomportDoc(QObject *parent) : QObject(parent)
, modified(false)
{
  if(!pViewList)
  {
    pViewList = new QList<KomportView*>();
  }
  // SPEC-M8 6.2/6.3: the document owns the session controller that binds the
  // serial transport to a session. The session starts with the document and ends
  // with it - newDocument() does not replace the transport or the controller.
  // The controller observes the transport through its signals only; it neither
  // owns nor calls it, and it is destroyed before the transport below.
  mSessionController = std::make_unique<SessionController>(&mSerial);
  // SPEC-M9 5.8: the document also owns the recorder that consumes the
  // controller's event stream. It is created after the controller, because it
  // observes it, and destroyed before it (see the destructor).
  mSessionRecorder = std::make_unique<SessionRecorder>(mSessionController.get());
  // Note: unlike the original Qt3 QPtrList, this list does not own/delete
  // the views it tracks - it is only used to broadcast repaints across all
  // open windows (see slotUpdateAllViews()). Views are owned as normal
  // QWidget children and destroyed with their window.
}

KomportDoc::~KomportDoc()
{
  // SPEC-M8 6.2: the controller is destroyed before the transport, and its
  // destruction neither emits an event nor calls into the transport. Resetting
  // it here (rather than relying on member or QObject-child order) is what makes
  // that ordering explicit, because mSerial is a by-value member of this object.
  mSessionController.reset();
  // SPEC-M9 5.8: then the recorder, whose destruction flushes and closes a running
  // recording silently - no event, no report. Reaching this point is not an error
  // and not a notification: the application's close path stops the recorder
  // explicitly (KomportApp::closeEvent()), and this is only the safety net.
  mSessionRecorder.reset();
  // SPEC-M10 5.9: then the replay player and the session it shares, before the
  // transport. The player is closed and detached here, in order; only its memory is
  // released later (see retireReplayPlayer()).
  retireReplayPlayer(std::move(mReplayPlayer), true);
  mOfflineSession.reset();
}

void KomportDoc::addView(KomportView *view)
{
  pViewList->append(view);
  QObject::connect(view,SIGNAL(viewModified(KomportView*)),this,SLOT(slotViewModified(KomportView*)));
}

void KomportDoc::removeView(KomportView *view)
{
  pViewList->removeAll(view);
}
void KomportDoc::setURL(const QUrl &url)
{
  doc_url=url;
}

const QUrl& KomportDoc::URL() const
{
  return doc_url;
}

/** display name of the document */
QString KomportDoc::fileName() const
{
  QString name = QFileInfo( doc_url.toLocalFile() ).fileName();
  return name.isEmpty() ? QStringLiteral("Untitled") : name;
}

void KomportDoc::slotUpdateAllViews(KomportView *sender)
{
  if(pViewList)
  {
    for( KomportView *w : std::as_const(*pViewList) )
    {
      if(w!=sender)
        w->repaint();
    }
  }

}

bool KomportDoc::saveModified()
{
  bool completed=true;

  if(modified)
  {
    KomportApp *win = qobject_cast<KomportApp *>(parent());
    QMessageBox::StandardButton want_save = QMessageBox::warning(win,
                                         tr("Warning"),
                                         tr("The current file has been modified.\n"
                                              "Do you want to save it?"),
                                         QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    switch(want_save)
    {
      case QMessageBox::Yes:
           if (fileName() == QLatin1String("untitled.kom"))
           {
             win->slotFileSaveAs();
           }
           else
           {
             saveDocument(URL());
       	   };

       	   deleteContents();
           completed=true;
           break;

      case QMessageBox::No:
           setModified(false);
           deleteContents();
           completed=true;
           break;

      default:
           completed=false;
           break;
    }
  }

  return completed;
}

void KomportDoc::closeDocument()
{
  deleteContents();
}

bool KomportDoc::newDocument()
{
  /////////////////////////////////////////////////
  // TODO: Add your document initialization code here
  /////////////////////////////////////////////////
  doc_url = QUrl::fromLocalFile( QStringLiteral("untitled.kom") );

  // Deliberately NOT setModified(true) here: this used to mark every fresh
  // window as "modified" from the moment it was created, and nothing else
  // in this codebase ever clears it back to false except answering "No" in
  // saveModified()'s dialog - so closing *any* window (File > Close, File >
  // Quit, or just the window manager's close button), even one that never
  // received a byte, popped a blocking "The current file has been modified.
  // Do you want to save it?" prompt. That made sense in the original
  // KDevelop Document/View template this class is based on (a real
  // document that starts dirty until first saved), but komportdoc's
  // save/open document methods are permanently unimplemented TODO stubs
  // (see CLAUDE.md) - there is no real "document" to protect here, only a
  // spurious modal on every close.
  modified=false;

  return true;
}

bool KomportDoc::openDocument(const QUrl& url)
{
  Q_UNUSED(url);
  /////////////////////////////////////////////////
  // TODO: Add your document opening code here
  // (this was already an empty stub in the original KDE3 version - no file
  // content was ever actually read here, so nothing was lost by dropping
  // the KIO::NetAccess remote-download plumbing that used to wrap it)
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

bool KomportDoc::saveDocument(const QUrl& url)
{
  Q_UNUSED(url);
  /////////////////////////////////////////////////
  // TODO: Add your document saving code here
  /////////////////////////////////////////////////

  modified=false;
  return true;
}

void KomportDoc::deleteContents()
{
  /////////////////////////////////////////////////
  // TODO: Add implementation to delete the document contents
  /////////////////////////////////////////////////

}

void KomportDoc::setModified(bool _m)
{
  bool changed = (_m!=modified);
  modified=_m;
  if ( changed && modified ) {
    emit documentModified();
  }
};
/** get the serial port */
KomportSerial* KomportDoc::getSerial(){
  return &mSerial;
}
/** the document-owned session controller (SPEC-M8 6.2) */
SessionController* KomportDoc::getSessionController(){
  return mSessionController.get();
}

SessionRecorder* KomportDoc::getSessionRecorder(){
  return mSessionRecorder.get();
}

SessionRecordingReport KomportDoc::closeSession()
{
  // SPEC-M9 5.8, normative close ordering: the transport is closed *first*, while
  // the controller and the recorder are still alive, so the terminal
  // `TransportClosed` event is still recorded. Only then is a running recording
  // finalised (a last flush, a close and a report). Doing it the other way round
  // would lose that final event from the file.
  mSerial.close();
  if ( mSessionRecorder->state() == SessionRecorder::State::Stopped )
    return SessionRecordingReport();
  return mSessionRecorder->stop();
}
void KomportDoc::retireReplayPlayer(std::unique_ptr<SessionReplayPlayer> player, bool silent)
{
  // The player may be executing: a receiver of one of its synchronous signals
  // (stateChanged, eventDelivered, replayFinished) can reach the document's close
  // path or its destructor, and the player's own call resumes after the signal
  // returns. SessionReplayPlayer::retire() (SPEC-M10 5.11 rule 11) therefore ends
  // the player's life: synchronously when none of its calls is on the stack, else
  // when the outermost returns. @p silent - the destructor's case, which emits
  // nothing - drops every receiver first.
  if ( !player )
    return;
  if ( silent )
    player->disconnect();
  SessionReplayPlayer::retire(std::move(player));
}

bool KomportDoc::hasOfflineSession() const
{
  return mOfflineSession != nullptr;
}

SessionReplayPlayer* KomportDoc::getReplayPlayer() const
{
  return mReplayPlayer.get();
}

SessionFilePtr KomportDoc::offlineSession() const
{
  return mOfflineSession;
}

namespace {
/** This document plus the document of every open KomportApp window, without
  * duplicates (SPEC-M10 5.7: the offline state is process-wide, ADR-011 D11). The
  * walk is the one slotFileQuit() uses; @p self is always included so a document
  * without a window (the tests) is checked too. */
QList<const KomportDoc*> allOpenDocuments(const KomportDoc *self)
{
  QList<const KomportDoc*> documents;
  documents.append(self);
  const QWidgetList windows = QApplication::topLevelWidgets();
  for ( QWidget *w : windows ) {
    if ( auto *win = qobject_cast<KomportApp*>(w) ) {
      const KomportDoc *d = win->getDocument();
      if ( d && !documents.contains(d) )
        documents.append(d);
    }
  }
  return documents;
}
} // namespace

bool KomportDoc::isAnyOfflineSession() const
{
  for ( const KomportDoc *d : allOpenDocuments(this) )
    if ( d->hasOfflineSession() )
      return true;
  return false;
}

SessionLoadOutcome KomportDoc::loadOfflineSession(const QString &path)
{
  // SPEC-M10 5.7: loading is permitted only while the whole application is idle.
  // Order of the reasons: a live session, a running recording, a loaded session.
  const QList<const KomportDoc*> documents = allOpenDocuments(this);
  QString refusal;
  for ( const KomportDoc *d : documents ) {
    if ( d->mSessionController->state() != SessionController::State::Idle ) {
      refusal = tr("a live session is active - close it before loading a session for replay");
      break;
    }
  }
  if ( refusal.isEmpty() ) {
    for ( const KomportDoc *d : documents ) {
      if ( d->mSessionRecorder->state() != SessionRecorder::State::Stopped ) {
        refusal = tr("a recording is running - stop it before loading a session for replay");
        break;
      }
    }
  }
  if ( refusal.isEmpty() && isAnyOfflineSession() )
    refusal = tr("an offline session is already loaded - close it first");
  if ( !refusal.isEmpty() ) {
    // A refusal changes nothing: no close, open or configure of the transport, no
    // recording stopped, no loaded session discarded.
    SessionLoadOutcome refused;
    refused.path = path;
    refused.reason = refusal;
    return refused;
  }

  SessionLoadOutcome outcome = mSessionReader.load(path);
  if ( !outcome.ok || !outcome.session )
    return outcome;   // a load failure enters no offline state

  // The player shares the one immutable session; start() attaches it and positions
  // it at the first event without delivering anything (state Ready).
  auto player = std::make_unique<SessionReplayPlayer>();
  const SessionReplayStart started = player->start(outcome.session);
  if ( !started.ok ) {
    outcome.ok = false;
    outcome.reason = started.reason;
    outcome.session.reset();
    return outcome;
  }
  mOfflineSession = outcome.session;
  mReplayPlayer = std::move(player);
  emit offlineSessionChanged(true);
  return outcome;
}

void KomportDoc::closeOfflineSession()
{
  // Re-entrancy: player->close() emits stateChanged(Idle) synchronously, so a
  // receiver may call back into this function. The flag makes that inner call a
  // no-op, and the members stay set until the player's close() has returned, so a
  // load attempted from that receiver is still refused ("already loaded") and the
  // player is never destroyed inside its own close() frame.
  if ( mClosingOfflineSession || (!mOfflineSession && !mReplayPlayer) )
    return;
  // Lifetime: a receiver of the player's synchronous signals may also delete this
  // document or re-enter from inside a player call. The player is therefore taken
  // out of the member and only retired (closed, detached, deleted later), and
  // nothing of *this is touched afterwards unless the guard shows the document is
  // still alive.
  QPointer<KomportDoc> self(this);
  mClosingOfflineSession = true;
  // The session member stays set until the player is retired, so a load attempted
  // from a receiver is refused.
  retireReplayPlayer(std::move(mReplayPlayer), false);
  if ( !self )
    return;   // the document was deleted by a receiver
  mOfflineSession.reset();
  mClosingOfflineSession = false;
  // Announced last, with the document already in its final state: a receiver may
  // load another session from here, and that load must survive this call. Nothing
  // of *this is touched afterwards.
  emit offlineSessionChanged(false);
}

/** No descriptions */
void KomportDoc::slotViewModified(KomportView* _v){
    emit viewModified(_v);
}
