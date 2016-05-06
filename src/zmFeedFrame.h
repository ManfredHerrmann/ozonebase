#ifndef ZM_FEED_FRAME_H
#define ZM_FEED_FRAME_H

#include "zmFeedBase.h"

#include "libimg/libimgImage.h"
extern "C" {
#include <libavformat/avformat.h>
}
#include <sys/time.h>

class FeedProvider;
class VideoProvider;
class AudioProvider;

///
/// Base class representing information passed between providers and consumers.
/// A 'frame' is a unit of information, normally representing a video frame, a 
/// series of audio samples or some other type of data.
/// Frames are never modified, rather new frames are derived from them and a reference
/// is made to the originating frame.
///
class FeedFrame
{
public:
    typedef enum { FRAME_TYPE_DATA, FRAME_TYPE_VIDEO, FRAME_TYPE_AUDIO } FeedType;     ///< Types of frame. More can be added here if necessary

private:
    FeedProvider   *mProvider;      ///< Pointer to provider of this frame
    FeedType       mFeedType;       ///< What type of frame this is 

protected:
    uint64_t        mId;            ///< Frame id, as a counting index
    uint64_t        mTimestamp;     ///< Frame timestamp in microseconds. Divide by 1000000LL to get Unix time
    ByteBuffer      mBuffer;        ///< Data buffer containing frame information
    FramePtr        mParent;        ///< Pointer to a parent frame if present. Parent frames are ones that this frame is derived from
                                    ///< and provide an audit trail of what has happened to the frame

protected:
    /// Create a new frame from a data buffer
    FeedFrame( FeedProvider *provider, FeedType mediaType, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer ) :
        mProvider( provider ),
        mFeedType( mediaType ),
        mId( id ),
        mTimestamp( timestamp ),
        mBuffer( buffer ),
        mParent( NULL )
    {
    }
    /// Create a new frame from a data pointer
    FeedFrame( FeedProvider *provider, FeedType mediaType, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size ) :
        mProvider( provider ),
        mFeedType( mediaType ),
        mId( id ),
        mTimestamp( timestamp ),
        mBuffer( buffer, size ),
        mParent( NULL )
    {
    }
    /// Create a new frame from a data buffer and relate it to a parent frame
    FeedFrame( FeedProvider *provider, FramePtr parent, FeedType mediaType, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer ) :
        mProvider( provider ),
        mFeedType( mediaType ),
        mId( id ),
        mTimestamp( timestamp ),
        mBuffer( buffer ),
        mParent( parent )
    {
    }
    /// Create a new frame from a data pointer and relate it to a parent frame
    FeedFrame( FeedProvider *provider, FramePtr parent, FeedType mediaType, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size ) :
        mProvider( provider ),
        mFeedType( mediaType ),
        mId( id ),
        mTimestamp( timestamp ),
        mBuffer( buffer, size ),
        mParent( parent )
    {
    }
    FeedFrame( FeedProvider *provider, FramePtr parent ) :
        mProvider( provider ),
        mFeedType( parent->mFeedType ),
        mId( parent->mId ),
        mTimestamp( parent->mTimestamp ),
        mBuffer( parent->mBuffer ),
        mParent( parent )
    {
    }
    /*
    FeedFrame( const FeedFrame &frame ) :
        mProvider( frame.mProvider ),
        mFeedType( frame.mFeedType ),
        mId( frame.mId ),
        mTimestamp( frame.mTimestamp ),
        mBuffer( frame.mBuffer ),
        mParent( frame.mParent )
    {
    }
    */
private:
    FeedFrame( const FeedFrame &frame );                            ///< No copying, only referencing allowed

public:
    virtual ~FeedFrame()
    {
        //if ( mParent )
            //delete mParent;
    }

    const FeedProvider *provider() const { return( mProvider ); }   ///< Return the provider of this frame
    const FeedProvider *originator() const                          /// Return the provider of the originating frame
    {
        //return( sourceFrame()->mProvider );
        const FeedProvider *source = mProvider;
        FramePtr parent( mParent );
        while ( parent.get() )
        {
            source = parent->mProvider;
            parent = parent->mParent;
        }
        return( source );
    }
    FeedType mediaType() const { return( mFeedType ); }
    uint64_t id() const { return( mId ); }
    uint64_t timestamp() const { return( mTimestamp ); }
    double age( uint64_t checkTimestamp=0 ) const                   /// How old this frame is (in seconds) relative to 'now' or the supplied timestamp
    {
        if ( !checkTimestamp )
        {
            struct timeval now;
            gettimeofday( &now, NULL );
            checkTimestamp = ((uint64_t)now.tv_sec*1000000LL)+now.tv_usec;
        }
        int64_t age = 0;
        age = checkTimestamp - mTimestamp;
        return( age/1000000.0L );
    }
    const ByteBuffer &buffer() const { return( mBuffer ); }
    //ByteBuffer &buffer() { return( mBuffer ); }
    //virtual const AVCodecContext *codecContext() const=0;

    FramePtr parentFrame() const { return( mParent ); }             ///< Return the parent frame (if present)
    FramePtr sourceFrame() const                                    /// Return the ultimate ancestor frame
    {
        const FeedFrame *source = this;
        while ( source->mParent.get() )
            source = source->mParent.get();
        return( FramePtr( source ) );
        //FramePtr source( this );
        //while ( source->mParent.get() )
            //source = source->mParent;
        //return( source );
    }
};

///
/// Class representing a video frame of a given size and format
///
class VideoFrame : public FeedFrame
{
protected:
    VideoProvider   *mVideoProvider;        ///< Specialised pointer to the video provider
    PixelFormat     mPixelFormat;           ///< The ffmpeg image format of this frame
    uint16_t        mWidth;                 ///< Frame image width (in pixels)
    uint16_t        mHeight;                ///< Frame image height (in pixels)

public:
    VideoFrame( VideoProvider *provider, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer );
    VideoFrame( VideoProvider *provider, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size );
    VideoFrame( VideoProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer );
    VideoFrame( VideoProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size );
    VideoFrame( VideoProvider *provider, FramePtr parent );

    const VideoProvider *videoProvider() const { return( mVideoProvider ); }
    PixelFormat pixelFormat() const { return( mPixelFormat ); }
    uint16_t width() const { return( mWidth ); }
    uint16_t height() const { return( mHeight ); }
};

///
/// Class representing an audio frame of a given sample rate, channels and number of samples
///
class AudioFrame : public FeedFrame
{
protected:
    AudioProvider   *mAudioProvider;        ///< Specialised pointer to the audio provider
    AVSampleFormat  mSampleFormat;          ///< The ffmpeg sample format of this frame
    uint32_t        mSampleRate;            ///< Sample rate (samples per second) of this frame
    uint8_t         mChannels;              ///< Number of audio channels
    uint16_t        mSamples;               ///< Number of samples present in this frame

public:
    AudioFrame( AudioProvider *provider, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer, uint16_t samples );
    AudioFrame( AudioProvider *provider, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size, uint16_t samples );
    AudioFrame( AudioProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer, uint16_t samples );
    AudioFrame( AudioProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size, uint16_t samples );
    AudioFrame( AudioProvider *provider, FramePtr parent );

    const AudioProvider *audioProvider() const { return( mAudioProvider ); }
    AVSampleFormat sampleFormat() const { return( mSampleFormat ); }
    uint32_t sampleRate() const { return( mSampleRate ); }
    uint8_t channels() const { return( mChannels ); }
    uint16_t samples() const { return( mSamples ); }
};

///
/// Class representing a data frame of arbitrary data
///
class DataFrame : public FeedFrame
{
public:
    DataFrame( FeedProvider *provider, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer );
    DataFrame( FeedProvider *provider, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size );
    DataFrame( FeedProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const ByteBuffer &buffer );
    DataFrame( FeedProvider *provider, FramePtr parent, uint64_t id, uint64_t timestamp, const uint8_t *buffer, size_t size );
    DataFrame( FeedProvider *provider, FramePtr parent );
};

#endif // ZM_FEED_FRAME_H