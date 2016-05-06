#include "../zm.h"
#include "zmLocalFileOutput.h"

#include "../zmFeedFrame.h"
#include "../zmFeedProvider.h"

int LocalFileOutput::run()
{
    if ( waitForProviders() )
    {
        while( !mStop )
        {
            mQueueMutex.lock();
            if ( !mFrameQueue.empty() )
            {
                for ( FrameQueue::iterator iter = mFrameQueue.begin(); iter != mFrameQueue.end(); iter++ )
                {
                    writeFrame( iter->get() );
                    //delete *iter;
                }
                mFrameQueue.clear();
            }
            mQueueMutex.unlock();
            checkProviders();
            usleep( INTERFRAME_TIMEOUT );
        }
    }
    cleanup();
    return( 0 );
}

bool LocalFileOutput::writeFrame( const FeedFrame *frame )
{
    std::string path = mLocation+"/";
    path = stringtf( "%simg%lld.jpg", path.c_str(), frame->id() );
    Info( "Path: %s", path.c_str() );
    const VideoFrame *videoFrame = dynamic_cast<const VideoFrame *>(frame);
    const VideoProvider *provider = dynamic_cast<const VideoProvider *>(frame->provider());
    Info( "PF:%d @ %dx%d", videoFrame->pixelFormat(), videoFrame->width(), videoFrame->height() );
    Image image( videoFrame->pixelFormat(), videoFrame->width(), videoFrame->height(), frame->buffer().data() );
    image.writeJpeg( path.c_str() );
    return( true );
}