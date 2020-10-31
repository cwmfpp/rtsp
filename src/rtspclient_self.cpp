
#include <pthread.h>
#include "rtspclient_self.h"

/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2020, Live Networks, Inc.  All rights reserved
// A demo application, showing how to create and run a RTSP client (that can potentially receive multiple streams concurrently).
//
// NOTE: This code - although it builds a running application - is intended only to illustrate how to develop your own RTSP
// client application.  For a full-featured RTSP client application - with much more functionality, and many options - see
// "openRTSP": http://www.live555.com/openRTSP/

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

// Forward function definitions:

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData, char const* reason);
  // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
RTSPClient* openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

void usage(UsageEnvironment& env, char const* progName) {
  env << "Usage: " << progName << " <rtsp-url-1> ... <rtsp-url-N>\n";
  env << "\t(where each <rtsp-url-i> is a \"rtsp://\" URL)\n";
}

char eventLoopWatchVariable = 0;

int main_rtspclient(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

  // We need at least one "rtsp://" URL argument:
  if (argc < 2) {
    usage(*env, argv[0]);
    return 1;
  }

  // There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
  for (int i = 1; i <= argc-1; ++i) {
#include <asm-generic/unistd.h>
      //__NR_gettid = 186
    *env << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << " " << argv[i]  << "\n";
    openURL(*env, argv[0], argv[i]);
  }

  // All subsequent activity takes place within the event loop:
  *env << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";
  env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
  *env << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";
    // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.

  return 0;

  // If you choose to continue the application past this point (i.e., if you comment out the "return 0;" statement above),
  // and if you don't intend to do anything more with the "TaskScheduler" and "UsageEnvironment" objects,
  // then you can also reclaim the (small) memory used by these objects by uncommenting the following code:
  /*
    env->reclaim(); env = NULL;
    delete scheduler; scheduler = NULL;
  */
}

// Define a class to hold per-stream state that we maintain throughout each stream's lifetime:

class StreamClientState {
public:
  StreamClientState();
  virtual ~StreamClientState();

public:
  MediaSubsessionIterator* iter;
  MediaSession* session;
  MediaSubsession* subsession;
  TaskToken streamTimerTask;
  double duration;
};

// If you're streaming just a single stream (i.e., just from a single URL, once), then you can define and use just a single
// "StreamClientState" structure, as a global variable in your application.  However, because - in this demo application - we're
// showing how to play multiple streams, concurrently, we can't do that.  Instead, we have to have a separate "StreamClientState"
// structure for each "RTSPClient".  To do this, we subclass "RTSPClient", and add a "StreamClientState" field to the subclass:

class ourRTSPClient: public RTSPClient {
public:
  static ourRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
                  int verbosityLevel = 0,
                  char const* applicationName = NULL,
                  portNumBits tunnelOverHTTPPortNum = 0);

protected:
  ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
        int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum);
    // called only by createNew();
  virtual ~ourRTSPClient();

public:
  StreamClientState scs;
  RTSPClient_CallBack* m_pRTSPClientCallBack;
  void *m_pvPri;
};

// Define a data sink (a subclass of "MediaSink") to receive the data for each subsession (i.e., each audio or video 'substream').
// In practice, this might be a class (or a chain of classes) that decodes and then renders the incoming audio or video.
// Or it might be a "FileSink", for outputting the received data into a file (as is done by the "openRTSP" application).
// In this example code, however, we define a simple 'dummy' sink that receives incoming data, but does nothing with it.

class DummySink: public MediaSink {
public:
  static DummySink* createNew(UsageEnvironment& env,
                  MediaSubsession& subsession, // identifies the kind of data that's being received
                  char const* streamId = NULL); // identifies the stream itself (optional)
  void printfHex(u_int8_t* data, int len);
  RTSPClient_CallBack* m_pRTSPClientCallBack;
  void *m_pvPri;

private:
  DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId);
    // called only by "createNew()"
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize,
                                unsigned numTruncatedBytes,
                struct timeval presentationTime,
                                unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
             struct timeval presentationTime, unsigned durationInMicroseconds);

private:
  // redefined virtual functions:
  virtual Boolean continuePlaying();

private:
  u_int8_t* fReceiveBuffer;
  MediaSubsession& fSubsession;
  char* fStreamId;
};

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"

static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.

RTSPClient* openURL(UsageEnvironment& env, char const* progName, char const* rtspURL) {
  // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
  // to receive (even if more than stream uses the same "rtsp://" URL).
  env << "chenwenmin pid" << getpid() << " "  << __func__ << " " << __LINE__ << " chenwenmin : " << rtspURL << "\n";
  RTSPClient* rtspClient = ourRTSPClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
  if (rtspClient == NULL) {
    env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
    return NULL;
  }

  ++rtspClientCount;

  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
  // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
  // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
  env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE);

  return rtspClient;
}


// Implementation of the RTSP 'response handlers':

void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
    if (resultCode != 0) {
      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
      delete[] resultString;
      break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.session == NULL) {
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
      break;
    } else if (!scs.session->hasSubsessions()) {
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void setupNextSubsession(RTSPClient* rtspClient) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL) {
    if (!scs.subsession->initiate()) {
      env << *rtspClient << "Failed to initiate the \"" << *scs.subsession << "\" subsession: " << env.getResultMsg() << "\n";
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    } else {
      env << *rtspClient << "Initiated the \"" << *scs.subsession << "\" subsession (";
      if (scs.subsession->rtcpIsMuxed()) {
    env << "client port " << scs.subsession->clientPortNum();
      } else {
    env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
      }
      env << ")\n";

      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
      env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
      rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
  }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias
    env << "chenwenmin pid" << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " " << __func__ << ":" <<__LINE__ << "\n";
    if (resultCode != 0) {
      env << *rtspClient << "Failed to set up the \"" << *scs.subsession << "\" subsession: " << resultString << "\n";
      break;
    }

    env << *rtspClient << "Set up the \"" << *scs.subsession << "\" subsession (";
    if (scs.subsession->rtcpIsMuxed()) {
      env << "client port " << scs.subsession->clientPortNum();
    } else {
      env << "client ports " << scs.subsession->clientPortNum() << "-" << scs.subsession->clientPortNum()+1;
    }
    env << ")\n";

    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
    // after we've sent a RTSP "PLAY" command.)

    scs.subsession->sink = DummySink::createNew(env, *scs.subsession, rtspClient->url());
      // perhaps use your own custom "MediaSink" subclass instead
    if (scs.subsession->sink == NULL) {
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.subsession
      << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    ((DummySink *)(scs.subsession->sink))->m_pRTSPClientCallBack = ((ourRTSPClient*)rtspClient)->m_pRTSPClientCallBack;
    ((DummySink *)(scs.subsession->sink))->m_pvPri = ((ourRTSPClient*)rtspClient)->m_pvPri;

    env << *rtspClient << "Created a data sink for the \"" << *scs.subsession << "\" subsession\n";
    scs.subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << "\n";
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, scs.subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
  Boolean success = False;

  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.duration > 0) {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << "chenwenmin pid " << getpid() << " "  << __func__ << ":" <<__LINE__ << " " << *rtspClient << "Started playing session";
    if (scs.duration > 0) {
      env << " (for up to " << scs.duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}


// Implementation of the other event handlers:

void subsessionAfterPlaying(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData, char const* reason) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << "chenwenmin pid" << getpid() << " "  << __func__ << ":" <<__LINE__ << " " << *rtspClient << "Received RTCP \"BYE\"";
  if (reason != NULL) {
    env << " (reason:\"" << reason << "\")";
    delete[] (char*)reason;
  }
  env << " on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  ourRTSPClient* rtspClient = (ourRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((ourRTSPClient*)rtspClient)->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != NULL) {
        env << "chenwenmin pid" << getpid() << " "  << __func__ << ":"<< __LINE__ << " iter " << ".\n";
      if (subsession->sink != NULL) {
    Medium::close(subsession->sink);
    subsession->sink = NULL;

    if (subsession->rtcpInstance() != NULL) {
      subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
    }

    someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  RTSPClient_CallBack* pRTSPClientCallBack = NULL;
  pRTSPClientCallBack = ((ourRTSPClient *)rtspClient)->m_pRTSPClientCallBack;
  if(NULL != pRTSPClientCallBack) {
       (*pRTSPClientCallBack)(RTSPC_CALLBACK_TYPE_SESSION_CLOSE, NULL, NULL, ((ourRTSPClient *)rtspClient)->m_pvPri);
  }
  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.
  env << "chenwenmin pid" << getpid() << " "  << __func__ << ":"<< __LINE__ << " rtspClientCount=" << rtspClientCount << ".\n";
  if (--rtspClientCount == 0) {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
    env << "chenwenmin pid" << getpid() << " "  << __func__ << ":"<< __LINE__ << " rtspClientCount=" << rtspClientCount << ".\n";
  }
}


// Implementation of "ourRTSPClient":

ourRTSPClient* ourRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL,
                    int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum) {
  return new ourRTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

ourRTSPClient::ourRTSPClient(UsageEnvironment& env, char const* rtspURL,
                 int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1) {
}

ourRTSPClient::~ourRTSPClient() {
}


// Implementation of "StreamClientState":

StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}


// Implementation of "DummySink":

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId) {
  return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env),
    fSubsession(subsession) {
  fStreamId = strDup(streamId);
  fReceiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink() {
  delete[] fReceiveBuffer;
  delete[] fStreamId;
}

#include <stdio.h>

void DummySink::printfHex(u_int8_t* data, int len)
{
#if 1
    int i = 0;

    for(i = 0; i < len; i++) {
        if(0 == (i % 8)) {
            envir() << "\n";
        }
        envir() << data[i] << ",";
    }
    envir() << "\n";
#else

    int i = 0;

    for(i = 0; i < len; i++) {
        if(0 == (i % 8)) {
            printf("\n");
        }
        printf("0x%2x, ", data[i]);
    }
    printf("\n");
#endif
}
void DummySink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned durationInMicroseconds) {
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void DummySink::afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes,
                  struct timeval presentationTime, unsigned /*durationInMicroseconds*/) {
  // We've just received a frame of data.  (Optionally) print out information about it:
#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
  if (fStreamId != NULL) envir() << "chenwenmin " << "pid " << getpid() << " " << "Stream \"" << fStreamId << "\"; ";
  envir() << fSubsession.mediumName() << "/" << fSubsession.codecName() << ":\tReceived " << frameSize << " bytes";
  if (numTruncatedBytes > 0) envir() << " (with " << numTruncatedBytes << " bytes truncated)";
  char uSecsStr[6+1]; // used to output the 'microseconds' part of the presentation time
  sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
  envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
  if (fSubsession.rtpSource() != NULL && !fSubsession.rtpSource()->hasBeenSynchronizedUsingRTCP()) {
    envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
  }
#ifdef DEBUG_PRINT_NPT
  envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
  envir() << "\n";
#endif
    if(NULL != m_pRTSPClientCallBack) {
        envir() << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";
        //(int _iType, RTSPClientAttr *_pstRTSPClientAttr, unsigned char *_pucData, void *_pvPri);
        RTSPClientAttr stRTSPClientAttr;
        envir() << "chenwenmin pid " << (int *)m_pRTSPClientCallBack << " "<< __func__ << ":" <<__LINE__ << "\n";
        fReceiveBuffer[0] = 0x00;
        fReceiveBuffer[1] = 0x00;
        fReceiveBuffer[2] = 0x00;
        fReceiveBuffer[3] = 0x01;
        stRTSPClientAttr.m_uiDataLen = 4 + frameSize;
        stRTSPClientAttr.m_uiTimestamp = fSubsession.getNormalPlayTime(presentationTime) * 1000;
        (*m_pRTSPClientCallBack)(RTSPC_CALLBACK_TYPE_MEDIA_DATA, &stRTSPClientAttr, fReceiveBuffer, m_pvPri);
    }
    printfHex(fReceiveBuffer, (frameSize > 32) ? 32 : frameSize);
  // Then continue, to request the next frame of data:
    envir() << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";
  continuePlaying();
}


Boolean DummySink::continuePlaying() {
  if (fSource == NULL) return False; // sanity check (should not happen)

  envir() << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";

  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(fReceiveBuffer + 4, DUMMY_SINK_RECEIVE_BUFFER_SIZE - 4,
                        afterGettingFrame, this,
                        onSourceClosure, this);
  return True;
}


TaskScheduler* RTSPClientSession::m_pscheduler = NULL;
UsageEnvironment* RTSPClientSession::m_penv = NULL;

RTSPClientSession::RTSPClientSession()
{
    m_pRTSPClient = NULL;
    m_pucReceiveFrame = NULL;
    m_pvPri = this;

    return;
}

RTSPClientSession::~RTSPClientSession()
{

    return;
}

static void *RTSPClientThread(void *)
{
    for(;;) {
        UsageEnvironment* env = RTSPClientSession::m_penv;
        // All subsequent activity takes place within the event loop:
        *env << "chenwenmin  " << __NR_gettid << " "<< __func__ << ":" <<__LINE__ << "\n";
        *env << "chenwenmin pid " << getpid() << " "  << "tid " << (int)syscall(__NR_gettid) << " "<< __func__ << ":" <<__LINE__ << "\n";
        env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
        // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.
    }

    return NULL;
}

int RTSPClientSession::RTSPClientSessionInit()
{
    // Begin by setting up our usage environment:
    if(NULL == RTSPClientSession::m_penv) {
        RTSPClientSession::m_pscheduler = BasicTaskScheduler::createNew();
        RTSPClientSession::m_penv = BasicUsageEnvironment::createNew(*(RTSPClientSession::m_pscheduler));

        pthread_t new_th;
        int ret;

        ret = pthread_create(&new_th, NULL, RTSPClientThread, NULL);
        if (ret != 0) {
            return -1;
        }
        pthread_detach(new_th);
    }

    return 0;
}

/*
int RTSPClientSession::RTSPClientSessionDispatch()
{
    pthread_t new_th;
    int ret;

    ret = pthread_create(&new_th, NULL, RTSPClientThread, NULL);
    if (ret != 0) {
        return -1;
    }
    pthread_detach(new_th);

    return 0;
}
*/

int RTSPClientSession::StartRTSPClientSession(RTSPClientInfo *_pRTSPClientInfo)
{
    if(NULL == _pRTSPClientInfo) {
        return -1;
    }

    if(NULL == _pRTSPClientInfo->m_pRTSPClientCallBack) {
        return -1;
    }

    UsageEnvironment* env = RTSPClientSession::m_penv;

    m_pRTSPClient = openURL(*env, "wenminchen@126.com", _pRTSPClientInfo->m_cRTSPUrl);
    ((ourRTSPClient*)m_pRTSPClient)->m_pRTSPClientCallBack = _pRTSPClientInfo->m_pRTSPClientCallBack;
    ((ourRTSPClient*)m_pRTSPClient)->m_pvPri = _pRTSPClientInfo->m_pvPri;

    //StreamClientState& scs = ((ourRTSPClient*)m_pRTSPClient)->scs; // alias

    return 0;
}


int RTSPClientSession::StopRTSPClientSession()
{
    shutdownStream(m_pRTSPClient, 1);

    return 0;
}



/*
void TestRTSPClientSession()
{
    RTSPClientSession::RTSPClientSessionInit();

    RTSPClientSession stRTSPClientSession;
    RTSPClientSession stRTSPClientSession2;
    int iPri = 0;

    RTSPClientInfo stRTSPClientInfo;
    snprintf(stRTSPClientInfo.m_cRTSPUrl, sizeof(stRTSPClientInfo.m_cRTSPUrl), "%s", "rtsp://192.168.128.30:8554/slamtv60.264");
    stRTSPClientInfo.m_pRTSPClientCallBack = TestRTSPClient_CallBack;
    stRTSPClientInfo.m_pvPri = (void *)iPri;
    stRTSPClientSession.StartRTSPClientSession(&stRTSPClientInfo);

    snprintf(stRTSPClientInfo.m_cRTSPUrl, sizeof(stRTSPClientInfo.m_cRTSPUrl), "%s", "rtsp://192.168.128.30:8554/slamtv61.264");
    stRTSPClientInfo.m_pRTSPClientCallBack = TestRTSPClient_CallBack;
    stRTSPClientSession2.StartRTSPClientSession(&stRTSPClientInfo);

    sleep(5);
    stRTSPClientSession.StopRTSPClientSession();
    sleep(5);
    stRTSPClientSession2.StopRTSPClientSession();

}
*/
