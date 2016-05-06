#include "../zm.h"
#include "zmHttpStream.h"

#include "zmHttp.h"
#include "zmHttpSession.h"
#include "../zmConnection.h"
#include "../zmFeedFrame.h"
#include "../encoders/zmJpegEncoder.h"

HttpStream::HttpStream( HttpSession *httpSession, Connection *connection, FeedProvider *provider, uint16_t width, uint16_t height, FrameRate frameRate, uint8_t quality ) :
    Stream( cClass(), stringtf( "%X", httpSession->session() ), connection, provider ),
    Thread( identity() ),
    mHttpSession( httpSession )
{
    Debug( 2, "New HTTP stream" );
    std::string encoderKey = JpegEncoder::getPoolKey( provider->identity(), width, height, frameRate, quality );
    if ( !(mEncoder = Encoder::getPooledEncoder( encoderKey )) )
    {
        JpegEncoder *jpegEncoder = new JpegEncoder( provider->identity(), width, height, frameRate, quality );
        jpegEncoder->registerProvider( *provider );
        Encoder::poolEncoder( jpegEncoder );
        jpegEncoder->start();
        mEncoder = jpegEncoder;
    }
    registerProvider( *mEncoder );
}

HttpStream::~HttpStream()
{
    deregisterProvider( *mEncoder );
}

bool HttpStream::sendFrame( Select::CommsList &writeable, FramePtr frame )
{
    const ByteBuffer &packet = frame->buffer();

    std::string txHeaders = "--imgboundary\r\n";
    txHeaders += "Content-Type: image/jpeg\r\n";
    txHeaders += stringtf( "Content-Length: %zd\r\n\r\n", packet.size() );

    ByteBuffer txBuffer;
    txBuffer.reserve( packet.size()+128 );
    txBuffer.append( txHeaders.c_str(), txHeaders.size() );
    txBuffer.append( packet.data(), packet.size() );
    txBuffer.append( "\r\n", 2 );

    for ( Select::CommsList::iterator iter = writeable.begin(); iter != writeable.end(); iter++ )
    {
        if ( TcpInetSocket *socket = dynamic_cast<TcpInetSocket *>(*iter) )
        {
            if ( socket == mConnection->socket() )
            {
                int nBytes = socket->write( txBuffer.data(), txBuffer.size() );
                Debug( 4, "Wrote %d bytes on sd %d", nBytes, socket->getWriteDesc() );
                if ( nBytes != txBuffer.size() )
                {
                    Error( "Incomplete write, %d bytes instead of %zd", nBytes, packet.size() );
                    mStop = true;
                    return( false );
                }
            }
        }
    }
    return( true );
}

int HttpStream::run()
{
    Select select( 60 );
    select.addWriter( mConnection->socket() );

    if ( !waitForProviders() )
        return( 1 );

    while ( !mStop && select.wait() >= 0 )
    {
        if ( mStop )
           break;
        Select::CommsList writeable = select.getWriteable();
        if ( writeable.size() <= 0 )
        {
            Error( "Writer timed out" );
            mStop = true;
            break;
        }
        mQueueMutex.lock();
        if ( !mFrameQueue.empty() )
        {
            for ( FrameQueue::iterator iter = mFrameQueue.begin(); iter != mFrameQueue.end(); iter++ )
            {
                sendFrame( writeable, *iter );
                //delete *iter;
            }
            mFrameQueue.clear();
        }
        mQueueMutex.unlock();
        usleep( INTERFRAME_TIMEOUT );
    }

    return( 0 );
}